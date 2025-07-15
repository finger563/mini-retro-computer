#include <chrono>
#include <thread>

#include "gui.hpp"
#include "lvgl.h"

#if CONFIG_MRP_HARDWARE_BYTE90
#include "byte90.hpp"
using Bsp = espp::Byte90;
#define HAS_ACCELEROMETER 1
#elif CONFIG_MRP_HARDWARE_WS_S3_TOUCHLCD
#include "ws-s3-touch.hpp"
using Bsp = espp::WsS3Touch;
#define HAS_IMU 1
#else
#error                                                                                             \
    "Unsupported hardware configuration. Please select a valid hardware configuration in the menuconfig."
#endif

#include "file_system.hpp"
#include "logger.hpp"
#include "task.hpp"

#include "jpeg.hpp"

namespace fs = std::filesystem;
using namespace std::chrono_literals;

static Jpeg decoder;

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

  // load the image file (smith.jpg) from the root of the littlefs partition
  logger.info("Loading image from file system");
  const fs::path file = espp::FileSystem::get().get_root_path() / "smith.jpg";

  // ensure it exists
  if (!fs::exists(file)) {
    logger.error("File '{}' does not exist!", file.string());
    return;
  }

  // load the file
  decoder.decode(file.c_str());
  // make the descriptor
  lv_image_dsc_t img_desc;
  memset(&img_desc, 0, sizeof(img_desc));
  img_desc.header.cf = LV_COLOR_FORMAT_NATIVE;
  img_desc.header.w = decoder.get_width();
  img_desc.header.h = decoder.get_height();
  img_desc.data_size = decoder.get_size();
  img_desc.data = decoder.get_decoded_data();

  // now initialize the GUI
  Gui gui({});
  if (auto mr = gui.get_matrix_rain()) {
    mr->set_image(&img_desc);
    mr->set_min_image_brightness(0);
  }

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
