#include "terminal.hpp"
#include <lvgl.h>

Terminal::Terminal(const Config &config)
    : config_(config) {}
Terminal::~Terminal() { deinit(); }

void Terminal::init(lv_obj_t *parent) {
  deinit();
  container_ = lv_obj_create(parent);
  lv_obj_set_size(container_, config_.width, config_.height);
  lv_obj_set_style_bg_opa(container_, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(container_, 0, 0);
  lv_obj_set_style_pad_all(container_, 0, 0);
  lv_obj_set_pos(container_, 0, 0);
  lv_obj_set_scrollbar_mode(container_, LV_SCROLLBAR_MODE_OFF);
  label_ = lv_label_create(container_);
  lv_label_set_long_mode(label_, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(label_, config_.width);
  lv_obj_set_style_text_color(label_, lv_color_hex(0x00FF00), 0);
  lv_obj_set_style_bg_opa(label_, LV_OPA_TRANSP, 0);
  lv_obj_set_style_text_align(label_, LV_TEXT_ALIGN_LEFT, 0);
  if (config_.font)
    lv_obj_set_style_text_font(label_, config_.font, 0);
  lv_label_set_text(label_, "");
  cursor_visible_ = true;
  fading_ = false;
}

void Terminal::deinit() {
  if (container_) {
    lv_obj_del(container_);
    container_ = nullptr;
    label_ = nullptr;
  }
  prompt_.clear();
  typed_.clear();
  cursor_visible_ = true;
  fading_ = false;
}

void Terminal::update() {
  // TODO: handle typing animation and cursor blink
}

void Terminal::set_prompt(const std::string &prompt) {
  if (fading_)
    return;
  prompt_ = prompt;
  typed_.clear();
  if (label_)
    lv_label_set_text(label_, prompt_.c_str());
}

void Terminal::kb_type(char c) {
  if (fading_)
    return;
  typed_ += c;
  std::string text = prompt_ + typed_;
  if (cursor_visible_)
    text += "_";
  if (label_)
    lv_label_set_text(label_, text.c_str());
}

void Terminal::blink() {
  if (fading_)
    return;
  cursor_visible_ = !cursor_visible_;
  std::string text = prompt_ + typed_;
  if (cursor_visible_)
    text += "_";
  if (label_)
    lv_label_set_text(label_, text.c_str());
}

void Terminal::set_visible(bool visible) {
  if (container_) {
    if (visible)
      lv_obj_clear_flag(container_, LV_OBJ_FLAG_HIDDEN);
    else
      lv_obj_add_flag(container_, LV_OBJ_FLAG_HIDDEN);
  }
}

bool Terminal::is_visible() const {
  return container_ && !(lv_obj_has_flag(container_, LV_OBJ_FLAG_HIDDEN));
}

void Terminal::start_fade_out() {
  if (!container_ || fading_)
    return;
  fading_ = true;
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, container_);
  lv_anim_set_values(&a, LV_OPA_COVER, LV_OPA_TRANSP);
  lv_anim_set_time(&a, 400);
  lv_anim_set_exec_cb(&a,
                      [](void *obj, int32_t v) { lv_obj_set_style_opa((lv_obj_t *)obj, v, 0); });
  lv_anim_set_user_data(&a, this);
  lv_anim_set_ready_cb(&a, [](lv_anim_t *anim) {
    auto *self = static_cast<Terminal *>(lv_anim_get_user_data(anim));
    if (self && self->container_) {
      lv_obj_del(self->container_);
      self->container_ = nullptr;
      self->label_ = nullptr;
    }
    // fading_ will be reset on next init
  });
  lv_anim_start(&a);
}

bool Terminal::is_fading() const { return fading_; }