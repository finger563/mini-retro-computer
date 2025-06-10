#include "matrix_rain.hpp"
#include <cstdlib>
#include <cstring>
#include <ctime>

MatrixRain::MatrixRain(const Config &config)
    : config_(config)
    , update_interval_(config.update_interval_ms) {
  font_ = nullptr;
}

MatrixRain::~MatrixRain() { deinit(); }

void MatrixRain::set_font(const lv_font_t *font) {
  font_ = font;
  // Optionally update all labels' font
}

void MatrixRain::set_visible(bool visible) {
  for (auto &col : columns_) {
    if (col.container) {
      if (visible)
        lv_obj_clear_flag(col.container, LV_OBJ_FLAG_HIDDEN);
      else
        lv_obj_add_flag(col.container, LV_OBJ_FLAG_HIDDEN);
    }
  }
  if (prompt_label_) {
    if (visible)
      lv_obj_clear_flag(prompt_label_, LV_OBJ_FLAG_HIDDEN);
    else
      lv_obj_add_flag(prompt_label_, LV_OBJ_FLAG_HIDDEN);
  }
}

void MatrixRain::set_prompt(const char *text) {
  if (!parent_)
    return;
  if (!prompt_label_) {
    prompt_label_ = lv_label_create(parent_);
    lv_label_set_long_mode(prompt_label_, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(prompt_label_, config_.screen_width);
    lv_obj_set_style_text_color(prompt_label_, lv_color_hex(0x00FF00), 0);
    lv_obj_set_style_bg_opa(prompt_label_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_text_align(prompt_label_, LV_TEXT_ALIGN_LEFT, 0);
    if (font_)
      lv_obj_set_style_text_font(prompt_label_, font_, 0);
    lv_obj_set_pos(prompt_label_, 0, 0);
  }
  lv_label_set_text(prompt_label_, text);
  lv_obj_move_foreground(prompt_label_);
}

void MatrixRain::init(lv_obj_t *parent) {
  deinit();
  parent_ = parent;
  cols_ = config_.screen_width / config_.char_width;
  rows_ = config_.screen_height / config_.char_height;
  columns_.clear();
  for (int x = 0; x < cols_; ++x) {
    Column col;
    int col_length = config_.min_drop_length +
                     (rand() % (config_.max_drop_length - config_.min_drop_length + 1));
    col.container = lv_obj_create(parent_);
    lv_obj_set_size(col.container, config_.char_width, config_.screen_height);
    lv_obj_set_pos(col.container, x * config_.char_width, 0);
    lv_obj_set_scrollbar_mode(col.container, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_opa(col.container, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(col.container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_border_width(col.container, 0, 0);
    lv_obj_set_style_pad_all(col.container, 0, 0);
    lv_obj_set_style_pad_row(col.container, 0, 0);
    lv_obj_set_style_pad_column(col.container, 0, 0);
    // Tail label
    col.tail_label = lv_label_create(col.container);
    lv_obj_set_width(col.tail_label, config_.char_width);
    lv_obj_set_height(col.tail_label, (col_length - 1) * config_.char_height);
    lv_label_set_long_mode(col.tail_label, LV_LABEL_LONG_WRAP);
    if (font_)
      lv_obj_set_style_text_font(col.tail_label, font_, 0);
    lv_obj_set_style_text_color(col.tail_label, lv_color_hex(0x00FF00), 0);
    lv_obj_set_style_bg_opa(col.tail_label, LV_OPA_TRANSP, 0);
    lv_obj_set_style_text_align(col.tail_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(col.tail_label, 0, -col_length * config_.char_height);
    // Head label
    col.head_label = lv_label_create(col.container);
    lv_obj_set_width(col.head_label, config_.char_width);
    lv_obj_set_height(col.head_label, config_.char_height);
    lv_label_set_long_mode(col.head_label, LV_LABEL_LONG_WRAP);
    if (font_)
      lv_obj_set_style_text_font(col.head_label, font_, 0);
    lv_obj_set_style_text_color(col.head_label, lv_color_hex(0xB6FF00), 0);
    lv_obj_set_style_bg_opa(col.head_label, LV_OPA_TRANSP, 0);
    lv_obj_set_style_text_align(col.head_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(col.head_label, 0,
                   -col_length * config_.char_height + (col_length - 1) * config_.char_height);
    col.state = Column::State::WAITING;
    col.timer = lv_tick_get() + (rand() % 1200);
    col.chars.clear();
    for (int t = 0; t < col_length; ++t)
      col.chars.push_back(random_katakana());
    col.rain_speed = 1.0f + (rand() % 100) / 60.0f;
    columns_.push_back(col);
  }
  last_update_ = lv_tick_get();
}

void MatrixRain::deinit() {
  for (auto &col : columns_) {
    if (col.tail_label)
      lv_obj_del(col.tail_label);
    if (col.head_label)
      lv_obj_del(col.head_label);
    if (col.container)
      lv_obj_del(col.container);
  }
  columns_.clear();
  if (prompt_label_) {
    lv_obj_del(prompt_label_);
    prompt_label_ = nullptr;
  }
}

void MatrixRain::restart() {
  deinit();
  if (parent_)
    init(parent_);
}

void MatrixRain::update() {
  uint32_t now = lv_tick_get();
  for (auto &col : columns_) {
    update_column(col);
  }
}

void MatrixRain::start_column(Column &col) {
  int col_length =
      config_.min_drop_length + (rand() % (config_.max_drop_length - config_.min_drop_length + 1));
  lv_obj_set_height(col.tail_label, (col_length - 1) * config_.char_height);
  lv_obj_set_height(col.head_label, config_.char_height);
  col.chars.clear();
  for (int t = 0; t < col_length; ++t)
    col.chars.push_back(random_katakana());
  col.rain_speed = 1.0f + (rand() % 100) / 60.0f;
  lv_obj_set_pos(col.tail_label, 0, -col_length * config_.char_height);
  lv_obj_set_pos(col.head_label, 0,
                 -col_length * config_.char_height + (col_length - 1) * config_.char_height);
  // Animate tail_label
  int start_y = -col_length * config_.char_height;
  int end_y = config_.screen_height;
  int duration_ms = (int)((end_y - start_y) / col.rain_speed * (update_interval_));
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, col.tail_label);
  lv_anim_set_values(&a, start_y, end_y);
  lv_anim_set_time(&a, duration_ms);
  lv_anim_set_exec_cb(&a, set_label_y);
  lv_anim_set_ready_cb(&a, anim_ready_cb);
  lv_anim_set_user_data(&a, &col);
  lv_anim_start(&a);
  // Animate head_label
  lv_anim_t b;
  lv_anim_init(&b);
  lv_anim_set_var(&b, col.head_label);
  lv_anim_set_values(&b, start_y + (col_length - 1) * config_.char_height,
                     end_y + (col_length - 1) * config_.char_height);
  lv_anim_set_time(&b, duration_ms);
  lv_anim_set_exec_cb(&b, set_label_y);
  lv_anim_start(&b);
  col.timer = lv_tick_get();
}

void MatrixRain::update_column(Column &col) {
  uint32_t now = lv_tick_get();
  int col_length = col.chars.size();
  if (col.state == Column::State::WAITING) {
    if (now >= col.timer) {
      col.state = Column::State::RAINING;
      start_column(col);
    }
    return;
  }
  if (col.state == Column::State::RAINING) {
    // Randomize characters as the drop falls
    for (size_t t = 0; t < col.chars.size(); ++t) {
      if ((rand() % 4) == 0) {
        col.chars[t] = random_katakana();
      }
    }
    // Update tail label text (all but last char)
    std::string tail_text;
    for (size_t j = 0; j < col.chars.size() - 1; ++j) {
      char utf8[5] = {0};
      unicode_to_utf8(col.chars[j], utf8);
      tail_text += utf8;
      tail_text += "\n";
    }
    lv_label_set_text(col.tail_label, tail_text.c_str());
    // Update head label text (last char)
    char head_utf8[5] = {0};
    unicode_to_utf8(col.chars.back(), head_utf8);
    lv_label_set_text(col.head_label, head_utf8);
  }
}

uint32_t MatrixRain::random_katakana() { return 0x30A0 + (rand() % (0x30FF - 0x30A0 + 1)); }

void MatrixRain::unicode_to_utf8(uint32_t unicode, char *utf8) {
  if (unicode < 0x80)
    utf8[0] = unicode;
  else if (unicode < 0x800) {
    utf8[0] = 0xC0 | (unicode >> 6);
    utf8[1] = 0x80 | (unicode & 0x3F);
  } else if (unicode < 0x10000) {
    utf8[0] = 0xE0 | (unicode >> 12);
    utf8[1] = 0x80 | ((unicode >> 6) & 0x3F);
    utf8[2] = 0x80 | (unicode & 0x3F);
  } else {
    utf8[0] = 0xF0 | (unicode >> 18);
    utf8[1] = 0x80 | ((unicode >> 12) & 0x3F);
    utf8[2] = 0x80 | ((unicode >> 6) & 0x3F);
    utf8[3] = 0x80 | (unicode & 0x3F);
  }
}

void MatrixRain::set_label_y(void *label, int32_t v) { lv_obj_set_y((lv_obj_t *)label, v); }

void MatrixRain::anim_ready_cb(lv_anim_t *a) {
  auto *col = static_cast<Column *>(lv_anim_get_user_data(a));
  if (!col)
    return;
  // Set head to space when resetting
  if (!col->chars.empty())
    col->chars.back() = ' ';
  col->state = Column::State::WAITING;
  col->timer = lv_tick_get() + 200 + (rand() % 1200);
  int label_height = col->chars.size() * 8; // char_height
  lv_obj_set_y(col->tail_label, -label_height);
  lv_obj_set_y(col->head_label, -label_height + (col->chars.size() - 1) * 8);
}