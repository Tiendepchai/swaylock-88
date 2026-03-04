#include "widgets.h"
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#include <pwd.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static int cached_battery_cap = -1;
static bool cached_battery_charging = false;
static time_t last_battery_check = 0;

static void update_battery_cache_if_needed() {
  time_t now = time(NULL);
  if (now - last_battery_check < 5)
    return;

  FILE *f = fopen("/sys/class/power_supply/BAT0/capacity", "r");
  if (!f)
    f = fopen("/sys/class/power_supply/BAT1/capacity", "r");
  if (f) {
    int cap;
    if (fscanf(f, "%d", &cap) == 1)
      cached_battery_cap = cap;
    fclose(f);
  } else {
    cached_battery_cap = -1;
  }

  f = fopen("/sys/class/power_supply/BAT0/status", "r");
  if (!f)
    f = fopen("/sys/class/power_supply/BAT1/status", "r");
  if (f) {
    char status[32];
    if (fscanf(f, "%31s", status) == 1) {
      cached_battery_charging = (strcmp(status, "Charging") == 0);
    }
    fclose(f);
  } else {
    cached_battery_charging = false;
  }

  last_battery_check = now;
}

static char cached_username[256] = {0};
static bool username_cached = false;

static const char *get_cached_username() {
  if (!username_cached) {
    struct passwd *pw = getpwuid(getuid());
    if (pw && pw->pw_name) {
      strncpy(cached_username, pw->pw_name, sizeof(cached_username) - 1);
    }
    username_cached = true;
  }
  return cached_username;
}

static char last_clock_text[128] = {0};
static int last_battery_cap = -2;
static bool last_battery_charging = false;

bool widgets_need_redraw(struct swaylock_state *state) {
  bool changed = false;

  if (state->args.clock) {
    time_t t = time(NULL);
    struct tm *tm_info = localtime(&t);
    char hm_buf[32];
    strftime(hm_buf, sizeof(hm_buf), "%H:%M", tm_info);
    if (strcmp(hm_buf, last_clock_text) != 0) {
      strncpy(last_clock_text, hm_buf, sizeof(last_clock_text) - 1);
      changed = true;
    }
  }

  if (state->args.battery) {
    update_battery_cache_if_needed();
    if (cached_battery_cap != last_battery_cap ||
        cached_battery_charging != last_battery_charging) {
      last_battery_cap = cached_battery_cap;
      last_battery_charging = cached_battery_charging;
      changed = true;
    }
  }

  return changed;
}

// Helper to draw a rounded rectangle (pill shape)
static void draw_pill_background(cairo_t *cairo, double x, double y,
                                 double width, double height, double radius) {
  cairo_new_sub_path(cairo);
  cairo_arc(cairo, x + width - radius, y + radius, radius, -M_PI / 2, 0);
  cairo_arc(cairo, x + width - radius, y + height - radius, radius, 0,
            M_PI / 2);
  cairo_arc(cairo, x + radius, y + height - radius, radius, M_PI / 2, M_PI);
  cairo_arc(cairo, x + radius, y + radius, radius, M_PI, 3 * M_PI / 2);
  cairo_close_path(cairo);
  cairo_set_source_rgba(cairo, 0.0, 0.0, 0.0,
                        0.4); // Glassmorphism dark background
  cairo_fill(cairo);
}

void render_widgets(cairo_t *cairo, struct swaylock_surface *surface,
                    int buffer_width, int buffer_height) {
  struct swaylock_state *state = surface->state;
  int margin = 50 * surface->scale;
  int padding = 15 * surface->scale;
  int font_sz = (state->args.font_size > 0 ? state->args.font_size
                                           : state->args.radius / 1.5) *
                surface->scale;

  cairo_set_source_rgba(cairo, 1.0, 1.0, 1.0, 1.0); // White text

  // Clock
  if (state->args.clock) {
    time_t t = time(NULL);
    struct tm *tm_info = localtime(&t);
    char hm_buf[32];
    char date_buf[128];
    strftime(hm_buf, sizeof(hm_buf), "%H:%M", tm_info);
    strftime(date_buf, sizeof(date_buf), state->args.datestr, tm_info);

    cairo_text_extents_t hm_ext, date_ext;

    // Measure Hour:Minute (Bold, Large)
    cairo_select_font_face(cairo, state->args.font, CAIRO_FONT_SLANT_NORMAL,
                           CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cairo, font_sz * 2.5);
    cairo_text_extents(cairo, hm_buf, &hm_ext);

    // Measure Date (Light, Smaller)
    cairo_select_font_face(cairo, state->args.font, CAIRO_FONT_SLANT_NORMAL,
                           CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cairo, font_sz * 0.9);
    cairo_text_extents(cairo, date_buf, &date_ext);

    // Calculate Pill Box for Clock
    double clock_width = hm_ext.width + padding * 2;
    double clock_height = hm_ext.height + date_ext.height + padding * 3;
    double cx = margin;
    double cy = buffer_height - margin - clock_height;

    draw_pill_background(cairo, cx, cy, clock_width, clock_height,
                         20 * surface->scale);

    // Draw Hour:Minute
    cairo_set_source_rgba(cairo, 1.0, 1.0, 1.0, 1.0);
    cairo_select_font_face(cairo, state->args.font, CAIRO_FONT_SLANT_NORMAL,
                           CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cairo, font_sz * 2.5);
    cairo_move_to(cairo, cx + padding, cy + padding + hm_ext.height);
    cairo_show_text(cairo, hm_buf);

    // Draw Date
    cairo_select_font_face(cairo, state->args.font, CAIRO_FONT_SLANT_NORMAL,
                           CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cairo, font_sz * 0.9);
    cairo_move_to(cairo, cx + padding, cy + clock_height - padding);
    cairo_show_text(cairo, date_buf);
  }

  // Battery
  if (state->args.battery) {
    update_battery_cache_if_needed();
    int bat = cached_battery_cap;
    if (bat >= 0) {
      bool charging = cached_battery_charging;
      const char *icon = "󰁹"; // 100%
      if (charging) {
        icon = "󰂄";
      } else {
        if (bat <= 10)
          icon = "󰂃";
        else if (bat <= 25)
          icon = "󰁼";
        else if (bat <= 50)
          icon = "󰁾";
        else if (bat <= 75)
          icon = "󰂀";
      }

      char bat_buf[64];
      snprintf(bat_buf, sizeof(bat_buf), "%s  %d%%", icon, bat);

      cairo_select_font_face(cairo, state->args.font, CAIRO_FONT_SLANT_NORMAL,
                             CAIRO_FONT_WEIGHT_BOLD);
      cairo_set_font_size(cairo, font_sz * 1.2);
      cairo_text_extents_t extents;
      cairo_text_extents(cairo, bat_buf, &extents);

      double bx = buffer_width - margin - extents.width - padding * 2;
      double by = buffer_height - margin - extents.height - padding * 2;

      draw_pill_background(cairo, bx, by, extents.width + padding * 2,
                           extents.height + padding * 2, 20 * surface->scale);

      cairo_set_source_rgba(cairo, 1.0, 1.0, 1.0, 1.0);
      cairo_move_to(cairo, bx + padding, by + padding + extents.height);
      cairo_show_text(cairo, bat_buf);
    }
  }

  // User
  if (state->args.show_user) {
    const char *pw_name = get_cached_username();
    if (pw_name[0] != '\0') {
      char user_buf[256];
      snprintf(user_buf, sizeof(user_buf), "  %s", pw_name);

      cairo_select_font_face(cairo, state->args.font, CAIRO_FONT_SLANT_NORMAL,
                             CAIRO_FONT_WEIGHT_BOLD);
      cairo_set_font_size(cairo, font_sz * 1.2);
      cairo_text_extents_t extents;
      cairo_text_extents(cairo, user_buf, &extents);

      double ux = margin;
      double uy = margin;

      draw_pill_background(cairo, ux, uy, extents.width + padding * 2,
                           extents.height + padding * 2, 20 * surface->scale);

      cairo_set_source_rgba(cairo, 1.0, 1.0, 1.0, 1.0);
      cairo_move_to(cairo, ux + padding, uy + padding + extents.height);
      cairo_show_text(cairo, user_buf);
    }
  }
}
