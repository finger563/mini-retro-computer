#pragma once

#include <memory>
#include <mutex>
#include <optional>
#include <vector>

#include "base_component.hpp"
#include "display.hpp"
#include "high_resolution_timer.hpp"

class Gui : public espp::BaseComponent {
public:
  struct Config {
    espp::Logger::Verbosity log_level{espp::Logger::Verbosity::WARN};
    uint32_t boot_line_delay_ms{500};
    uint32_t terminal_duration_ms{2000};
    uint32_t matrix_rain_speed{1}; // Not used yet, but for future extension
    uint32_t timer_interval_ms{30};
  };

  explicit Gui(const Config &config)
      : BaseComponent("Gui", config.log_level)
      , timer_interval_ms_(config.timer_interval_ms)
      , boot_line_delay_ms_(config.boot_line_delay_ms)
      , terminal_duration_ms_(config.terminal_duration_ms)
      , matrix_rain_speed_(config.matrix_rain_speed) {
    init_ui();
    logger_.debug("Starting task...");
    // now start the gui updater task
    task_.periodic(timer_interval_ms_ * 1000);
    using namespace std::placeholders;
  }

  ~Gui() {
    task_.stop();
    deinit_ui();
  }

  void pause() {
    paused_ = true;
    task_.stop();
  }

  void resume() {
    task_.periodic(timer_interval_ms_ * 1000);
    paused_ = false;
  }

protected:
  enum class Mode { BOOT, TERMINAL, MATRIX_RAIN };

  void init_ui();
  void deinit_ui();

  void update();

  uint32_t timer_interval_ms_{30};

  // Boot/terminal/matrix rain state
  Mode mode_{Mode::BOOT};
  size_t boot_line_index_{0};
  std::vector<std::string> boot_lines_ = {"Retro Computer BIOS v1.03",
                                          "640K RAM SYSTEM",
                                          "Initializing devices...",
                                          "Detecting display: OK",
                                          "Detecting keyboard: OK",
                                          "Booting...",
                                          "READY."};
  uint32_t boot_line_delay_ms_{500};
  uint32_t last_boot_line_time_{0};
  uint32_t terminal_start_time_{0};
  uint32_t terminal_duration_ms_{2000};
  uint32_t matrix_rain_start_time_{0};

  // Matrix rain state
  struct MatrixRainColumn {
    lv_obj_t *container{nullptr};
    lv_obj_t *label{nullptr};
    enum class State { WAITING, RAINING } state{State::WAITING};
    uint32_t timer{0};
    float rain_speed{1.0f};
    std::vector<uint32_t> chars;
  };
  std::vector<MatrixRainColumn> matrix_rain_columns_;
  int matrix_char_height_{8};
  int matrix_char_width_{8};
  int matrix_cols_{0};
  int matrix_rows_{0};
  uint32_t matrix_rain_last_update_{0};
  uint32_t matrix_rain_update_interval_{40}; // ms, how often to animate rain
  int matrix_rain_head_glow_length_{2};      // number of spans for the head glow

  // LVGL object pointers
  lv_obj_t *boot_terminal_label_{nullptr};

  // LVGL styles
  lv_style_t style_green_text_;
  lv_style_t style_matrix_rain_;
  lv_style_t style_matrix_head_;
  bool style_matrix_rain_init_{false};
  bool style_matrix_head_init_{false};

  void draw_boot_screen();
  void draw_terminal();
  void draw_matrix_rain();
  void advance_matrix_rain();

  static void event_callback(lv_event_t *e) {
    lv_event_code_t event_code = lv_event_get_code(e);
    auto user_data = lv_event_get_user_data(e);
    auto gui = static_cast<Gui *>(user_data);
    if (!gui) {
      return;
    }
    switch (event_code) {
    case LV_EVENT_SHORT_CLICKED:
      break;
    case LV_EVENT_SCROLL:
      gui->on_scroll(e);
      break;
    case LV_EVENT_PRESSED:
    case LV_EVENT_CLICKED:
      gui->on_pressed(e);
      break;
    case LV_EVENT_VALUE_CHANGED:
      gui->on_value_changed(e);
      break;
    case LV_EVENT_LONG_PRESSED:
      break;
    case LV_EVENT_KEY:
      gui->on_key(e);
      break;
    default:
      break;
    }
  }

  void on_pressed(lv_event_t *e);
  void on_value_changed(lv_event_t *e);
  void on_key(lv_event_t *e);
  void on_scroll(lv_event_t *e);

  std::atomic<bool> paused_{false};
  espp::HighResolutionTimer task_{{
      .name = "Gui Task",
      .callback = std::bind(&Gui::update, this),
  }};
  std::recursive_mutex mutex_;

  uint32_t matrix_rain_speed_{1};

  // Test: single spangroup for smooth vertical scroll experiment
  lv_obj_t *test_spangroup_{nullptr};
  std::vector<lv_span_t *> test_spans_;
  int test_span_count_{16};
  float test_head_pos_{0.0f};     // head position in pixels
  float test_scroll_speed_{1.2f}; // pixels per update
  uint32_t test_last_scroll_{0};
  uint32_t test_scroll_interval_{16}; // ms (about 60 FPS)

  static uint32_t random_katakana();
  static void unicode_to_utf8(uint32_t unicode, char *utf8);
};
