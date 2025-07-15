#pragma once

#include <deque>
#include <lvgl.h>
#include <string>
#include <vector>

#include "format.hpp"

class MatrixRain {
public:
  /// @brief Configuration for the MatrixRain effect.
  /// This struct contains parameters to customize the appearance and behavior
  /// of the rain effect.
  struct Config {
    int screen_width = 128;           //< Size of the screen in pixels
    int screen_height = 128;          //< Size of the screen in pixels
    int char_width = 8;               //< Size of each character in pixels
    int char_height = 8;              //< Size of each character in pixels
    int min_drop_length = 6;          //< Min length of a drop in characters
    int max_drop_length = 16;         //< Max length of a drop in characters
    int update_interval_ms = 40;      //< Update interval in ms
    int fade_duration_ms = 100;       //< Duration to fade a character in ms
    int head_mutate_interval_ms = 10; //< Interval to mutate the head of a drop in ms
    int drop_spawn_interval_ms = 200; //< Interval to spawn a new drop in ms
    int drop_spawn_chance = 5;        //< Chance to spawn a drop on a frame. 1/x (1 in x) chance.
    int min_speed_ms = 10;            //< Minimum speed of a drop in ms
    int speed_range_ms = 100;         //< Range of speed variation in ms (max - min)
    int image_reveal_min_duration_ms = 3000;  //< Min duration to show the image in ms
    int image_reveal_max_duration_ms = 5000;  //< Max duration to show the image in ms
    int image_erase_duration_ms = 2000;       //< Duration of the erasing animation in ms
    int image_reveal_min_interval_ms = 8000;  //< Min interval between image reveals in ms
    int image_reveal_max_interval_ms = 15000; //< Max interval between image reveals in ms
    int image_drop_speed_ms = 10;             //< Speed of static drops for image reveal
  };

  /// @brief Constructor for the MatrixRain effect.
  /// @param config Configuration parameters for the rain effect.
  explicit MatrixRain(const Config &config);
  /// @brief Destructor for the MatrixRain effect.
  ~MatrixRain();

  /// @brief Initializes the MatrixRain effect on the specified parent object.
  /// This function creates the necessary UI elements and starts the effect.
  /// @param parent The parent object to attach the rain effect to.
  void init(lv_obj_t *parent);
  /// @brief Deinitializes the MatrixRain effect, cleaning up resources.
  void deinit();
  /// @brief Updates the MatrixRain effect.
  void update();
  /// @brief Restarts the MatrixRain effect, resetting all drops and cells.
  void restart();
  /// @brief Sets the font for the characters in the rain effect.
  /// @param font The font to use for rendering characters.
  void set_font(const lv_font_t *font);
  /// @brief Sets whether the MatrixRain effect is visible.
  /// @param visible True to show the effect, false to hide it.
  void set_visible(bool visible);
  /// @brief Sets a label / prompt to display behind the rain effect.
  /// @param text The text to display as a prompt.
  void set_prompt(const char *text); // Show a label behind the rain (optional)
  /// @brief Sets an image to be rendered using the matrix rain effect.
  /// Passing nullptr will disable image rendering mode and revert to random rain.
  /// @param img Pointer to the LVGL image descriptor.
  void set_image(const lv_img_dsc_t *img);

  /// @brief Sets the minimum brightness for the image rendering mode.
  /// This value is used to determine the minimum brightness of pixels in the image
  /// that will be rendered. Pixels with brightness below this value will not be shown.
  /// @param brightness The minimum brightness value (0-255).
  /// Default is 0, meaning all pixels will be shown.
  void set_min_image_brightness(uint8_t brightness) { min_image_brightness_ = brightness; }

  /// @brief Prints the current image brightness map to the console for debugging.
  void print_image_brightness_map();
  /// @brief Shows a static label which represents the computed brightness map.
  void debug_show_image();

private:
  enum class ImageRevealState {
    NORMAL,    //< Standard random rain
    CLEARING,  //< Stop spawning, let existing drops fall
    REVEALING, //< Show the image with static drops
    ERASING    //< Wash away the image with random drops
  };

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
    bool is_image_drop{false};  //< True if this drop is for revealing the image
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
  bool image_mode_{false};
  std::vector<uint8_t> image_brightness_map_;
  uint8_t min_image_brightness_{0};
  ImageRevealState image_state_{ImageRevealState::NORMAL};
  uint32_t state_transition_time_{0};

  void spawn_drop(Column &col, uint32_t now, bool is_image_drop = false);
  void update_drop(Column &col, Drop &drop, uint32_t now);
  void update_fade(Column &col, uint32_t now);
  void update_row_labels(uint32_t now);
  bool is_screen_clear();
  void set_next_reveal_time();

  static uint32_t random_katakana();
  static void unicode_to_utf8(uint32_t unicode, char *utf8);
  static uint8_t get_pixel_brightness(const lv_img_dsc_t *img, int x, int y);
};
