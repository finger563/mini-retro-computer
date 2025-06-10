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
  for (auto *lbl : boot_terminal_labels_) {
    if (lbl)
      lv_obj_del(lbl);
  }
  boot_terminal_labels_.clear();
  if (boot_terminal_container_) {
    lv_obj_del(boot_terminal_container_);
    boot_terminal_container_ = nullptr;
  }
}

void Gui::init_ui() {
  logger_.info("Initializing UI");
  // Remove any old matrix rain containers/labels
  for (auto &col : matrix_rain_columns_) {
    if (col.tail_label)
      lv_obj_del(col.tail_label);
    if (col.head_label)
      lv_obj_del(col.head_label);
    if (col.container)
      lv_obj_del(col.container);
  }
  matrix_rain_columns_.clear();

  // Remove any old boot/terminal container/labels
  for (auto *lbl : boot_terminal_labels_) {
    if (lbl)
      lv_obj_del(lbl);
  }
  boot_terminal_labels_.clear();
  if (boot_terminal_container_) {
    lv_obj_del(boot_terminal_container_);
    boot_terminal_container_ = nullptr;
  }

  // Set up green-on-black style
  lv_style_init(&style_green_text_);
  lv_style_set_text_color(&style_green_text_, lv_color_hex(0x00FF00));
  lv_style_set_bg_color(&style_green_text_, lv_color_hex(0x000000));
  lv_style_set_bg_opa(&style_green_text_, LV_OPA_COVER);
  lv_style_set_text_font(&style_green_text_, &unscii_8_jp);

  // Set background of main screen to black and hide scrollbars
  lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_opa(lv_screen_active(), LV_OPA_COVER, 0);
  lv_obj_clear_flag(lv_screen_active(), LV_OBJ_FLAG_SCROLL_CHAIN_HOR);
  lv_obj_set_scrollbar_mode(lv_screen_active(), LV_SCROLLBAR_MODE_OFF);

  // Create scrollable container for boot/terminal display
  boot_terminal_container_ = lv_obj_create(lv_screen_active());
  lv_obj_set_width(boot_terminal_container_, 128);
  lv_obj_set_scrollbar_mode(boot_terminal_container_, LV_SCROLLBAR_MODE_OFF);
  lv_obj_set_style_bg_opa(boot_terminal_container_, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(boot_terminal_container_, 0, 0);
  lv_obj_set_style_pad_all(boot_terminal_container_, 0, 0);
  lv_obj_set_pos(boot_terminal_container_, 0, 0);
  lv_obj_set_scroll_dir(boot_terminal_container_, LV_DIR_VER);
  lv_obj_add_flag(boot_terminal_container_, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_flex_flow(boot_terminal_container_, LV_FLEX_FLOW_COLUMN);

  // Matrix rain: container+label per column
  matrix_cols_ = 128 / matrix_char_width_;
  int screen_h = 128;
  for (int x = 0; x < matrix_cols_; ++x) {
    MatrixRainColumn col;
    int col_length = matrix_rain_num_chars_ + (rand() % matrix_rain_num_chars_ / 2);
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
    // Create tail label (all but last char)
    col.tail_label = lv_label_create(col.container);
    lv_obj_set_width(col.tail_label, matrix_char_width_);
    lv_obj_set_height(col.tail_label, (col_length - 1) * matrix_char_height_);
    lv_label_set_long_mode(col.tail_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(col.tail_label, &unscii_8_jp, 0);
    lv_obj_set_style_text_color(col.tail_label, lv_color_hex(0x00FF00), 0);
    lv_obj_set_style_bg_opa(col.tail_label, LV_OPA_TRANSP, 0);
    lv_obj_set_style_text_align(col.tail_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(col.tail_label, 0, -col_length * matrix_char_height_); // start above
    // Create head label (last char)
    col.head_label = lv_label_create(col.container);
    lv_obj_set_width(col.head_label, matrix_char_width_);
    lv_obj_set_height(col.head_label, matrix_char_height_);
    lv_label_set_long_mode(col.head_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(col.head_label, &unscii_8_jp, 0);
    lv_obj_set_style_text_color(col.head_label, lv_color_hex(0xB6FF00), 0); // bright green
    lv_obj_set_style_bg_opa(col.head_label, LV_OPA_TRANSP, 0);
    lv_obj_set_style_text_align(col.head_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(col.head_label, 0,
                   -col_length * matrix_char_height_ + (col_length - 1) * matrix_char_height_);
    col.state = MatrixRainColumn::State::WAITING;
    col.timer = lv_tick_get() + (rand() % 1200);
    col.chars.clear();
    for (int t = 0; t < col_length; ++t)
      col.chars.push_back(random_katakana());
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

// Animation callback for y-position
static void set_label_y(void *label, int32_t v) { lv_obj_set_y((lv_obj_t *)label, v); }

// Animation ready callback to reset the drop
void Gui::matrix_rain_anim_ready_cb(lv_anim_t *a) {
  // The user_data is a pointer to the MatrixRainColumn
  auto *col = static_cast<Gui::MatrixRainColumn *>(a->user_data);
  // Set head to space when resetting
  col->chars.back() = ' ';
  col->state = Gui::MatrixRainColumn::State::WAITING;
  col->timer = lv_tick_get() + 200 + (rand() % 1200);
  // Reset label positions
  int label_height = col->chars.size() * 8; // matrix_char_height_
  lv_obj_set_y(col->tail_label, -label_height);
  lv_obj_set_y(col->head_label, -label_height + (col->chars.size() - 1) * 8);
}

void Gui::update() {
  if (paused_)
    return;
  std::lock_guard<std::recursive_mutex> lk(mutex_);

  uint32_t now = lv_tick_get();
  int screen_h = 128;

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
      terminal_prompt_chars_shown_ = 0;
    }
    break;
  case Mode::TERMINAL:
    draw_terminal();
    // Animate typing the terminal prompt
    if (terminal_prompt_chars_shown_ < terminal_prompt_.size()) {
      static uint32_t last_char_time = 0;
      if (now - last_char_time > 60) { // 60ms per char
        terminal_prompt_chars_shown_++;
        last_char_time = now;
      }
    } else {
      // After a short pause, go to matrix rain
      if (now - terminal_start_time_ > terminal_duration_ms_ + 1000) {
        mode_ = Mode::MATRIX_RAIN;
        matrix_rain_start_time_ = now;
        // Remove boot/terminal label
        if (boot_terminal_container_) {
          lv_obj_del(boot_terminal_container_);
          boot_terminal_container_ = nullptr;
        }
        // TODO: Show terminal prompt text at the top of the Matrix rain effect
      }
    }
    break;
  case Mode::MATRIX_RAIN:
    for (size_t i = 0; i < matrix_rain_columns_.size(); ++i) {
      auto &col = matrix_rain_columns_[i];
      int col_length = col.chars.size();
      int label_height = col_length * matrix_char_height_;
      if (col.state == MatrixRainColumn::State::WAITING) {
        if (now >= col.timer) {
          col.state = MatrixRainColumn::State::RAINING;
          // Randomize chars, speed, and col_length
          col.chars.clear();
          int new_col_length = matrix_rain_num_chars_ + (rand() % matrix_rain_num_chars_ / 2);
          lv_obj_set_height(col.tail_label, (new_col_length - 1) * matrix_char_height_);
          lv_obj_set_height(col.head_label, matrix_char_height_);
          for (int t = 0; t < new_col_length; ++t)
            col.chars.push_back(random_katakana());
          col.rain_speed = 1.0f + (rand() % 100) / 60.0f;
          lv_obj_set_pos(col.tail_label, 0, -new_col_length * matrix_char_height_);
          lv_obj_set_pos(col.head_label, 0,
                         -new_col_length * matrix_char_height_ +
                             (new_col_length - 1) * matrix_char_height_);
          // Start LVGL animation for both labels
          int start_y = -new_col_length * matrix_char_height_;
          int end_y = screen_h;
          int duration_ms =
              (int)((end_y - start_y) / col.rain_speed * (matrix_rain_update_interval_));
          // Animate tail_label
          lv_anim_t a;
          lv_anim_init(&a);
          lv_anim_set_var(&a, col.tail_label);
          lv_anim_set_values(&a, start_y, end_y);
          lv_anim_set_time(&a, duration_ms);
          lv_anim_set_exec_cb(&a, set_label_y);
          lv_anim_set_ready_cb(&a, matrix_rain_anim_ready_cb);
          lv_anim_set_user_data(&a, &col);
          lv_anim_start(&a);
          // Animate head_label
          lv_anim_t b;
          lv_anim_init(&b);
          lv_anim_set_var(&b, col.head_label);
          lv_anim_set_values(&b, start_y + (new_col_length - 1) * matrix_char_height_,
                             end_y + (new_col_length - 1) * matrix_char_height_);
          lv_anim_set_time(&b, duration_ms);
          lv_anim_set_exec_cb(&b, set_label_y);
          lv_anim_start(&b);
          col.timer = now;
        }
        continue;
      }
      if (col.state == MatrixRainColumn::State::RAINING) {
        // Only randomize characters as the drop falls
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
    break;
  }

  lv_task_handler();
}

void Gui::draw_boot_screen() {
  if (!boot_terminal_container_)
    return;
  if (boot_terminal_labels_.empty()) {
    // make a label for the boot message
    auto *lbl = lv_label_create(boot_terminal_container_);
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(lbl, 128);
    // size to content
    lv_obj_set_height(lbl, LV_SIZE_CONTENT);
    lv_obj_set_style_text_font(lbl, &unscii_8_jp, 0);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0x00FF00), 0);
    lv_obj_set_style_bg_opa(lbl, LV_OPA_TRANSP, 0);
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_LEFT, 0);
    boot_terminal_labels_.push_back(lbl);
  }
  // now add the line at boot_line_index_ to the label
  if (boot_line_index_ >= boot_lines_.size()) {
    // No more lines to show
    return;
  }
  static int last_boot_index = -1;
  if (boot_line_index_ == last_boot_index) {
    // No change, nothing to do
    return;
  }
  std::string boot_text = "";
  for (size_t i = 0; i < boot_line_index_; ++i) {
    boot_text += "\n" + boot_lines_[i];
  }
  // Update the label text
  auto *lbl = boot_terminal_labels_.back();
  lv_label_set_text(lbl, boot_text.c_str());
  // Update layout and scroll to this label to ensure it's visible
  lv_obj_update_layout(boot_terminal_container_);
  lv_obj_scroll_to_view(lbl, LV_ANIM_OFF);
  // Scroll to the bottom of the container
  lv_obj_scroll_to_y(boot_terminal_container_, lv_obj_get_height(lbl), LV_ANIM_OFF);
  last_boot_index = boot_line_index_;
}

void Gui::draw_terminal() {
  if (!boot_terminal_container_)
    return;
  // Add the animated terminal prompt as a label
  if (terminal_prompt_chars_shown_ == 0) {
    // add the label the first time
    auto *lbl = lv_label_create(boot_terminal_container_);
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(lbl, 128);
    lv_obj_set_style_text_font(lbl, &unscii_8_jp, 0);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0x00FF00), 0);
    lv_obj_set_style_bg_opa(lbl, LV_OPA_TRANSP, 0);
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_LEFT, 0);
    boot_terminal_labels_.push_back(lbl);
  }
  std::string prompt = terminal_prompt_.substr(0, terminal_prompt_chars_shown_);
  if (!prompt.empty()) {
    auto *lbl = boot_terminal_labels_.back();
    lv_label_set_text(lbl, prompt.c_str());
    lv_obj_update_layout(boot_terminal_container_);
    lv_obj_scroll_to_view(lbl, LV_ANIM_OFF);
  }
}
