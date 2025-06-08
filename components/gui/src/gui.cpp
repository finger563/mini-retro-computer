#include "gui.hpp"
#include <cstdlib>
#include <ctime>
#include <lvgl.h>

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
  lv_obj_set_width(boot_terminal_label_, 128); // full width
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
        col.y = rand() % matrix_rows_;
        col.speed = 1 + rand() % 3;
        col.current_char = 'A' + (rand() % 26);
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
  // Move each column's drop down, randomize char
  for (auto &col : matrix_columns_) {
    if ((rand() % 3) < col.speed) {
      col.y = (col.y + 1) % matrix_rows_;
      col.current_char = 33 + (rand() % 94); // printable ASCII
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
    style_matrix_rain_init_ = true;
  }
  // Draw green chars on black background
  for (int x = 0; x < matrix_cols_; ++x) {
    const auto &col = matrix_columns_[x];
    lv_obj_t *lbl = lv_label_create(lv_screen_active());
    char c[2] = {col.current_char, 0};
    lv_label_set_text(lbl, c);
    lv_obj_set_pos(lbl, x * matrix_char_width_, col.y * matrix_char_height_);
    lv_obj_add_style(lbl, &style_matrix_rain_, 0);
    matrix_rain_labels_.push_back(lbl);
  }
}
