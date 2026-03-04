#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#include "gl_renderer.h"
#include "log.h"
#include "shaders.h"
#include "swaylock.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef EGL_EGLEXT_PROTOTYPES
#define EGL_EGLEXT_PROTOTYPES
#endif
#include <EGL/eglext.h>

// ============================================================
// Font path resolution via fontconfig (fc-match)
// ============================================================

bool gl_resolve_font_path(const char *font_family, char *out_path,
                          size_t out_size) {
  if (strchr(font_family, '\'')) {
    swaylock_log(LOG_ERROR, "Invalid font family name (contains single quote)");
    return false;
  }

  char cmd[512];
  snprintf(cmd, sizeof(cmd), "fc-match -f '%%{file}' '%s:style=Regular'",
           font_family);
  FILE *pipe = popen(cmd, "r");
  if (!pipe) {
    swaylock_log(LOG_ERROR, "popen(fc-match) failed");
    return false;
  }
  if (!fgets(out_path, out_size, pipe)) {
    pclose(pipe);
    swaylock_log(LOG_ERROR, "fc-match returned no result for '%s'",
                 font_family);
    return false;
  }
  pclose(pipe);
  // Strip trailing newline
  size_t len = strlen(out_path);
  if (len > 0 && out_path[len - 1] == '\n')
    out_path[len - 1] = '\0';
  swaylock_log(LOG_DEBUG, "Resolved font '%s' -> '%s'", font_family, out_path);
  return true;
}

// ============================================================
// Shader utilities
// ============================================================

static GLuint compile_shader(GLenum type, const char *src) {
  GLuint shader = glCreateShader(type);
  glShaderSource(shader, 1, &src, NULL);
  glCompileShader(shader);
  GLint ok;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
  if (!ok) {
    char log[512];
    glGetShaderInfoLog(shader, sizeof(log), NULL, log);
    swaylock_log(LOG_ERROR, "Shader compile error: %s", log);
    glDeleteShader(shader);
    return 0;
  }
  return shader;
}

static GLuint link_program(const char *vert_src, const char *frag_src) {
  GLuint vs = compile_shader(GL_VERTEX_SHADER, vert_src);
  GLuint fs = compile_shader(GL_FRAGMENT_SHADER, frag_src);
  if (!vs || !fs)
    return 0;

  GLuint prog = glCreateProgram();
  glAttachShader(prog, vs);
  glAttachShader(prog, fs);
  glLinkProgram(prog);

  GLint ok;
  glGetProgramiv(prog, GL_LINK_STATUS, &ok);
  if (!ok) {
    char log[512];
    glGetProgramInfoLog(prog, sizeof(log), NULL, log);
    swaylock_log(LOG_ERROR, "Program link error: %s", log);
    glDeleteProgram(prog);
    prog = 0;
  }
  glDeleteShader(vs);
  glDeleteShader(fs);
  return prog;
}

// ============================================================
// EGL Initialization
// ============================================================

bool gl_init(struct gl_state *gl, struct wl_display *display) {
  memset(gl, 0, sizeof(*gl));

  PFNEGLGETPLATFORMDISPLAYEXTPROC get_platform_display =
      (PFNEGLGETPLATFORMDISPLAYEXTPROC)eglGetProcAddress(
          "eglGetPlatformDisplayEXT");
  if (get_platform_display) {
    gl->egl_display = get_platform_display(
        0x31D8 /* EGL_PLATFORM_WAYLAND_EXT */, display, NULL);
  } else {
    gl->egl_display = eglGetDisplay((EGLNativeDisplayType)display);
  }

  if (gl->egl_display == EGL_NO_DISPLAY) {
    swaylock_log(LOG_ERROR, "eglGetDisplay failed");
    return false;
  }

  EGLint major, minor;
  if (!eglInitialize(gl->egl_display, &major, &minor)) {
    swaylock_log(LOG_ERROR, "eglInitialize failed");
    return false;
  }
  swaylock_log(LOG_DEBUG, "EGL %d.%d initialized", major, minor);

  if (!eglBindAPI(EGL_OPENGL_ES_API)) {
    swaylock_log(LOG_ERROR, "eglBindAPI failed");
    return false;
  }

  static const EGLint config_attribs[] = {
      EGL_SURFACE_TYPE,
      EGL_WINDOW_BIT,
      EGL_RED_SIZE,
      8,
      EGL_GREEN_SIZE,
      8,
      EGL_BLUE_SIZE,
      8,
      EGL_ALPHA_SIZE,
      8,
      EGL_RENDERABLE_TYPE,
      EGL_OPENGL_ES2_BIT,
      EGL_NONE,
  };

  EGLint num_configs;
  if (!eglChooseConfig(gl->egl_display, config_attribs, &gl->egl_config, 1,
                       &num_configs) ||
      num_configs == 0) {
    swaylock_log(LOG_ERROR, "eglChooseConfig failed");
    return false;
  }

  static const EGLint context_attribs[] = {
      EGL_CONTEXT_CLIENT_VERSION,
      2,
      EGL_NONE,
  };

  gl->egl_context = eglCreateContext(gl->egl_display, gl->egl_config,
                                     EGL_NO_CONTEXT, context_attribs);
  if (gl->egl_context == EGL_NO_CONTEXT) {
    swaylock_log(LOG_ERROR, "eglCreateContext failed");
    return false;
  }

  // Make current with no surface (for shader/texture setup)
  eglMakeCurrent(gl->egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE,
                 gl->egl_context);

  // Compile shaders
  gl->bg_program = link_program(bg_vertex_shader_src, bg_fragment_shader_src);
  gl->text_program =
      link_program(text_vertex_shader_src, text_fragment_shader_src);
  gl->blur_down_program =
      link_program(bg_vertex_shader_src, blur_down_fragment_shader_src);
  gl->blur_up_program =
      link_program(bg_vertex_shader_src, blur_up_fragment_shader_src);
  if (!gl->bg_program || !gl->text_program || !gl->blur_down_program ||
      !gl->blur_up_program) {
    swaylock_log(LOG_ERROR, "Failed to compile shaders");
    return false;
  }

  // Create fullscreen quad VBO
  // positions (clip space) + texcoords
  static const float quad_verts[] = {
      // x,    y,    u,   v
      -1.0f, -1.0f, 0.0f, 1.0f, 1.0f, -1.0f, 1.0f, 1.0f,
      -1.0f, 1.0f,  0.0f, 0.0f, 1.0f, 1.0f,  1.0f, 0.0f,
  };
  glGenBuffers(1, &gl->quad_vbo);
  glBindBuffer(GL_ARRAY_BUFFER, gl->quad_vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(quad_verts), quad_verts, GL_STATIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, 0);

  gl->initialized = true;
  swaylock_log(LOG_DEBUG, "OpenGL renderer initialized");
  return true;
}

// ============================================================
// Per-surface EGL setup
// ============================================================

bool gl_surface_init(struct gl_state *gl, struct gl_surface_state *gls,
                     struct wl_surface *surface, int width, int height) {
  memset(gls, 0, sizeof(*gls));

  gls->egl_window = wl_egl_window_create(surface, width, height);
  if (!gls->egl_window) {
    swaylock_log(LOG_ERROR, "wl_egl_window_create failed");
    return false;
  }

  gls->egl_surface =
      eglCreateWindowSurface(gl->egl_display, gl->egl_config,
                             (EGLNativeWindowType)gls->egl_window, NULL);
  if (gls->egl_surface == EGL_NO_SURFACE) {
    swaylock_log(LOG_ERROR, "eglCreateWindowSurface failed");
    wl_egl_window_destroy(gls->egl_window);
    return false;
  }

  return true;
}

void gl_surface_resize(struct gl_surface_state *gls, int width, int height) {
  if (gls->egl_window) {
    wl_egl_window_resize(gls->egl_window, width, height, 0, 0);
  }
}

// ============================================================
// Upload background as GL texture
// ============================================================

void gl_upload_background(struct gl_state *gl, struct gl_surface_state *gls,
                          unsigned char *pixels, int width, int height,
                          bool apply_blur, int blur_passes) {
  eglMakeCurrent(gl->egl_display, gls->egl_surface, gls->egl_surface,
                 gl->egl_context);

  GLuint orig_texture;
  glGenTextures(1, &orig_texture);
  glBindTexture(GL_TEXTURE_2D, orig_texture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, pixels);
  glBindTexture(GL_TEXTURE_2D, 0);

  if (!apply_blur || blur_passes <= 0) {
    if (gls->bg_texture)
      glDeleteTextures(1, &gls->bg_texture);
    gls->bg_texture = orig_texture;
    gls->bg_texture_valid = true;
    gls->bg_tex_width = width;
    gls->bg_tex_height = height;
    return;
  }

  // --- Dual Filtering Blur ---
  // Allocate FBOs for pyramid
  int max_passes = blur_passes;
  if (max_passes > 8)
    max_passes = 8;

  GLuint fbos[8];
  GLuint texs[8];
  int ws[8];
  int hs[8];

  glGenFramebuffers(max_passes, fbos);
  glGenTextures(max_passes, texs);

  int cw = width / 2;
  int ch = height / 2;
  for (int i = 0; i < max_passes; i++) {
    ws[i] = cw > 1 ? cw : 1;
    hs[i] = ch > 1 ? ch : 1;
    glBindTexture(GL_TEXTURE_2D, texs[i]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, ws[i], hs[i], 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glBindFramebuffer(GL_FRAMEBUFFER, fbos[i]);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           texs[i], 0);

    cw /= 2;
    ch /= 2;
  }

  glBindBuffer(GL_ARRAY_BUFFER, gl->quad_vbo);

  // Downsample Phase
  glUseProgram(gl->blur_down_program);
  GLint d_pos = glGetAttribLocation(gl->blur_down_program, "a_pos");
  GLint d_tc = glGetAttribLocation(gl->blur_down_program, "a_texcoord");
  glVertexAttribPointer(d_pos, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                        (void *)0);
  glEnableVertexAttribArray(d_pos);
  glVertexAttribPointer(d_tc, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                        (void *)(2 * sizeof(float)));
  glEnableVertexAttribArray(d_tc);

  GLint d_tex = glGetUniformLocation(gl->blur_down_program, "u_texture");
  GLint d_hp = glGetUniformLocation(gl->blur_down_program, "u_halfpixel");
  glUniform1i(d_tex, 0);

  for (int i = 0; i < max_passes; i++) {
    glBindFramebuffer(GL_FRAMEBUFFER, fbos[i]);
    glViewport(0, 0, ws[i], hs[i]);

    glActiveTexture(GL_TEXTURE0);
    if (i == 0) {
      glBindTexture(GL_TEXTURE_2D, orig_texture);
      glUniform2f(d_hp, 0.5f / width, 0.5f / height);
    } else {
      glBindTexture(GL_TEXTURE_2D, texs[i - 1]);
      glUniform2f(d_hp, 0.5f / ws[i - 1], 0.5f / hs[i - 1]);
    }

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  }

  // Upsample Phase
  glUseProgram(gl->blur_up_program);
  GLint u_pos = glGetAttribLocation(gl->blur_up_program, "a_pos");
  GLint u_tc = glGetAttribLocation(gl->blur_up_program, "a_texcoord");
  glVertexAttribPointer(u_pos, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                        (void *)0);
  glEnableVertexAttribArray(u_pos);
  glVertexAttribPointer(u_tc, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                        (void *)(2 * sizeof(float)));
  glEnableVertexAttribArray(u_tc);

  GLint u_tex = glGetUniformLocation(gl->blur_up_program, "u_texture");
  GLint u_hp = glGetUniformLocation(gl->blur_up_program, "u_halfpixel");
  glUniform1i(u_tex, 0);

  for (int i = max_passes - 2; i >= 0; i--) {
    glBindFramebuffer(GL_FRAMEBUFFER, fbos[i]);
    glViewport(0, 0, ws[i], hs[i]);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texs[i + 1]);
    glUniform2f(u_hp, 0.5f / ws[i + 1], 0.5f / hs[i + 1]);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  }

  // Final upsample back to full res
  GLuint final_texture;
  glGenTextures(1, &final_texture);
  glBindTexture(GL_TEXTURE_2D, final_texture);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  GLuint final_fbo;
  glGenFramebuffers(1, &final_fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, final_fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                         final_texture, 0);

  glViewport(0, 0, width, height);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, texs[0]);
  glUniform2f(u_hp, 0.5f / ws[0], 0.5f / hs[0]);
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

  // Cleanup FBOs
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glDeleteFramebuffers(max_passes, fbos);
  glDeleteTextures(max_passes, texs);
  glDeleteFramebuffers(1, &final_fbo);
  glDeleteTextures(1, &orig_texture);
  glBindBuffer(GL_ARRAY_BUFFER, 0);

  if (gls->bg_texture)
    glDeleteTextures(1, &gls->bg_texture);
  gls->bg_texture = final_texture;
  gls->bg_texture_valid = true;
  gls->bg_tex_width = width;
  gls->bg_tex_height = height;
}

// ============================================================
// SDF Font Loading (using stb_truetype)
// ============================================================

bool gl_load_font(struct gl_state *gl, struct gl_surface_state *gls,
                  const char *font_path, float font_size) {
  FILE *f = fopen(font_path, "rb");
  if (!f) {
    swaylock_log(LOG_ERROR, "Cannot open font: %s", font_path);
    return false;
  }

  fseek(f, 0, SEEK_END);
  long fsize = ftell(f);
  fseek(f, 0, SEEK_SET);
  unsigned char *font_data = malloc(fsize);
  if (!font_data) {
    fclose(f);
    return false;
  }
  if (fread(font_data, 1, fsize, f) != (size_t)fsize) {
    free(font_data);
    fclose(f);
    return false;
  }
  fclose(f);

  // Generate SDF atlas
  gls->atlas_width = 512;
  gls->atlas_height = 512;
  unsigned char *atlas_bitmap = calloc(gls->atlas_width * gls->atlas_height, 1);
  if (!atlas_bitmap) {
    free(font_data);
    return false;
  }

  stbtt_fontinfo font;
  if (!stbtt_InitFont(&font, font_data, 0)) {
    swaylock_log(LOG_ERROR, "stbtt_InitFont failed for %s", font_path);
    free(atlas_bitmap);
    free(font_data);
    return false;
  }

  float scale = stbtt_ScaleForPixelHeight(&font, font_size);

  // Pack glyphs for printable ASCII chars
  int pad = 4;      // SDF padding
  int onedge = 128; // value at edge
  float sdf_scale = (float)onedge / (float)pad;
  int atlas_x = 1, atlas_y = 1;
  int row_height = 0;

  for (int ch = 32; ch < 127; ch++) {
    int ix0, iy0, ix1, iy1;
    stbtt_GetCodepointBitmapBox(&font, ch, scale, scale, &ix0, &iy0, &ix1,
                                &iy1);
    int gw = ix1 - ix0;
    int gh = iy1 - iy0;
    int sdf_w = gw + pad * 2;
    int sdf_h = gh + pad * 2;

    // Line-wrap in atlas
    if (atlas_x + sdf_w + 1 >= gls->atlas_width) {
      atlas_x = 1;
      atlas_y += row_height + 1;
      row_height = 0;
    }
    if (atlas_y + sdf_h + 1 >= gls->atlas_height) {
      swaylock_log(LOG_ERROR, "Font atlas too small");
      break;
    }

    // Render SDF for this glyph
    unsigned char *sdf = stbtt_GetCodepointSDF(
        &font, scale, ch, pad, onedge, sdf_scale, &sdf_w, &sdf_h, NULL, NULL);
    if (sdf) {
      for (int y = 0; y < sdf_h; y++) {
        for (int x = 0; x < sdf_w; x++) {
          int dst = (atlas_y + y) * gls->atlas_width + (atlas_x + x);
          atlas_bitmap[dst] = sdf[y * sdf_w + x];
        }
      }
      stbtt_FreeSDF(sdf, NULL);
    }

    // Store glyph metrics
    gls->glyphs[ch].x0 = (float)atlas_x / gls->atlas_width;
    gls->glyphs[ch].y0 = (float)atlas_y / gls->atlas_height;
    gls->glyphs[ch].x1 = (float)(atlas_x + sdf_w) / gls->atlas_width;
    gls->glyphs[ch].y1 = (float)(atlas_y + sdf_h) / gls->atlas_height;
    gls->glyphs[ch].xoff = (float)ix0 - pad;
    gls->glyphs[ch].yoff = (float)iy0 - pad;
    gls->glyphs[ch].w = (float)sdf_w;
    gls->glyphs[ch].h = (float)sdf_h;

    int advance, lsb;
    stbtt_GetCodepointHMetrics(&font, ch, &advance, &lsb);
    gls->glyphs[ch].xadvance = (float)advance * scale;

    if (sdf_h > row_height)
      row_height = sdf_h;
    atlas_x += sdf_w + 1;
  }

  // Upload atlas to GPU as alpha texture
  glGenTextures(1, &gls->font_atlas_texture);
  glBindTexture(GL_TEXTURE_2D, gls->font_atlas_texture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, gls->atlas_width, gls->atlas_height,
               0, GL_ALPHA, GL_UNSIGNED_BYTE, atlas_bitmap);
  glBindTexture(GL_TEXTURE_2D, 0);

  free(atlas_bitmap);
  free(font_data);

  gls->font_loaded = true;
  swaylock_log(LOG_DEBUG, "SDF font atlas generated (%dx%d)", gls->atlas_width,
               gls->atlas_height);
  return true;
}

// ============================================================
// Text rendering with elastic kerning
// ============================================================

static void draw_text(struct gl_state *gl, struct gl_surface_state *gls,
                      const char *text, float x, float y, float extra_spacing,
                      float r, float g, float b, float a, int viewport_w,
                      int viewport_h, float time, int anim_mode) {
  if (!gls->font_loaded || !text || !*text)
    return;

  glUseProgram(gl->text_program);

  // Simple orthographic projection matrix
  float proj[16] = {0};
  proj[0] = 2.0f / viewport_w;
  proj[5] = -2.0f / viewport_h; // Y flipped
  proj[10] = 1.0f;
  proj[12] = -1.0f;
  proj[13] = 1.0f;
  proj[15] = 1.0f;

  GLint loc_proj = glGetUniformLocation(gl->text_program, "u_projection");
  glUniformMatrix4fv(loc_proj, 1, GL_FALSE, proj);

  GLint loc_color = glGetUniformLocation(gl->text_program, "u_text_color");
  glUniform4f(loc_color, r, g, b, a);

  GLint loc_smooth = glGetUniformLocation(gl->text_program, "u_smoothing");
  glUniform1f(loc_smooth, 0.1f);

  GLint loc_time = glGetUniformLocation(gl->text_program, "u_time");
  glUniform1f(loc_time, time);

  GLint loc_anim_mode = glGetUniformLocation(gl->text_program, "u_anim_mode");
  glUniform1i(loc_anim_mode, anim_mode);

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, gls->font_atlas_texture);
  GLint loc_atlas = glGetUniformLocation(gl->text_program, "u_font_atlas");
  glUniform1i(loc_atlas, 0);

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  GLint a_pos = glGetAttribLocation(gl->text_program, "a_pos");
  GLint a_tc = glGetAttribLocation(gl->text_program, "a_texcoord");
  GLint a_char_idx = glGetAttribLocation(gl->text_program, "a_char_idx");

  float cursor_x = x;
  int char_idx = 0;
  for (const char *p = text; *p; p++) {
    int ch = (unsigned char)*p;
    if (ch < 32 || ch >= 127)
      ch = '?';

    float gx = cursor_x + gls->glyphs[ch].xoff;
    float gy = y + gls->glyphs[ch].yoff;
    float gw = gls->glyphs[ch].w;
    float gh = gls->glyphs[ch].h;

    float u0 = gls->glyphs[ch].x0;
    float v0 = gls->glyphs[ch].y0;
    float u1 = gls->glyphs[ch].x1;
    float v1 = gls->glyphs[ch].y1;

    float verts[] = {
        gx, gy,      u0, v0, gx + gw, gy,      u1, v0,
        gx, gy + gh, u0, v1, gx + gw, gy + gh, u1, v1,
    };

    glVertexAttribPointer(a_pos, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                          verts);
    glEnableVertexAttribArray(a_pos);
    glVertexAttribPointer(a_tc, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                          verts + 2);
    glEnableVertexAttribArray(a_tc);

    if (a_char_idx >= 0) {
      glVertexAttrib1f(a_char_idx, (float)char_idx);
    }

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    cursor_x += gls->glyphs[ch].xadvance + extra_spacing;
    char_idx++;
  }
  glDisable(GL_BLEND);
}

// ============================================================
// Main render entry point
// ============================================================

void gl_render_frame(struct gl_state *gl, struct gl_surface_state *gls,
                     struct swaylock_surface *surface, int width, int height) {
  struct swaylock_state *state = surface->state;

  eglMakeCurrent(gl->egl_display, gls->egl_surface, gls->egl_surface,
                 gl->egl_context);

  glViewport(0, 0, width, height);
  glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);

  // --- Draw Background ---
  if (gls->bg_texture_valid) {
    glUseProgram(gl->bg_program);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gls->bg_texture);

    GLint loc_tex = glGetUniformLocation(gl->bg_program, "u_texture");
    glUniform1i(loc_tex, 0);

    GLint loc_alpha = glGetUniformLocation(gl->bg_program, "u_alpha");
    glUniform1f(loc_alpha, 1.0f);

    glBindBuffer(GL_ARRAY_BUFFER, gl->quad_vbo);
    GLint a_pos = glGetAttribLocation(gl->bg_program, "a_pos");
    GLint a_tc = glGetAttribLocation(gl->bg_program, "a_texcoord");
    glVertexAttribPointer(a_pos, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                          (void *)0);
    glEnableVertexAttribArray(a_pos);
    glVertexAttribPointer(a_tc, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                          (void *)(2 * sizeof(float)));
    glEnableVertexAttribArray(a_tc);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
  }

  // --- Draw Text Indicator ---
  const char *indicator_text = "locked";
  if (state->input_state == INPUT_STATE_CLEAR) {
    indicator_text = "cleared";
  } else if (state->auth_state == AUTH_STATE_VALIDATING) {
    indicator_text = "verifying...";
  } else if (state->auth_state == AUTH_STATE_INVALID) {
    indicator_text = "access denied";
  } else if (state->input_state == INPUT_STATE_LETTER ||
             state->input_state == INPUT_STATE_BACKSPACE) {
    indicator_text = "typing...";
  }

  // Spring physics for kerning
  float extra_spacing = 0; // Removing old kerning override if anim is selected

  // Calculate time since animation started (for Pop/Glitch/Ripple sync)
  struct timespec now;
  clock_gettime(CLOCK_MONOTONIC, &now);
  double time = (now.tv_sec - surface->last_anim_update.tv_sec) +
                (now.tv_nsec - surface->last_anim_update.tv_nsec) / 1e9;

  int anim_mode = state->args.indicator_anim;

  // Measure approx text width for centering
  float text_width = 0;
  for (const char *p = indicator_text; *p; p++) {
    int ch = (unsigned char)*p;
    if (ch >= 32 && ch < 127) {
      text_width += gls->glyphs[ch].xadvance + extra_spacing;
    }
  }

  float tx = (width - text_width) / 2.0f;
  float ty = height / 2.0f;

  // Color based on state
  float cr = 1.0f, cg = 1.0f, cb = 1.0f, ca = 0.7f;
  if (state->auth_state == AUTH_STATE_INVALID) {
    cr = 1.0f;
    cg = 0.3f;
    cb = 0.3f;
    ca = 1.0f;
  } else if (state->auth_state == AUTH_STATE_VALIDATING) {
    cr = 0.5f;
    cg = 0.8f;
    cb = 1.0f;
    ca = 1.0f;
  } else if (state->input_state == INPUT_STATE_LETTER ||
             state->input_state == INPUT_STATE_BACKSPACE) {
    cr = 1.0f;
    cg = 1.0f;
    cb = 1.0f;
    ca = 1.0f;
  }

  draw_text(gl, gls, indicator_text, tx, ty, extra_spacing, cr, cg, cb, ca,
            width, height, (float)time, anim_mode);

  eglSwapBuffers(gl->egl_display, gls->egl_surface);
}

// ============================================================
// Cleanup
// ============================================================

void gl_surface_destroy(struct gl_state *gl, struct gl_surface_state *gls) {
  if (gls->bg_texture)
    glDeleteTextures(1, &gls->bg_texture);
  if (gls->font_atlas_texture)
    glDeleteTextures(1, &gls->font_atlas_texture);
  if (gls->egl_surface != EGL_NO_SURFACE) {
    eglDestroySurface(gl->egl_display, gls->egl_surface);
  }
  if (gls->egl_window)
    wl_egl_window_destroy(gls->egl_window);
  memset(gls, 0, sizeof(*gls));
}

void gl_destroy(struct gl_state *gl) {
  if (gl->bg_program)
    glDeleteProgram(gl->bg_program);
  if (gl->text_program)
    glDeleteProgram(gl->text_program);
  if (gl->quad_vbo)
    glDeleteBuffers(1, &gl->quad_vbo);
  if (gl->egl_context != EGL_NO_CONTEXT) {
    eglDestroyContext(gl->egl_display, gl->egl_context);
  }
  if (gl->egl_display != EGL_NO_DISPLAY) {
    eglTerminate(gl->egl_display);
  }
  memset(gl, 0, sizeof(*gl));
}
