#pragma once

#include <deque>
#include <lvgl.h>
#include <string>
#include <vector>

#include "format.hpp"

class MatrixRain {
public:
  struct Config {
    int screen_width = 128;
    int screen_height = 128;
    int char_width = 8;
    int char_height = 8;
    int min_drop_length = 6;
    int max_drop_length = 16;
    int update_interval_ms = 40;
    int fade_duration_ms = 800;
    int head_mutate_interval_ms = 60;
    int drop_spawn_interval_ms = 400;
  };

  explicit MatrixRain(const Config &config);
  ~MatrixRain();

  void init(lv_obj_t *parent);
  void deinit();
  void update();
  void restart();
  void set_font(const lv_font_t *font);
  void set_visible(bool visible);
  void set_prompt(const char *text); // Show a label behind the rain (optional)

private:
  struct CharCell {
    uint32_t codepoint{0};
    uint32_t fade_start_time{0};
    bool fading{false};
    bool is_head{false};
  };

  struct Drop {
    int head_row{-1};
    int length{0};
    uint32_t last_mutate_time{0};
    uint32_t last_advance_time{0};
    bool active{true};
    int speed_ms{40};           // Per-drop speed in ms
    std::deque<uint32_t> chars; // head at back, tail at front
  };

  struct Column {
    std::vector<CharCell> cells; // one per row
    std::vector<Drop> drops;
    uint32_t last_spawn_time{0};
  };

  std::vector<Column> columns_;
  std::vector<lv_obj_t *> row_labels_;
  int cols_{0};
  int rows_{0};
  const lv_font_t *font_{nullptr};
  lv_obj_t *parent_{nullptr};
  lv_obj_t *prompt_label_{nullptr};
  Config config_;
  uint32_t last_update_{0};
  uint32_t update_interval_{40};

  void spawn_drop(Column &col, uint32_t now);
  void update_drop(Column &col, Drop &drop, uint32_t now);
  void update_fade(Column &col, uint32_t now);
  void update_row_labels(uint32_t now);
  static uint32_t random_katakana();
  static void unicode_to_utf8(uint32_t unicode, char *utf8);
};
