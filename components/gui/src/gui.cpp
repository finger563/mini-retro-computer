#include "gui.hpp"
#include <cstdlib>
#include <ctime>
#include <lvgl.h>

extern "C" {
extern const lv_font_t unscii_8_jp;
}

// Add helpers for Katakana and UTF-8
uint32_t Gui::random_katakana() { return 0x30A0 + (rand() % (0x30FF - 0x30A0 + 1)); }
void Gui::unicode_to_utf8(uint32_t unicode, char *utf8) {
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

void Gui::deinit_ui() {
  logger_.info("Deinitializing UI");
  // Delete boot/terminal label if present
  if (boot_terminal_label_) {
    lv_obj_del(boot_terminal_label_);
    boot_terminal_label_ = nullptr;
  }
}

void Gui::init_ui() {
  logger_.info("Initializing UI");
  // Remove any old test spangroup
  if (test_spangroup_) {
    lv_obj_del(test_spangroup_);
    test_spangroup_ = nullptr;
  }
  test_spans_.clear();

  // Remove any old matrix rain containers/labels
  for (auto &col : matrix_rain_columns_) {
    if (col.label)
      lv_obj_del(col.label);
    if (col.container)
      lv_obj_del(col.container);
  }
  matrix_rain_columns_.clear();

  // Set up green-on-black style
  lv_style_init(&style_green_text_);
  lv_style_set_text_color(&style_green_text_, lv_color_hex(0x00FF00));
  lv_style_set_bg_color(&style_green_text_, lv_color_hex(0x000000));
  lv_style_set_bg_opa(&style_green_text_, LV_OPA_COVER);
  lv_style_set_text_font(&style_green_text_, &unscii_8_jp);

  // Set up head style
  if (!style_matrix_head_init_) {
    lv_style_init(&style_matrix_head_);
    lv_style_set_text_color(&style_matrix_head_, lv_color_hex(0xB6FF00));
    lv_style_set_bg_opa(&style_matrix_head_, LV_OPA_TRANSP);
    lv_style_set_text_font(&style_matrix_head_, &unscii_8_jp);
    style_matrix_head_init_ = true;
  }

  // Set background of main screen to black and hide scrollbars
  lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_opa(lv_screen_active(), LV_OPA_COVER, 0);
  lv_obj_clear_flag(lv_screen_active(), LV_OBJ_FLAG_SCROLL_CHAIN_HOR);
  lv_obj_set_scrollbar_mode(lv_screen_active(), LV_SCROLLBAR_MODE_OFF);

  // Matrix rain: container+label per column
  matrix_cols_ = 128 / matrix_char_width_;
  int screen_h = 128;
  for (int x = 0; x < matrix_cols_; ++x) {
    MatrixRainColumn col;
    int col_length = 3 + (rand() % 2); // 3 or 4 chars per drop
    col.container = lv_obj_create(lv_screen_active());
    lv_obj_set_size(col.container, matrix_char_width_, screen_h);
    lv_obj_set_pos(col.container, x * matrix_char_width_, 0);
    lv_obj_set_scrollbar_mode(col.container, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_opa(col.container, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(col.container, LV_OBJ_FLAG_SCROLLABLE);
    // Remove border and padding
    lv_obj_set_style_border_width(col.container, 0, 0);
    lv_obj_set_style_pad_all(col.container, 0, 0);
    lv_obj_set_style_pad_row(col.container, 0, 0);
    lv_obj_set_style_pad_column(col.container, 0, 0);
    col.label = lv_label_create(col.container);
    lv_obj_set_width(col.label, matrix_char_width_);
    lv_obj_set_height(col.label, col_length * matrix_char_height_);
    lv_label_set_long_mode(col.label, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(col.label, &unscii_8_jp, 0);
    lv_obj_set_style_text_color(col.label, lv_color_hex(0x00FF00), 0);
    lv_obj_set_style_bg_opa(col.label, LV_OPA_TRANSP, 0);
    lv_obj_set_style_text_align(col.label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(col.label, 0, -col_length * matrix_char_height_); // start above
    col.state = MatrixRainColumn::State::WAITING;
    col.timer = lv_tick_get() + (rand() % 1200);
    col.chars.clear();
    for (int t = 0; t < col_length - 1; ++t)
      col.chars.push_back(random_katakana());
    col.chars.push_back(' '); // last char is space
    col.rain_speed = 1.0f + (rand() % 100) / 60.0f;
    matrix_rain_columns_.push_back(col);
  }
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
  int screen_h = 128;

  // TEST: Smooth scroll the test spangroup
  if (test_spangroup_) {
    if (now - test_last_scroll_ > test_scroll_interval_) {
      test_head_pos_ += test_scroll_speed_;
      lv_obj_scroll_to_y(test_spangroup_, (int)test_head_pos_, LV_ANIM_ON);
      // Update span colors for glow effect
      for (int i = 0; i < test_span_count_; ++i) {
        float span_y = i * matrix_char_height_;
        float dist = fabsf((test_head_pos_ + 64) - span_y); // +64 to center head in view
        lv_style_t *style = lv_span_get_style(test_spans_[i]);
        if (dist < matrix_char_height_ * 1.2f) {
          lv_style_set_text_color(style, lv_color_hex(0xB6FF00)); // head
        } else if (dist < matrix_char_height_ * 2.5f) {
          lv_style_set_text_color(style, lv_color_hex(0x55FF55)); // glow
        } else {
          lv_style_set_text_color(style, lv_color_hex(0x00FF00)); // tail
        }
      }
      // If head is off the bottom, reset
      if (test_head_pos_ > (test_span_count_ * matrix_char_height_ + 128)) {
        test_head_pos_ = 0.0f;
        lv_obj_scroll_to_y(test_spangroup_, 0, LV_ANIM_OFF);
        // Randomize chars
        for (auto *span : test_spans_) {
          uint32_t unicode = 0x30A0 + (rand() % (0x30FF - 0x30A0 + 1));
          char utf8[5] = {0};
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
          lv_span_set_text(span, utf8);
        }
      }
      test_last_scroll_ = now;
    }
  }

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
      // Remove boot/terminal label
      if (boot_terminal_label_) {
        lv_obj_del(boot_terminal_label_);
        boot_terminal_label_ = nullptr;
      }
    }
    break;
  case Mode::MATRIX_RAIN:
    static std::vector<float> progress;
    if (progress.size() != matrix_rain_columns_.size())
      progress.resize(matrix_rain_columns_.size(), 0.0f);
    for (size_t i = 0; i < matrix_rain_columns_.size(); ++i) {
      auto &col = matrix_rain_columns_[i];
      int col_length = col.chars.size();
      int label_height = col_length * matrix_char_height_;
      if (col.state == MatrixRainColumn::State::WAITING) {
        if (now >= col.timer) {
          col.state = MatrixRainColumn::State::RAINING;
          // Randomize chars and speed and col_length
          col.chars.clear();
          int new_col_length = 3 + (rand() % 2);
          lv_obj_set_height(col.label, new_col_length * matrix_char_height_);
          for (int t = 0; t < new_col_length - 1; ++t)
            col.chars.push_back(random_katakana());
          col.chars.push_back(' ');
          col.rain_speed = 1.0f + (rand() % 100) / 60.0f;
          progress[i] = 0.0f;
          lv_obj_set_pos(col.label, 0, -new_col_length * matrix_char_height_);
          col.timer = now;
        }
        continue;
      }
      if (col.state == MatrixRainColumn::State::RAINING) {
        progress[i] += col.rain_speed;
        // Randomize characters as the drop falls
        for (size_t t = 0; t < col.chars.size() - 1; ++t) {
          if ((rand() % 4) == 0) { // 25% chance to change per update
            col.chars[t] = random_katakana();
          }
        }
        // Update label text
        std::string text;
        for (auto ch : col.chars) {
          char utf8[5] = {0};
          unicode_to_utf8(ch, utf8);
          text += utf8;
          text += "\n";
        }
        lv_label_set_text(col.label, text.c_str());
        int y = -label_height + (int)progress[i];
        lv_obj_set_y(col.label, y);
        if (y >= screen_h) {
          col.state = MatrixRainColumn::State::WAITING;
          col.timer = now + 200 + (rand() % 1200);
          lv_obj_set_y(col.label, -label_height);
        }
      }
    }
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
