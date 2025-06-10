#pragma once
#include <lvgl.h>
#include <string>

class Terminal {
public:
  struct Config {
    int width = 128;
    int height = 128;
    const lv_font_t *font = nullptr;
  };

  Terminal(const Config &config);
  ~Terminal();

  void init(lv_obj_t *parent);
  void deinit();
  void update();
  void set_prompt(const std::string &prompt);
  void kb_type(char c);
  void blink();
  void set_visible(bool visible);
  bool is_visible() const;
  void start_fade_out();
  bool is_fading() const;

private:
  Config config_;
  lv_obj_t *container_{nullptr};
  lv_obj_t *label_{nullptr};
  std::string prompt_;
  std::string typed_;
  bool cursor_visible_{true};
  bool fading_{false};
  // ... typing/blink state ...
};