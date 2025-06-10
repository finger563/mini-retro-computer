#include "boot.hpp"
#include <lvgl.h>

Boot::Boot(const Config &config)
    : config_(config) {}
Boot::~Boot() { deinit(); }

void Boot::init(lv_obj_t *parent) {
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
  visible_ = true;
  fading_ = false;
}

void Boot::deinit() {
  if (container_) {
    lv_obj_del(container_);
    container_ = nullptr;
    label_ = nullptr;
  }
  lines_.clear();
  fading_ = false;
  visible_ = false;
}

void Boot::update() {
  // TODO: handle dynamic/animated lines
}

void Boot::add_line(const std::string &line) {
  lines_.push_back(line);
  std::string text;
  for (size_t i = 0; i < lines_.size(); ++i) {
    if (i > 0)
      text += "\n";
    text += lines_[i];
  }
  if (label_) {
    lv_label_set_text(label_, text.c_str());
    lv_obj_update_layout(container_);
    lv_obj_scroll_to_view(label_, LV_ANIM_OFF);
    lv_obj_scroll_to_y(container_, lv_obj_get_height(label_), LV_ANIM_OFF);
  }
}

void Boot::update_last_line(const std::string &line) {
  if (!lines_.empty()) {
    lines_.back() = line;
    std::string text;
    for (size_t i = 0; i < lines_.size(); ++i) {
      if (i > 0)
        text += "\n";
      text += lines_[i];
    }
    if (label_) {
      lv_label_set_text(label_, text.c_str());
      lv_obj_update_layout(container_);
      lv_obj_scroll_to_view(label_, LV_ANIM_OFF);
      lv_obj_scroll_to_y(container_, lv_obj_get_height(label_), LV_ANIM_OFF);
    }
  }
}

void Boot::start_fade_out() {
  if (!container_ || !label_ || fading_)
    return;
  fading_ = true;
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, label_);
  lv_anim_set_values(&a, LV_OPA_COVER, LV_OPA_TRANSP);
  lv_anim_set_time(&a, 400);
  lv_anim_set_exec_cb(&a,
                      [](void *obj, int32_t v) { lv_obj_set_style_opa((lv_obj_t *)obj, v, 0); });
  lv_anim_set_user_data(&a, this);
  lv_anim_set_ready_cb(&a, [](lv_anim_t *anim) {
    auto *self = static_cast<Boot *>(lv_anim_get_user_data(anim));
    if (self && self->container_) {
      lv_obj_del(self->container_);
      self->container_ = nullptr;
      self->label_ = nullptr;
    }
    // fading_ will be reset on next init
  });
  lv_anim_start(&a);
}

bool Boot::is_fading() const { return fading_; }
bool Boot::is_visible() const { return visible_; }
void Boot::set_visible(bool visible) {
  visible_ = visible;
  if (container_) {
    if (visible)
      lv_obj_clear_flag(container_, LV_OBJ_FLAG_HIDDEN);
    else
      lv_obj_add_flag(container_, LV_OBJ_FLAG_HIDDEN);
  }
}