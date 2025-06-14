#pragma once

#include <memory>
#include <mutex>
#include <optional>
#include <vector>

#include "base_component.hpp"
#include "boot.hpp"
#include "display.hpp"
#include "high_resolution_timer.hpp"
#include "matrix_rain.hpp"
#include "terminal.hpp"

class Gui : public espp::BaseComponent {
public:
  struct Config {
    espp::Logger::Verbosity log_level{espp::Logger::Verbosity::WARN};
    uint32_t boot_line_delay_ms{250};
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

  void restart();

protected:
  enum class Mode { BOOT, TERMINAL, MATRIX_RAIN };

  void init_ui();
  void deinit_ui();

  void update();

  uint32_t timer_interval_ms_{30};

  // Boot/terminal/matrix rain state
  Mode mode_{Mode::BOOT};
  size_t boot_line_index_{0};
  std::vector<std::string> boot_lines_ = {
      "Retro Computer BIOS v1.03",
      "640K RAM SYSTEM",
      "Phoenix Systems Ltd.",
      "Copyright 1988-1999",
      "CPU = 8086",
      "RAM = 640K",
      "Video BIOS shadowed",
      "UMB upper memory initialized",
      "Checking memory: {MEM} KB",
      "Initializing devices...",
      "Primary master disk: 20MB ST-225",
      "Primary slave disk: None",
      "Secondary master disk: None",
      "Secondary slave disk: None",
      "Floppy drive A: 1.44MB 3.5in",
      "Floppy drive B: None",
      "Serial port(s): COM1 COM2",
      "Parallel port(s): LPT1",
      "Detecting display: OK",
      "Detecting keyboard: OK",
      "\nREADY.",
  };
  std::string terminal_prompt_ = "> wake up, Neo...\n> the Matrix has you...\n> follow the white "
                                 "rabbit.\n> knock, knock, Neo.";
  size_t terminal_prompt_chars_shown_{0};
  uint32_t boot_line_delay_ms_{100};
  uint32_t last_boot_line_time_{0};
  uint32_t terminal_start_time_{0};
  uint32_t terminal_duration_ms_{2000};
  uint32_t matrix_rain_start_time_{0};
  uint32_t last_char_time_{0};

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

  static void matrix_rain_anim_ready_cb(lv_anim_t *a);

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
  uint8_t matrix_rain_num_chars_{5};

  std::unique_ptr<MatrixRain> matrix_rain_;

  // Character cell size for MatrixRain config
  int matrix_char_width_{8};
  int matrix_char_height_{8};

  std::unique_ptr<Boot> boot_;
  std::unique_ptr<Terminal> terminal_;
};
