#pragma once

#include <string>
#include <vector>

#include <lvgl.h>

class MatrixRain {
public:
  struct Config {
    int screen_width = 128;
    int screen_height = 128;
    int char_width = 8;
    int char_height = 8;
    int min_drop_length = 3;
    int max_drop_length = 6;
    int update_interval_ms = 40;
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
  struct Column {
    lv_obj_t *container{nullptr};
    lv_obj_t *tail_label{nullptr};
    lv_obj_t *head_label{nullptr};
    enum class State { WAITING, RAINING } state{State::WAITING};
    uint32_t timer{0};
    float rain_speed{1.0f};
    std::vector<uint32_t> chars;
  };
  std::vector<Column> columns_;
  int cols_{0};
  int rows_{0};
  const lv_font_t *font_{nullptr};
  lv_obj_t *parent_{nullptr};
  lv_obj_t *prompt_label_{nullptr};
  Config config_;
  uint32_t last_update_{0};
  uint32_t update_interval_{40};

  void start_column(Column &col);
  void update_column(Column &col);
  static uint32_t random_katakana();
  static void unicode_to_utf8(uint32_t unicode, char *utf8);
  static void set_label_y(void *label, int32_t v);
  static void anim_ready_cb(lv_anim_t *a);
};
