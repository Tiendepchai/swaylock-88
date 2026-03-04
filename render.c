#include "gl_renderer.h"
#include "log.h"
#include "swaylock.h"

#include <stdlib.h>
#include <time.h>
#include <wayland-client.h>

#define M_PI 3.14159265358979323846
const float TYPE_INDICATOR_RANGE = M_PI / 3.0f;

static void surface_frame_handle_done(void *data, struct wl_callback *callback,
                                      uint32_t time) {
  struct swaylock_surface *surface = data;

  wl_callback_destroy(callback);
  surface->frame = NULL;

  render(surface);
}

static const struct wl_callback_listener surface_frame_listener = {
    .done = surface_frame_handle_done,
};

void render(struct swaylock_surface *surface) {
  struct swaylock_state *state = surface->state;

  int buffer_width = surface->width * surface->scale;
  int buffer_height = surface->height * surface->scale;
  if (buffer_width == 0 || buffer_height == 0) {
    return; // not yet configured
  }

  if (!surface->dirty || surface->frame) {
    // Nothing to do or frame already pending
    return;
  }

  surface->dirty = false; // will be set back to true if animations are ongoing

  // ======== OpenGL Rendering Path ========
  if (state->gl.initialized && surface->gl_surface.egl_window) {
    // Load font once per surface
    if (!surface->gl_surface.font_loaded) {
      char font_path[512];
      const char *family = state->args.font ? state->args.font : "monospace";
      if (gl_resolve_font_path(family, font_path, sizeof(font_path))) {
        float scaled_font_size = 28.0f * (float)surface->scale;
        gl_load_font(&state->gl, &surface->gl_surface, font_path,
                     scaled_font_size);
      }
    }

    // Upload background texture once
    if (!surface->gl_surface.bg_texture_valid && surface->image) {
      int img_w = cairo_image_surface_get_width(surface->image);
      int img_h = cairo_image_surface_get_height(surface->image);
      unsigned char *data = cairo_image_surface_get_data(surface->image);
      if (data && img_w > 0 && img_h > 0) {
        // Cairo uses BGRA, OpenGL expects RGBA - swap R and B channels
        int stride = cairo_image_surface_get_stride(surface->image);
        unsigned char *rgba = malloc(img_w * img_h * 4);
        if (rgba) {
          for (int y = 0; y < img_h; y++) {
            unsigned char *src = data + y * stride;
            unsigned char *dst = rgba + y * img_w * 4;
            for (int x = 0; x < img_w; x++) {
              dst[x * 4 + 0] = src[x * 4 + 2]; // R <- B
              dst[x * 4 + 1] = src[x * 4 + 1]; // G
              dst[x * 4 + 2] = src[x * 4 + 0]; // B <- R
              dst[x * 4 + 3] = src[x * 4 + 3]; // A
            }
          }
          gl_upload_background(&state->gl, &surface->gl_surface, rgba, img_w,
                               img_h, state->args.effect_blur,
                               state->args.effect_blur_radius);
          free(rgba);
        }
      }
    }

    // EGL frame scheduling must happen before swap buffers
    wl_surface_set_buffer_scale(surface->surface, surface->scale);
    surface->frame = wl_surface_frame(surface->surface);
    wl_callback_add_listener(surface->frame, &surface_frame_listener, surface);

    // Render the frame via GPU (calls eglSwapBuffers which commits)
    gl_render_frame(&state->gl, &surface->gl_surface, surface, buffer_width,
                    buffer_height);

    // Manage animation framerate
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    if (surface->last_auth_state != state->auth_state) {
      surface->last_anim_update = now;
      surface->last_auth_state = state->auth_state;
    }

    double time = (now.tv_sec - surface->last_anim_update.tv_sec) +
                  (now.tv_nsec - surface->last_anim_update.tv_nsec) / 1e9;

    bool is_animating = false;
    if (state->args.indicator_anim == 1 || state->args.indicator_anim == 5) {
      is_animating = true; // Breathing and glow are infinite
    } else if (state->args.indicator_anim > 0 && time < 2.0) {
      is_animating = true; // Other effects fade out after 2 seconds
    }

    if (is_animating) {
      surface->dirty = true;
    }

    return;
  } else {
    swaylock_log(LOG_ERROR, "Fatal: OpenGL is not initialized and Cairo "
                            "fallback has been removed.");
  }
}
