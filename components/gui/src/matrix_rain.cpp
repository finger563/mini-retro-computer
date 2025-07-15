#include "matrix_rain.hpp"
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fmt/core.h> // Added for fmt::format

MatrixRain::MatrixRain(const Config &config)
    : config_(config)
    , update_interval_(config.update_interval_ms) {
  font_ = nullptr;
  set_next_reveal_time();
}

MatrixRain::~MatrixRain() { deinit(); }

void MatrixRain::set_font(const lv_font_t *font) {
  font_ = font;
  for (auto &label : row_labels_) {
    if (label) {
      lv_obj_set_style_text_font(label, font_, 0);
    }
  }
  if (prompt_label_) {
    lv_obj_set_style_text_font(prompt_label_, font_, 0);
  }
}

void MatrixRain::set_visible(bool visible) {
  for (auto &label : row_labels_) {
    if (label) {
      if (visible)
        lv_obj_clear_flag(label, LV_OBJ_FLAG_HIDDEN);
      else
        lv_obj_add_flag(label, LV_OBJ_FLAG_HIDDEN);
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

void MatrixRain::debug_show_image() {
  if (!image_mode_ || image_brightness_map_.empty()) {
    fmt::print("Image mode is not enabled or brightness map is empty.\n");
    return;
  }

  static lv_obj_t *debug_img_label = lv_label_create(parent_);

  lv_label_set_long_mode(debug_img_label, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(debug_img_label, config_.screen_width);
  lv_obj_set_style_text_align(debug_img_label, LV_TEXT_ALIGN_LEFT, 0);
  if (font_) {
    lv_obj_set_style_text_font(debug_img_label, font_, 0);
  }
  lv_obj_move_foreground(debug_img_label);
  std::string debug_text;

  debug_text.reserve(cols_ * rows_ * 10); // Reserve enough space for the output
  for (int y = 0; y < rows_; ++y) {
    for (int x = 0; x < cols_; ++x) {
      uint8_t brightness = image_brightness_map_[y * cols_ + x];
      if (brightness < 10) {
        debug_text += " "; // Two spaces for very dark pixels
      } else {
        debug_text += fmt::format("#{0:02x}{0:02x}{0:02x} 0#", brightness);
      }
    }
    debug_text += "\n";
  }
  lv_label_set_recolor(debug_img_label, true);
  lv_label_set_text(debug_img_label, debug_text.c_str());
}

void MatrixRain::set_image(const lv_img_dsc_t *img) {
  if (!img) {
    image_mode_ = false;
    image_brightness_map_.clear();
    return;
  }

  image_mode_ = true;
  image_brightness_map_.assign(cols_ * rows_, 0);

  if (img->header.w == 0 || img->header.h == 0) {
    return;
  }

  // Use floating-point for more accurate scaling
  float x_scale = (float)img->header.w / cols_;
  float y_scale = (float)img->header.h / rows_;

  for (int y = 0; y < rows_; ++y) {
    for (int x = 0; x < cols_; ++x) {
      // Determine the source region in the image for this cell
      int src_x_start = x * x_scale;
      int src_x_end = (x + 1) * x_scale;
      int src_y_start = y * y_scale;
      int src_y_end = (y + 1) * y_scale;

      // Average the brightness over the source region
      uint32_t total_brightness = 0;
      int pixel_count = 0;
      for (int sy = src_y_start; sy < src_y_end; ++sy) {
        for (int sx = src_x_start; sx < src_x_end; ++sx) {
          total_brightness += get_pixel_brightness(img, sx, sy);
          pixel_count++;
        }
      }

      if (pixel_count > 0) {
        image_brightness_map_[y * cols_ + x] = total_brightness / pixel_count;
      }
    }
  }
}

void MatrixRain::print_image_brightness_map() {
  fmt::print("Image brightness map initialized: {} cells\n", image_brightness_map_.size());
  fmt::print("Image brightness map:\n");
  for (int y = 0; y < rows_; ++y) {
    for (int x = 0; x < cols_; ++x) {
      uint8_t brightness = image_brightness_map_[y * cols_ + x];
      fmt::print("{:02x} ", brightness);
    }
    fmt::print("\n");
  }
}

void MatrixRain::init(lv_obj_t *parent) {
  deinit();
  if (!parent) {
    parent_ = nullptr;
    return;
  }
  parent_ = parent;
  cols_ = config_.screen_width / config_.char_width;

  // disable scrolling on the parent
  lv_obj_set_scrollbar_mode(parent_, LV_SCROLLBAR_MODE_OFF);

  // Use the font's line height for layout, not the configured char_height
  const lv_font_t *font = font_ ? font_ : lv_obj_get_style_text_font(parent_, 0);
  int font_line_height = lv_font_get_line_height(font);
  if (font_line_height <= 0) {
    font_line_height = config_.char_height; // Fallback
  }
  rows_ = std::ceil(config_.screen_height / (float)font_line_height);

  // Init labels
  row_labels_.clear();
  row_labels_.reserve(rows_);
  std::string initial_row_text(cols_, ' ');
  for (int y = 0; y < rows_; ++y) {
    auto label = lv_label_create(parent_);
    if (!label)
      continue;
    lv_obj_set_size(label, config_.screen_width, font_line_height);
    lv_obj_set_pos(label, 0, y * font_line_height);
    lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
    lv_label_set_recolor(label, true);
    if (font_)
      lv_obj_set_style_text_font(label, font_, 0);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_style_bg_opa(label, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(label, 0, 0);
    lv_obj_set_style_pad_all(label, 0, 0);
    lv_label_set_text(label, initial_row_text.c_str());
    row_labels_.push_back(label);
  }

  // Init columns
  columns_.clear();
  columns_.reserve(cols_);
  for (int x = 0; x < cols_; ++x) {
    Column col;
    col.cells.resize(rows_);
    for (int y = 0; y < rows_; ++y) {
      col.cells[y] = CharCell{};
    }
    col.drops.clear();
    // Randomize last_spawn_time to desynchronize columns
    col.last_spawn_time = lv_tick_get() + (rand() % config_.drop_spawn_interval_ms);
    columns_.push_back(std::move(col));
  }
  last_update_ = lv_tick_get();
}

void MatrixRain::deinit() {
  for (auto &label : row_labels_) {
    if (label && lv_obj_is_valid(label))
      lv_obj_del(label);
  }
  row_labels_.clear();

  for (auto &col : columns_) {
    col.cells.clear();
    col.drops.clear();
  }
  columns_.clear();
  if (prompt_label_ && lv_obj_is_valid(prompt_label_)) {
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

  // State machine for image reveal
  if (image_mode_) {
    switch (image_state_) {
    case ImageRevealState::NORMAL:
      if (now >= state_transition_time_) {
        image_state_ = ImageRevealState::CLEARING;
      }
      break;
    case ImageRevealState::CLEARING:
      if (is_screen_clear()) {
        image_state_ = ImageRevealState::REVEALING;
        // Spawn all the image drops at once
        for (auto &col : columns_) {
          spawn_drop(col, now, true); // true for image drop
        }
        // Set duration for how long the image will be revealed
        uint32_t duration = config_.image_reveal_min_duration_ms +
                            (rand() % (config_.image_reveal_max_duration_ms -
                                       config_.image_reveal_min_duration_ms + 1));
        state_transition_time_ = now + duration;
      }
      break;
    case ImageRevealState::REVEALING:
      if (now >= state_transition_time_) {
        image_state_ = ImageRevealState::ERASING;
        // Set duration for the erasing animation
        state_transition_time_ = now + config_.image_erase_duration_ms;
      }
      break;
    case ImageRevealState::ERASING:
      if (now >= state_transition_time_) {
        image_state_ = ImageRevealState::NORMAL;
        set_next_reveal_time();
      }
      break;
    }
  }

  // Animation update logic
  for (auto &col : columns_) {
    // Clear previous drop characters that are not fading. This prepares the
    // column for the new state of the drops.
    for (auto &cell : col.cells) {
      if (!cell.fading) {
        cell.codepoint = 0;
      }
      cell.is_head = false;
    }

    // Possibly spawn a new drop
    bool should_spawn =
        (image_state_ == ImageRevealState::NORMAL || image_state_ == ImageRevealState::ERASING);
    if (should_spawn && now - col.last_spawn_time > (uint32_t)config_.drop_spawn_interval_ms) {
      // randomize drop frequency
      if (rand() % config_.drop_spawn_chance == 0) {
        spawn_drop(col, now);
      }
    }

    // Update all drops in this column
    for (auto &drop : col.drops) {
      if (drop.active)
        update_drop(col, drop, now);
    }
    // Remove inactive drops
    col.drops.erase(
        std::remove_if(col.drops.begin(), col.drops.end(), [](const Drop &d) { return !d.active; }),
        col.drops.end());
    // Update fading for all cells
    update_fade(col, now);
  }
  update_row_labels(now);
  last_update_ = now;
}

void MatrixRain::spawn_drop(Column &col, uint32_t now, bool is_image_drop) {
  Drop drop;
  drop.is_image_drop = is_image_drop;
  if (is_image_drop) {
    // Make drops extra long to ensure they cover the screen as they fall
    drop.length = rows_;
    drop.speed_ms = config_.image_drop_speed_ms;
    // Start drops at random negative positions to stagger their appearance
    drop.head_row = -(rand() % 4);
  } else {
    drop.length = config_.min_drop_length +
                  (rand() % (config_.max_drop_length - config_.min_drop_length + 1));
    drop.speed_ms = config_.min_speed_ms + (rand() % config_.speed_range_ms);
    drop.head_row = -1;
  }
  drop.last_mutate_time = now;
  drop.last_advance_time = now;
  drop.active = true;
  drop.chars.clear();
  for (int i = 0; i < drop.length; ++i)
    drop.chars.push_back(random_katakana());
  col.drops.push_back(drop);
}

void MatrixRain::update_drop(Column &col, Drop &drop, uint32_t now) {
  // Mutate head character rapidly (but not for static image drops)
  if (!drop.is_image_drop &&
      now - drop.last_mutate_time > (uint32_t)config_.head_mutate_interval_ms) {
    drop.chars.back() = random_katakana();
    drop.last_mutate_time = now;
  }

  // Advance head state if it's time
  if (now - drop.last_advance_time > (uint32_t)drop.speed_ms) {
    drop.head_row++;
    drop.last_advance_time = now;

    // Shift tail chars and add a new one
    drop.chars.pop_front();
    drop.chars.push_back(random_katakana());

    // If the whole drop is offscreen, mark inactive
    if (drop.head_row - drop.length >= (int)col.cells.size()) {
      drop.active = false;
    }
  }

  // Always paint the drop's current state into the cell buffer,
  // as long as it's active.
  if (drop.active) {
    for (int i = 0; i < drop.length; ++i) {
      int row = drop.head_row - i;
      if (row >= 0 && row < (int)col.cells.size()) {
        auto &cell = col.cells[row];
        cell.codepoint = drop.chars[drop.length - 1 - i];
        cell.is_head = (i == 0);

        if (cell.is_head) {
          cell.fading = false;
        } else {
          cell.fading = true;
          // Create a fake start time in the past to simulate a spatial gradient.
          // The further down the tail (higher i), the more faded the char is.
          uint32_t offset = (config_.fade_duration_ms / (config_.max_drop_length + 2)) * i;
          cell.fade_start_time = now - offset;
        }
      }
    }
  }
}

void MatrixRain::update_fade(Column &col, uint32_t now) {
  for (auto &cell : col.cells) {
    if (cell.fading) {
      float progress = (now - cell.fade_start_time) / (float)config_.fade_duration_ms;
      if (progress >= 1.0f) {
        // Fade complete, clear cell
        cell.fading = false;
        cell.codepoint = 0;
      }
    }
  }
}

void MatrixRain::update_row_labels(uint32_t now) {
  char utf8[5] = {0};
  for (int y = 0; y < rows_; ++y) {
    if (y >= row_labels_.size() || !row_labels_[y])
      continue;

    std::string text_buffer;
    text_buffer.reserve(cols_ * (10 + 5)); // Approx: #RRGGBB + utf8 + # + space

    for (int x = 0; x < cols_; ++x) {
      const auto &cell = columns_[x].cells[y];
      if (cell.codepoint == 0) {
        text_buffer += " ";
        continue;
      }

      unicode_to_utf8(cell.codepoint, utf8);

      if (cell.is_head) {
        if (image_mode_ && image_state_ == ImageRevealState::REVEALING) {
          uint8_t brightness = image_brightness_map_[y * cols_ + x];
          if (brightness < min_image_brightness_) {
            text_buffer += " "; // Use space for very dark pixels
            continue;
          }
        }
        text_buffer += fmt::format("#B6FF00 {}#", utf8);
      } else if (cell.fading) {
        float fade_duration = config_.fade_duration_ms;
        if (image_mode_ && image_state_ == ImageRevealState::REVEALING) {
          uint8_t brightness = image_brightness_map_[y * cols_ + x];
          // For dark pixels, make the character disappear almost instantly.
          if (brightness < min_image_brightness_) {
            text_buffer += " ";
            continue;
          }
          // For brighter pixels, make the fade duration longer.
          fade_duration = 1.0f + (config_.fade_duration_ms * (brightness / 255.0f)) * 5.0f;
        }

        float progress = (now - cell.fade_start_time) / fade_duration;
        progress = std::min(progress, 1.0f);

        // Once faded, render a space to be transparent.
        if (progress >= 1.0f) {
          text_buffer += " ";
        } else {
          uint8_t green = 0xFF * (1.0f - progress);
          text_buffer += fmt::format("#00{:02X}00 {}#", green, utf8);
        }
      } else {
        // This case is for the body of the drop, which is not the head and not fading yet.
        text_buffer += fmt::format("#00FF00 {}#", utf8);
      }
    }
    lv_label_set_text(row_labels_[y], text_buffer.c_str());
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

uint8_t MatrixRain::get_pixel_brightness(const lv_img_dsc_t *img, int x, int y) {
  if (x < 0 || x >= img->header.w || y < 0 || y >= img->header.h) {
    return 0;
  }

  // This function currently supports ARGB8888 and RGB565 (NATIVE).
  // To support more formats, add cases here.
  switch (img->header.cf) {
  case LV_COLOR_FORMAT_RGB565: {
    const uint16_t *data_u16 = (const uint16_t *)img->data;
    uint16_t pixel = data_u16[y * img->header.w + x];

    // Extract R, G, B components from RGB565
    uint8_t r5 = (pixel >> 11) & 0x1F;
    uint8_t g6 = (pixel >> 5) & 0x3F;
    uint8_t b5 = pixel & 0x1F;

    // Scale to 8-bit
    uint8_t r8 = (r5 << 3) | (r5 >> 2);
    uint8_t g8 = (g6 << 2) | (g6 >> 4);
    uint8_t b8 = (b5 << 3) | (b5 >> 2);

    // Calculate brightness (luma)
    return (r8 * 77 + g8 * 151 + b8 * 28) >> 8;
  }
  case LV_COLOR_FORMAT_ARGB8888: {
    const uint8_t *data_u8 = (const uint8_t *)img->data;
    // For little-endian systems (like ESP32), ARGB8888 is stored as BGRA in memory.
    uint32_t px_offset = (y * img->header.w + x) * 4;
    uint8_t b = data_u8[px_offset + 0];
    uint8_t g = data_u8[px_offset + 1];
    uint8_t r = data_u8[px_offset + 2];
    uint8_t a = data_u8[px_offset + 3];

    if (a == 0) {
      return 0;
    }

    // Standard RGB to luma conversion, weighted by alpha.
    uint32_t brightness = (r * 77 + g * 151 + b * 28) >> 8;
    return (brightness * a) / 255;
  }
  default:
    // Unsupported format, return 0 (black).
    return 0;
  }
}

bool MatrixRain::is_screen_clear() {
  for (const auto &col : columns_) {
    if (!col.drops.empty()) {
      return false;
    }
  }
  return true;
}

void MatrixRain::set_next_reveal_time() {
  uint32_t interval =
      config_.image_reveal_min_interval_ms +
      (rand() % (config_.image_reveal_max_interval_ms - config_.image_reveal_min_interval_ms + 1));
  state_transition_time_ = lv_tick_get() + interval;
}
