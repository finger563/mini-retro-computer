#pragma once
#include <lvgl.h>
#include <string>
#include <vector>

class Boot {
public:
  struct Config {
    int width = 128;
    int height = 128;
    const lv_font_t *font = nullptr;
  };

  explicit Boot(const Config &config);
  ~Boot();

  void init(lv_obj_t *parent);
  void deinit();
  void update();
  void add_line(const std::string &line);
  void update_last_line(const std::string &line);
  void start_fade_out();
  bool is_fading() const;
  bool is_visible() const;
  void set_visible(bool visible);

private:
  Config config_;
  lv_obj_t *container_{nullptr};
  lv_obj_t *label_{nullptr};
  std::vector<std::string> lines_;
  bool fading_{false};
  bool visible_{true};
};
