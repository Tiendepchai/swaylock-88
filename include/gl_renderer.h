#ifndef _SWAY_GL_RENDERER_H
#define _SWAY_GL_RENDERER_H

#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <stdbool.h>
#include <wayland-client.h>
#include <wayland-egl.h>

struct swaylock_state;
struct swaylock_surface;

// Per-surface OpenGL context
struct gl_surface_state {
  struct wl_egl_window *egl_window;
  EGLSurface egl_surface;

  // Background texture (rendered once from cairo image)
  GLuint bg_texture;
  bool bg_texture_valid;
  int bg_tex_width, bg_tex_height;

  // Font atlas texture
  GLuint font_texture;
  bool font_texture_valid;
};

// Global OpenGL state (shared across surfaces)
struct gl_state {
  EGLDisplay egl_display;
  EGLContext egl_context;
  EGLConfig egl_config;
  bool initialized;

  // Shader programs
  GLuint bg_program;        // background quad + blur
  GLuint text_program;      // SDF text rendering
  GLuint blur_down_program; // dual filter downsample
  GLuint blur_up_program;   // dual filter upsample

  // Shared vertex data
  GLuint quad_vbo;

  // Font atlas
  GLuint font_atlas_texture;
  int atlas_width, atlas_height;
  bool font_loaded;

  // Glyph metrics for ASCII 32..126
  struct {
    float x0, y0, x1, y1; // texture coords
    float xoff, yoff;     // pixel offset from baseline
    float xadvance;       // horizontal advance
    float w, h;           // pixel dimensions
  } glyphs[128];
};

// Resolve font family name to TTF file path via fontconfig
bool gl_resolve_font_path(const char *font_family, char *out_path,
                          size_t out_size);

// Initialize global EGL context
bool gl_init(struct gl_state *gl, struct wl_display *display);

// Create per-surface EGL window surface
bool gl_surface_init(struct gl_state *gl, struct gl_surface_state *gls,
                     struct wl_surface *surface, int width, int height);

// Resize the EGL window
void gl_surface_resize(struct gl_surface_state *gls, int width, int height);

// Upload a background image (from pixels) as a GL texture
void gl_upload_background(struct gl_state *gl, struct gl_surface_state *gls,
                          unsigned char *pixels, int width, int height,
                          bool apply_blur, int blur_passes);

// Load a TTF font file and generate SDF atlas
bool gl_load_font(struct gl_state *gl, const char *font_path, float font_size);

// Render the full frame: background blur + text indicator
void gl_render_frame(struct gl_state *gl, struct gl_surface_state *gls,
                     struct swaylock_surface *surface, int width, int height);

// Cleanup
void gl_surface_destroy(struct gl_state *gl, struct gl_surface_state *gls);
void gl_destroy(struct gl_state *gl);

#endif
