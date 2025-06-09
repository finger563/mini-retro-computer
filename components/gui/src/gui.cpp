#include "gui.hpp"
#include <cstdlib>
#include <ctime>
#include <lvgl.h>

extern "C" {
extern const lv_font_t unscii_8_jp;
}

void Gui::deinit_ui() {
  logger_.info("Deinitializing UI");
  // Delete boot/terminal label if present
  if (boot_terminal_label_) {
    lv_obj_del(boot_terminal_label_);
    boot_terminal_label_ = nullptr;
  }
  // Delete matrix rain labels if present
  for (auto *lbl : matrix_rain_labels_) {
    lv_obj_del(lbl);
  }
  matrix_rain_labels_.clear();
}

void Gui::init_ui() {
  logger_.info("Initializing UI");
  // Set up green-on-black style
  lv_style_init(&style_green_text_);
  lv_style_set_text_color(&style_green_text_, lv_color_hex(0x00FF00));
  lv_style_set_bg_color(&style_green_text_, lv_color_hex(0x000000));
  lv_style_set_bg_opa(&style_green_text_, LV_OPA_COVER);
  lv_style_set_text_font(&style_green_text_, &unscii_8_jp);

  // Set background of main screen to black and hide scrollbars
  lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_opa(lv_screen_active(), LV_OPA_COVER, 0);
  lv_obj_clear_flag(lv_screen_active(), LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(lv_screen_active(), LV_OBJ_FLAG_SCROLL_CHAIN_HOR);
  lv_obj_clear_flag(lv_screen_active(), LV_OBJ_FLAG_SCROLL_CHAIN_VER);
  lv_obj_set_scrollbar_mode(lv_screen_active(), LV_SCROLLBAR_MODE_OFF);

  // Create the boot/terminal label
  boot_terminal_label_ = lv_label_create(lv_screen_active());
  lv_label_set_long_mode(boot_terminal_label_, LV_LABEL_LONG_WRAP);
  lv_obj_align(boot_terminal_label_, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_text_align(boot_terminal_label_, LV_TEXT_ALIGN_LEFT, 0);
  lv_obj_set_width(boot_terminal_label_, 128);  // full width
  lv_obj_set_height(boot_terminal_label_, 128); // full width
  lv_obj_add_style(boot_terminal_label_, &style_green_text_, 0);
  lv_obj_clear_flag(boot_terminal_label_, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(boot_terminal_label_, LV_SCROLLBAR_MODE_OFF);
}

void Gui::on_value_changed(lv_event_t *e) {
  lv_obj_t *target = (lv_obj_t *)lv_event_get_target(e);
  logger_.info("Value changed: {}", fmt::ptr(target));
}

void Gui::on_pressed(lv_event_t *e) {
  lv_obj_t *target = (lv_obj_t *)lv_event_get_target(e);
  logger_.info("PRESSED: {}", fmt::ptr(target));
}

void Gui::on_scroll(lv_event_t *e) {
  lv_obj_t *target = (lv_obj_t *)lv_event_get_target(e);
  logger_.info("SCROLL: {}", fmt::ptr(target));
}

void Gui::on_key(lv_event_t *e) {
  // print the key
  auto key = lv_indev_get_key(lv_indev_get_act());
  lv_obj_t *target = (lv_obj_t *)lv_event_get_target(e);
  logger_.info("KEY: {} on {}", key, fmt::ptr(target));
}

void Gui::update() {
  if (paused_)
    return;
  std::lock_guard<std::recursive_mutex> lk(mutex_);

  uint32_t now = lv_tick_get();

  switch (mode_) {
  case Mode::BOOT:
    draw_boot_screen();
    if (boot_line_index_ < boot_lines_.size()) {
      if (now - last_boot_line_time_ > boot_line_delay_ms_) {
        boot_line_index_++;
        last_boot_line_time_ = now;
      }
    } else {
      // Boot finished, go to terminal
      mode_ = Mode::TERMINAL;
      terminal_start_time_ = now;
    }
    break;
  case Mode::TERMINAL:
    draw_terminal();
    if (now - terminal_start_time_ > terminal_duration_ms_) {
      // Go to matrix rain
      mode_ = Mode::MATRIX_RAIN;
      matrix_rain_start_time_ = now;
      // Initialize matrix rain columns
      matrix_cols_ = 128 / matrix_char_width_;
      matrix_rows_ = 128 / matrix_char_height_;
      matrix_columns_.clear();
      for (int i = 0; i < matrix_cols_; ++i) {
        MatrixColumn col;
        int start_y = rand() % matrix_rows_;
        for (int t = 0; t < trail_length_; ++t) {
          MatrixChar mc;
          mc.y = (start_y - t + matrix_rows_) % matrix_rows_;
          mc.unicode = 0x30A0 + (rand() % (0x30FF - 0x30A0 + 1));
          col.trail.push_back(mc);
        }
        col.speed = 1 + rand() % 3;
        matrix_columns_.push_back(col);
      }
    }
    break;
  case Mode::MATRIX_RAIN:
    advance_matrix_rain();
    draw_matrix_rain();
    break;
  }

  lv_task_handler();
}

void Gui::draw_boot_screen() {
  if (!boot_terminal_label_)
    return;
  std::string text;
  for (size_t i = 0; i < boot_line_index_ && i < boot_lines_.size(); ++i) {
    text += boot_lines_[i] + "\n";
  }
  lv_label_set_text(boot_terminal_label_, text.c_str());
  // Scroll to bottom if needed
  lv_obj_scroll_to_y(boot_terminal_label_, lv_obj_get_height(boot_terminal_label_), LV_ANIM_OFF);
}

void Gui::draw_terminal() {
  if (!boot_terminal_label_)
    return;
  std::string text;
  for (const auto &line : boot_lines_) {
    text += line + "\n";
  }
  uint32_t now = lv_tick_get();
  bool show_cursor = ((now / 500) % 2) == 0;
  text += "> ";
  if (show_cursor)
    text += "_";
  lv_label_set_text(boot_terminal_label_, text.c_str());
  // Scroll to bottom if needed
  lv_obj_scroll_to_y(boot_terminal_label_, lv_obj_get_height(boot_terminal_label_), LV_ANIM_OFF);
}

void Gui::advance_matrix_rain() {
  // Move each column's drop down, randomize char (Japanese Katakana)
  for (auto &col : matrix_columns_) {
    if (col.trail.empty()) {
      // Initialize trail
      int start_y = rand() % matrix_rows_;
      for (int t = 0; t < trail_length_; ++t) {
        MatrixChar mc;
        mc.y = (start_y - t + matrix_rows_) % matrix_rows_;
        mc.unicode = 0x30A0 + (rand() % (0x30FF - 0x30A0 + 1));
        col.trail.push_back(mc);
      }
    } else {
      // Move trail down
      for (auto &mc : col.trail) {
        mc.y = (mc.y + 1) % matrix_rows_;
      }
      // Add new head
      MatrixChar head;
      head.y = (col.trail.front().y + 1) % matrix_rows_;
      head.unicode = 0x30A0 + (rand() % (0x30FF - 0x30A0 + 1));
      col.trail.insert(col.trail.begin(), head);
      // Trim trail
      if ((int)col.trail.size() > trail_length_)
        col.trail.pop_back();
    }
  }
}

void Gui::draw_matrix_rain() {
  // Remove boot/terminal label if present
  if (boot_terminal_label_) {
    lv_obj_del(boot_terminal_label_);
    boot_terminal_label_ = nullptr;
  }
  // Remove old rain labels
  for (auto *lbl : matrix_rain_labels_) {
    lv_obj_del(lbl);
  }
  matrix_rain_labels_.clear();
  // Initialize style if needed
  if (!style_matrix_rain_init_) {
    lv_style_init(&style_matrix_rain_);
    lv_style_set_text_color(&style_matrix_rain_, lv_color_hex(0x00FF00));
    lv_style_set_bg_opa(&style_matrix_rain_, LV_OPA_TRANSP);
    lv_style_set_text_font(&style_matrix_rain_, &unscii_8_jp);
    style_matrix_rain_init_ = true;
  }
  // Brighter style for head
  static lv_style_t style_matrix_head;
  static bool style_head_init = false;
  if (!style_head_init) {
    lv_style_init(&style_matrix_head);
    lv_style_set_text_color(&style_matrix_head, lv_color_hex(0xB6FF00));
    lv_style_set_bg_opa(&style_matrix_head, LV_OPA_TRANSP);
    lv_style_set_text_font(&style_matrix_head, &unscii_8_jp);
    style_head_init = true;
  }
  // Draw all trail chars for each column
  for (int x = 0; x < matrix_cols_; ++x) {
    const auto &col = matrix_columns_[x];
    for (size_t t = 0; t < col.trail.size(); ++t) {
      const auto &mc = col.trail[t];
      lv_obj_t *lbl = lv_label_create(lv_screen_active());
      char utf8[5] = {0};
      if (mc.unicode < 0x80) {
        utf8[0] = mc.unicode;
      } else if (mc.unicode < 0x800) {
        utf8[0] = 0xC0 | (mc.unicode >> 6);
        utf8[1] = 0x80 | (mc.unicode & 0x3F);
      } else if (mc.unicode < 0x10000) {
        utf8[0] = 0xE0 | (mc.unicode >> 12);
        utf8[1] = 0x80 | ((mc.unicode >> 6) & 0x3F);
        utf8[2] = 0x80 | (mc.unicode & 0x3F);
      } else {
        utf8[0] = 0xF0 | (mc.unicode >> 18);
        utf8[1] = 0x80 | ((mc.unicode >> 12) & 0x3F);
        utf8[2] = 0x80 | ((mc.unicode >> 6) & 0x3F);
        utf8[3] = 0x80 | (mc.unicode & 0x3F);
      }
      lv_label_set_text(lbl, utf8);
      lv_obj_set_pos(lbl, x * matrix_char_width_, mc.y * matrix_char_height_);
      if (t == 0) {
        lv_obj_add_style(lbl, &style_matrix_head, 0);
      } else {
        lv_obj_add_style(lbl, &style_matrix_rain_, 0);
      }
      matrix_rain_labels_.push_back(lbl);
    }
  }
}
