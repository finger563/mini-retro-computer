#include "matrix_rain.hpp"
#include <algorithm>
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
  if (!parent) {
    parent_ = nullptr;
    return;
  }
  parent_ = parent;
  cols_ = config_.screen_width / config_.char_width;
  rows_ = config_.screen_height / config_.char_height;
  columns_.clear();
  for (int x = 0; x < cols_; ++x) {
    Column col;
    col.container = lv_obj_create(parent_);
    if (!col.container)
      continue;
    lv_obj_set_size(col.container, config_.char_width, config_.screen_height - 1);
    lv_obj_set_pos(col.container, x * config_.char_width, 0);
    lv_obj_set_scrollbar_mode(col.container, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_opa(col.container, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(col.container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_border_width(col.container, 0, 0);
    lv_obj_set_style_pad_all(col.container, 0, 0);
    lv_obj_set_style_pad_row(col.container, 0, 0);
    lv_obj_set_style_pad_column(col.container, 0, 0);
    // Create one label per row
    for (int y = 0; y < rows_; ++y) {
      CharCell cell;
      cell.label = lv_label_create(col.container);
      if (!cell.label)
        continue;
      lv_obj_set_size(cell.label, config_.char_width, config_.char_height);
      lv_obj_set_pos(cell.label, 0, y * config_.char_height);
      lv_label_set_long_mode(cell.label, LV_LABEL_LONG_CLIP);
      if (font_)
        lv_obj_set_style_text_font(cell.label, font_, 0);
      lv_label_set_text(cell.label, "");
      lv_obj_set_style_text_color(cell.label, lv_color_hex(0x000000), 0);
      lv_obj_set_style_bg_opa(cell.label, LV_OPA_TRANSP, 0);
      lv_obj_set_style_text_align(cell.label, LV_TEXT_ALIGN_CENTER, 0);
      cell.codepoint = 0;
      cell.fading = false;
      cell.fade_progress = 0.0f;
      cell.fade_start_time = 0;
      col.cells.push_back(cell);
    }
    col.drops.clear();
    // Randomize last_spawn_time to desynchronize columns
    col.last_spawn_time = lv_tick_get() + (rand() % config_.drop_spawn_interval_ms);
    columns_.push_back(std::move(col));
  }
  last_update_ = lv_tick_get();
}

void MatrixRain::deinit() {
  for (auto &col : columns_) {
    for (auto &cell : col.cells) {
      if (cell.label && lv_obj_is_valid(cell.label))
        lv_obj_del(cell.label);
      cell.label = nullptr;
    }
    if (col.container && lv_obj_is_valid(col.container))
      lv_obj_del(col.container);
    col.container = nullptr;
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
  for (auto &col : columns_) {
    // Possibly spawn a new drop
    if (now - col.last_spawn_time > (uint32_t)config_.drop_spawn_interval_ms) {
      if (rand() % 10 == 0) // randomize drop frequency
        spawn_drop(col, now);
      col.last_spawn_time = now;
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
  last_update_ = now;
}

void MatrixRain::spawn_drop(Column &col, uint32_t now) {
  Drop drop;
  drop.length =
      config_.min_drop_length + (rand() % (config_.max_drop_length - config_.min_drop_length + 1));
  drop.head_row = -1;
  drop.last_mutate_time = now;
  drop.last_advance_time = now;
  drop.active = true;
  drop.speed_ms = 30 + (rand() % 51);
  drop.chars.clear();
  for (int i = 0; i < drop.length; ++i)
    drop.chars.push_back(random_katakana());
  col.drops.push_back(drop);
}

void MatrixRain::update_drop(Column &col, Drop &drop, uint32_t now) {
  // Mutate head character rapidly
  if (now - drop.last_mutate_time > (uint32_t)config_.head_mutate_interval_ms) {
    drop.chars.back() = random_katakana();
    drop.last_mutate_time = now;
  }
  // Advance head
  if (now - drop.last_advance_time > (uint32_t)drop.speed_ms) {
    drop.head_row++;
    drop.last_advance_time = now;
    // If head is within visible area
    if (drop.head_row >= 0 && drop.head_row < (int)col.cells.size()) {
      // Set head cell
      auto &cell = col.cells[drop.head_row];
      cell.codepoint = drop.chars.back();
      cell.fading = false;
      cell.fade_progress = 0.0f;
      cell.fade_start_time = 0;
      char utf8[5] = {0};
      unicode_to_utf8(cell.codepoint, utf8);
      lv_label_set_text(cell.label, utf8);
      update_cell_style(cell, 0.0f, true);
    }
    // Previous head becomes tail
    if (drop.head_row - 1 >= 0 && drop.head_row - 1 < (int)col.cells.size()) {
      auto &cell = col.cells[drop.head_row - 1];
      cell.codepoint = drop.chars[drop.length - 2];
      cell.fading = true;
      cell.fade_start_time = now;
      cell.fade_progress = 0.0f;
      char utf8[5] = {0};
      unicode_to_utf8(cell.codepoint, utf8);
      lv_label_set_text(cell.label, utf8);
      update_cell_style(cell, 0.0f, false);
    }
    // Shift tail chars
    for (int i = drop.length - 2; i > 0; --i)
      drop.chars[i] = drop.chars[i - 1];
    drop.chars[0] = random_katakana();
    // If head is past the bottom, mark drop inactive
    if (drop.head_row >= (int)col.cells.size())
      drop.active = false;
  }
}

void MatrixRain::update_fade(Column &col, uint32_t now) {
  for (auto &cell : col.cells) {
    if (cell.fading) {
      float progress = (now - cell.fade_start_time) / (float)config_.fade_duration_ms;
      if (progress >= 1.0f) {
        // Fade complete, clear cell
        lv_label_set_text(cell.label, "");
        cell.fading = false;
        cell.fade_progress = 0.0f;
        cell.codepoint = 0;
      } else {
        cell.fade_progress = progress;
        update_cell_style(cell, progress, false);
      }
    }
  }
}

void MatrixRain::update_cell_style(CharCell &cell, float fade_progress, bool is_head) {
  // Head: bright green, not faded
  if (is_head) {
    lv_obj_set_style_text_color(cell.label, lv_color_hex(0xB6FF00), 0);
    lv_obj_set_style_opa(cell.label, LV_OPA_COVER, 0);
    return;
  }
  // Tail: fade from green to black
  uint8_t green = 0xFF * (1.0f - fade_progress);
  uint32_t color = (green << 8);
  lv_obj_set_style_text_color(cell.label, lv_color_hex(color), 0);
  lv_obj_set_style_opa(cell.label, (lv_opa_t)(LV_OPA_COVER * (1.0f - fade_progress)), 0);
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
