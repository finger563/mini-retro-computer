#include <chrono>
#include <thread>

#include "gui.hpp"

#if 1
#include "byte90.hpp"
using Bsp = espp::Byte90;
#define HAS_ACCELEROMETER 1
#else
#include "ws-s3-touch.hpp"
using Bsp = espp::WsS3Touch;
#define HAS_IMU 1
#endif

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

  auto &bsp = Bsp::get();
  bsp.set_log_level(espp::Logger::Verbosity::INFO);

#if HAS_ACCELEROMETER
  // initialize the accelerometer
  if (!bsp.initialize_accelerometer()) {
    logger.error("Failed to initialize accelerometer!");
  }
#elif HAS_IMU
  // initialize the IMU
  if (!bsp.initialize_imu()) {
    logger.error("Failed to initialize IMU!");
  }
#endif

  // initialize the LCD
  if (!bsp.initialize_lcd()) {
    logger.error("Failed to initialize LCD!");
    return;
  }
  // set the pixel buffer to be 50 lines high
  static constexpr size_t pixel_buffer_size = bsp.lcd_width() * 50;
  // initialize the LVGL display
  if (!bsp.initialize_display(pixel_buffer_size)) {
    logger.error("Failed to initialize display!");
    return;
  }

  // now initialize the GUI
  Gui gui({});

  // initialize the button, which we'll use to cycle the rotation of the display
  logger.info("Initializing the button");
  auto on_button_pressed = [&](const auto &event) {
    if (event.active) {
      // call reset on the gui
      logger.info("Button pressed, restarting GUI");
      gui.restart();
    }
  };
  bsp.initialize_button(on_button_pressed);

  // also print in the main thread
  while (true) {
    logger.debug("[{:.3f}] Hello World!", elapsed());
    std::this_thread::sleep_for(1s);
  }
}
