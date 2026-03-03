#include <time.h>
#include <pwd.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include "widgets.h"

static int get_battery_percentage() {
    FILE *f = fopen("/sys/class/power_supply/BAT0/capacity", "r");
    if (!f) f = fopen("/sys/class/power_supply/BAT1/capacity", "r");
    if (!f) return -1;
    int cap;
    if (fscanf(f, "%d", &cap) != 1) cap = -1;
    fclose(f);
    return cap;
}

static bool is_battery_charging() {
    FILE *f = fopen("/sys/class/power_supply/BAT0/status", "r");
    if (!f) f = fopen("/sys/class/power_supply/BAT1/status", "r");
    if (!f) return false;
    char status[32];
    bool charging = false;
    if (fscanf(f, "%31s", status) == 1) {
        if (strcmp(status, "Charging") == 0) charging = true;
    }
    fclose(f);
    return charging;
}

void render_widgets(cairo_t *cairo, struct swaylock_surface *surface, int buffer_width, int buffer_height) {
    struct swaylock_state *state = surface->state;

    cairo_select_font_face(cairo, state->args.font,
        CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);

    uint32_t text_color = state->args.colors.text.input; // Use input text color
    cairo_set_source_u32(cairo, text_color);
    
    int margin = 50 * surface->scale;
    int font_sz = (state->args.font_size > 0 ? state->args.font_size : state->args.radius / 1.5) * surface->scale;

    // Clock
    if (state->args.clock) {
        time_t t = time(NULL);
        struct tm *tm_info = localtime(&t);
        char time_buf[128];
        char date_buf[128];
        strftime(time_buf, sizeof(time_buf), state->args.timestr, tm_info);
        strftime(date_buf, sizeof(date_buf), state->args.datestr, tm_info);

        cairo_text_extents_t extents;
        cairo_set_font_size(cairo, font_sz * 1.5);
        cairo_text_extents(cairo, time_buf, &extents);
        
        cairo_move_to(cairo, margin, buffer_height - margin - font_sz * 1.5);
        cairo_show_text(cairo, time_buf);

        cairo_set_font_size(cairo, font_sz * 0.8);
        cairo_text_extents(cairo, date_buf, &extents);
        cairo_move_to(cairo, margin, buffer_height - margin);
        cairo_show_text(cairo, date_buf);
    }

    // Battery
    if (state->args.battery) {
        int bat = get_battery_percentage();
        if (bat >= 0) {
            bool charging = is_battery_charging();
            char bat_buf[32];
            snprintf(bat_buf, sizeof(bat_buf), "Bat: %d%%%s", bat, charging ? " (+)" : "");
            
            cairo_text_extents_t extents;
            cairo_set_font_size(cairo, font_sz);
            cairo_text_extents(cairo, bat_buf, &extents);
            cairo_move_to(cairo, buffer_width - margin - extents.width, buffer_height - margin);
            cairo_show_text(cairo, bat_buf);
        }
    }

    // User
    if (state->args.show_user) {
        struct passwd *pw = getpwuid(getuid());
        if (pw) {
            cairo_text_extents_t extents;
            cairo_set_font_size(cairo, font_sz);
            cairo_text_extents(cairo, pw->pw_name, &extents);
            cairo_move_to(cairo, margin, margin + font_sz);
            cairo_show_text(cairo, pw->pw_name);
        }
    }
}
