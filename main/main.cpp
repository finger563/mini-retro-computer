#include <chrono>
#include <thread>

#include "byte90.hpp"
#include "logger.hpp"
#include "task.hpp"

using namespace std::chrono_literals;

static std::recursive_mutex lvgl_mutex;

extern "C" void app_main(void) {
  static auto start = std::chrono::high_resolution_clock::now();
  static auto elapsed = [&]() {
    auto now = std::chrono::high_resolution_clock::now();
    return std::chrono::duration<float>(now - start).count();
  };

  espp::Logger logger({.tag = "MRP", .level = espp::Logger::Verbosity::INFO});

  logger.info("Bootup");

  espp::Byte90 &byte90 = espp::Byte90::get();
  byte90.set_log_level(espp::Logger::Verbosity::INFO);

  // initialize the accelerometer
  if (!byte90.initialize_accelerometer()) {
    logger.error("Failed to initialize accelerometer!");
  }
  // initialize the LCD
  if (!byte90.initialize_lcd()) {
    logger.error("Failed to initialize LCD!");
    return;
  }
  // set the pixel buffer to be 50 lines high
  static constexpr size_t pixel_buffer_size = byte90.lcd_width() * 50;
  // initialize the LVGL display for the Byte90
  if (!byte90.initialize_display(pixel_buffer_size)) {
    logger.error("Failed to initialize display!");
    return;
  }
  // initialize the button, which we'll use to cycle the rotation of the display
  logger.info("Initializing the button");
  auto on_button_pressed = [&](const auto &event) {
    if (event.active) {
      // increment the brightness by 10%, looping back to 0% after 100%
      auto brightness = byte90.brightness();
      brightness = std::fmod(brightness + 10.0f, 100.0f);
      logger.info("Setting brightness to {:.0f}%", brightness);
      byte90.brightness(brightness);
      // lock the display mutex
      std::lock_guard<std::recursive_mutex> lock(lvgl_mutex);
      static auto rotation = LV_DISPLAY_ROTATION_0;
      rotation = static_cast<lv_display_rotation_t>((static_cast<int>(rotation) + 1) % 4);
      lv_display_t *disp = lv_disp_get_default();
      lv_disp_set_rotation(disp, rotation);
    }
  };
  byte90.initialize_button(on_button_pressed);

  // also print in the main thread
  while (true) {
    logger.debug("[{:.3f}] Hello World!", elapsed());
    std::this_thread::sleep_for(1s);
  }
}
