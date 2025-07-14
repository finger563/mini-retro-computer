#include "gui.hpp"
#include "boot.hpp"
#include "matrix_rain.hpp"
#include "terminal.hpp"
#include <cstdlib>
#include <ctime>
#include <functional>
#include <lvgl.h>

extern "C" {
extern const lv_font_t unscii_8_jp;
}

void Gui::deinit_ui() {
  logger_.info("Deinitializing UI");
  if (boot_)
    boot_->deinit();
  if (terminal_)
    terminal_->deinit();
  if (matrix_rain_)
    matrix_rain_->deinit();
  lv_anim_del(NULL, NULL);          // Cancel all animations
  lv_obj_clean(lv_screen_active()); // Clean all children from the screen
  boot_.reset();
  terminal_.reset();
  matrix_rain_.reset();
}

void Gui::init_ui() {
  logger_.info("Initializing UI");
  size_t screen_width = lv_disp_get_hor_res(NULL);
  size_t screen_height = lv_disp_get_ver_res(NULL);

  // Do NOT call lv_obj_clean here
  // Boot
  Boot::Config boot_cfg;
  boot_cfg.width = screen_width;
  boot_cfg.height = screen_height;
  boot_cfg.font = &unscii_8_jp;
  boot_ = std::make_unique<Boot>(boot_cfg);
  boot_->init(lv_screen_active());
  // Terminal
  Terminal::Config term_cfg;
  term_cfg.width = screen_width;
  term_cfg.height = screen_height;
  term_cfg.font = &unscii_8_jp;
  terminal_ = std::make_unique<Terminal>(term_cfg);
  terminal_->init(lv_screen_active());
  terminal_->set_visible(false);
  // MatrixRain
  MatrixRain::Config rain_cfg;
  rain_cfg.screen_width = screen_width;
  rain_cfg.screen_height = screen_height;
  rain_cfg.char_width = matrix_char_width_;
  rain_cfg.char_height = matrix_char_height_;
  rain_cfg.min_drop_length = 3;
  rain_cfg.max_drop_length = 6;
  rain_cfg.update_interval_ms = 40;
  matrix_rain_ = std::make_unique<MatrixRain>(rain_cfg);
  matrix_rain_->set_font(&unscii_8_jp);
  matrix_rain_->init(lv_screen_active());
  matrix_rain_->set_visible(false);
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

// Add boot animation state
struct BootLineAnim {
  enum class State { IDLE, ANIMATING_MEM, PAUSE_AFTER_COLON, DONE } state = State::IDLE;
  size_t current_mem = 0;
  uint32_t last_update = 0;
  std::string line_text;
  std::string prefix;
  std::string suffix;
};
static BootLineAnim boot_anim;

void Gui::update() {
  if (paused_)
    return;
  std::lock_guard<std::recursive_mutex> lk(mutex_);

  uint32_t now = lv_tick_get();

  switch (mode_) {
  case Mode::BOOT: {
    // Handle animated boot lines
    if (boot_line_index_ < boot_lines_.size()) {
      std::string line = boot_lines_[boot_line_index_];
      // Animated memory check
      if (line.find("{MEM}") != std::string::npos) {
        if (boot_anim.state == BootLineAnim::State::IDLE) {
          boot_anim.state = BootLineAnim::State::ANIMATING_MEM;
          boot_anim.current_mem = 0;
          boot_anim.last_update = now;
          boot_anim.line_text = line;
        }
        if (boot_anim.state == BootLineAnim::State::ANIMATING_MEM) {
          if (now - boot_anim.last_update > 10) {
            boot_anim.current_mem += 32;
            if (boot_anim.current_mem >= 640) {
              boot_anim.current_mem = 640;
              boot_anim.state = BootLineAnim::State::DONE;
            }
            boot_anim.last_update = now;
            // Update label with current value
            std::string mem_line = line;
            size_t pos = mem_line.find("{MEM}");
            mem_line.replace(pos, 5, std::to_string(boot_anim.current_mem));
            if (boot_)
              boot_->update_last_line(mem_line);
          }
          // Wait for animation to finish
          if (boot_anim.state != BootLineAnim::State::DONE)
            break;
        }
      }
      // Pause after colon
      else if (line.find(":") != std::string::npos &&
               boot_anim.state == BootLineAnim::State::IDLE) {
        size_t pos = line.find(":");
        boot_anim.prefix = line.substr(0, pos + 1);
        boot_anim.suffix = line.substr(pos + 1);
        boot_anim.last_update = now;
        boot_anim.state = BootLineAnim::State::PAUSE_AFTER_COLON;
        // Show only prefix
        if (boot_)
          boot_->add_line(boot_anim.prefix);
        break;
      } else if (boot_anim.state == BootLineAnim::State::PAUSE_AFTER_COLON) {
        if (now - boot_anim.last_update > 350) {
          // Show full line
          if (boot_)
            boot_->add_line(boot_anim.prefix + boot_anim.suffix);
          boot_anim.state = BootLineAnim::State::DONE;
        } else {
          break;
        }
      }
      // Normal line
      if (boot_anim.state == BootLineAnim::State::IDLE ||
          boot_anim.state == BootLineAnim::State::DONE) {
        if (boot_anim.state == BootLineAnim::State::DONE) {
          boot_anim.state = BootLineAnim::State::IDLE;
          boot_line_index_++;
          last_boot_line_time_ = now;
          break;
        }
        if (now - last_boot_line_time_ > boot_line_delay_ms_) {
          if (boot_)
            boot_->add_line(line);
          boot_line_index_++;
          last_boot_line_time_ = now;
        }
      }
    } else {
      // Boot finished, go to terminal
      mode_ = Mode::TERMINAL;
      terminal_start_time_ = now;
      terminal_prompt_chars_shown_ = 0;
      boot_anim = BootLineAnim{};
      if (boot_)
        boot_->start_fade_out();
      if (terminal_)
        terminal_->set_visible(true);
    }
    if (boot_)
      boot_->update();
    break;
  }
  case Mode::TERMINAL: {
    if (terminal_)
      terminal_->update();
    // Animate typing the terminal prompt with per-line delay
    if (terminal_prompt_chars_shown_ < terminal_prompt_.size()) {
      uint32_t delay = 60;
      if (terminal_prompt_[terminal_prompt_chars_shown_] == '\n')
        delay = 600; // longer pause after each line
      if (now - last_char_time_ > delay) {
        if (terminal_)
          terminal_->kb_type(terminal_prompt_[terminal_prompt_chars_shown_]);
        terminal_prompt_chars_shown_++;
        last_char_time_ = now;
      }
    } else {
      // After a short pause, go to matrix rain
      if (now - terminal_start_time_ > terminal_duration_ms_ + 1000) {
        mode_ = Mode::MATRIX_RAIN;
        matrix_rain_start_time_ = now;
        if (terminal_)
          terminal_->start_fade_out();
        if (matrix_rain_) {
          matrix_rain_->set_visible(true);
          // matrix_rain_->set_prompt(terminal_prompt_.c_str());
        }
      }
    }
    break;
  }
  case Mode::MATRIX_RAIN:
    if (matrix_rain_)
      matrix_rain_->update();
    break;
  }

  lv_task_handler();
}

void Gui::restart() {
  std::lock_guard<std::recursive_mutex> lk(mutex_);
  // Reset all state
  boot_line_index_ = 0;
  terminal_prompt_chars_shown_ = 0;
  last_boot_line_time_ = 0;
  terminal_start_time_ = 0;
  matrix_rain_start_time_ = 0;
  mode_ = Mode::BOOT;
  last_char_time_ = 0;
  // Re-init UI
  deinit_ui();
  init_ui();
}
