#ifndef _SWAYLOCK_H
#define _SWAYLOCK_H
#include "background-image.h"
#include "cairo.h"
#include "gl_renderer.h"
#include "pool-buffer.h"
#include "seat.h"
#include "wlr-screencopy-unstable-v1-client-protocol.h"
#include <stdbool.h>
#include <stdint.h>
#include <wayland-client.h>

// Indicator state: status of authentication attempt
enum auth_state {
  AUTH_STATE_IDLE,       // nothing happening
  AUTH_STATE_VALIDATING, // currently validating password
  AUTH_STATE_INVALID,    // displaying message: password was wrong
};

// Indicator state: status of password buffer / typing letters
enum input_state {
  INPUT_STATE_IDLE,  // nothing happening; other states decay to this after time
  INPUT_STATE_CLEAR, // displaying message: password buffer was cleared
  INPUT_STATE_LETTER,    // pressed a key that input a letter
  INPUT_STATE_BACKSPACE, // pressed backspace and removed a letter
  INPUT_STATE_NEUTRAL,   // pressed a key (like Ctrl) that did nothing
};

struct swaylock_colorset {
  uint32_t input;
  uint32_t cleared;
  uint32_t caps_lock;
  uint32_t verifying;
  uint32_t wrong;
};

struct swaylock_colors {
  uint32_t background;
  uint32_t bs_highlight;
  uint32_t key_highlight;
  uint32_t caps_lock_bs_highlight;
  uint32_t caps_lock_key_highlight;
  uint32_t separator;
  uint32_t layout_background;
  uint32_t layout_border;
  uint32_t layout_text;
  struct swaylock_colorset inside;
  struct swaylock_colorset line;
  struct swaylock_colorset ring;
  struct swaylock_colorset text;
};

#define MAX_RIPPLES 8
struct swaylock_ripple {
  struct timespec time;
  uint32_t color;
};

struct swaylock_args {
  struct swaylock_colors colors;
  enum background_mode mode;
  char *font;
  uint32_t font_size;
  uint32_t radius;
  uint32_t thickness;
  uint32_t indicator_x_position;
  uint32_t indicator_y_position;
  bool override_indicator_x_position;
  bool override_indicator_y_position;
  bool ignore_empty;
  bool show_indicator;
  bool show_caps_lock_text;
  bool show_caps_lock_indicator;
  bool show_keyboard_layout;
  bool hide_keyboard_layout;
  bool show_failed_attempts;
  bool daemonize;
  int ready_fd;
  bool indicator_idle_visible;
  bool screenshots;
  bool effect_blur;
  uint32_t effect_blur_radius;
  bool effect_pixelate;
  uint32_t effect_pixelate_block;
  bool effect_fade_in;
  uint32_t effect_fade_in_ms;
  bool clock;
  char *timestr;
  char *datestr;
  bool battery;
  bool show_user;
  int indicator_anim; // 0=none, 1=pulse, 2=spin, 3=pulse+spin
  uint32_t indicator_anim_duration;
  double indicator_anim_intensity;
};

struct swaylock_password {
  size_t len;
  size_t buffer_len;
  char *buffer;
};

struct swaylock_state {
  struct loop *eventloop;
  struct loop_timer *input_idle_timer; // timer to reset input state to IDLE
  struct loop_timer
      *auth_idle_timer; // timer to stop displaying AUTH_STATE_INVALID
  struct loop_timer *clear_password_timer; // clears the password buffer
  struct wl_display *display;
  struct wl_compositor *compositor;
  struct wl_subcompositor *subcompositor;
  struct wl_shm *shm;
  struct wl_list surfaces;
  struct wl_list images;
  struct swaylock_args args;
  struct swaylock_password password;
  struct swaylock_xkb xkb;
  cairo_surface_t *test_surface;
  cairo_t *test_cairo;          // used to estimate font/text sizes
  enum auth_state auth_state;   // state of the authentication attempt
  enum input_state input_state; // state of the password buffer and key inputs
  uint32_t highlight_start;     // position of highlight; 2048 = 1 full turn
  int failed_attempts;
  bool run_display, locked;
  struct ext_session_lock_manager_v1 *ext_session_lock_manager_v1;
  struct ext_session_lock_v1 *ext_session_lock_v1;
  struct zwlr_screencopy_manager_v1 *screencopy_manager;

  struct swaylock_ripple ripples[MAX_RIPPLES];
  uint32_t ripple_idx;

  // OpenGL state (shared)
  struct gl_state gl;
};

struct swaylock_surface {
  cairo_surface_t *image;
  struct swaylock_state *state;
  struct wl_output *output;
  uint32_t output_global_name;
  struct wl_surface *surface; // surface for background
  struct wl_surface *child;   // indicator surface made into subsurface
  struct wl_subsurface *subsurface;
  struct ext_session_lock_surface_v1 *ext_session_lock_surface_v1;
  struct pool_buffer indicator_buffers[2];
  struct pool_buffer background_buffers[2];
  bool created;
  bool dirty;
  bool bg_dirty;
  bool fade_started;
  struct timespec fade_start_time;

  double indicator_angle;
  double indicator_velocity;
  struct timespec last_anim_update;
  enum auth_state last_auth_state;
  uint32_t width, height;
  int32_t scale;
  enum wl_output_subpixel subpixel;
  char *output_name;
  struct wl_list link;
  struct wl_callback *frame;
  // Dimensions of last wl_buffer committed to background surface
  int last_buffer_width, last_buffer_height;
  struct zwlr_screencopy_frame_v1 *screencopy_frame;
  struct pool_buffer screencopy_buffer;
  bool screencopy_done;

  // OpenGL per-surface state
  struct gl_surface_state gl_surface;
};

// There is exactly one swaylock_image for each -i argument
struct swaylock_image {
  char *path;
  char *output_name;
  cairo_surface_t *cairo_surface;
  struct wl_list link;
};

void swaylock_handle_key(struct swaylock_state *state, xkb_keysym_t keysym,
                         uint32_t codepoint);

void render(struct swaylock_surface *surface);
void damage_state(struct swaylock_state *state);
void clear_password_buffer(struct swaylock_password *pw);
void schedule_auth_idle(struct swaylock_state *state);

void initialize_pw_backend(int argc, char **argv);
void run_pw_backend_child(void);
void clear_buffer(char *buf, size_t size);

#endif
