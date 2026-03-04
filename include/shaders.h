#ifndef _SWAY_SHADERS_H
#define _SWAY_SHADERS_H

// === Background Shader (Passthrough + optional blur) ===

static const char *bg_vertex_shader_src =
    "attribute vec2 a_pos;\n"
    "attribute vec2 a_texcoord;\n"
    "varying vec2 v_texcoord;\n"
    "void main() {\n"
    "    gl_Position = vec4(a_pos, 0.0, 1.0);\n"
    "    v_texcoord = a_texcoord;\n"
    "}\n";

static const char *bg_fragment_shader_src =
    "precision mediump float;\n"
    "varying vec2 v_texcoord;\n"
    "uniform sampler2D u_texture;\n"
    "uniform float u_alpha;\n"
    "void main() {\n"
    "    vec4 color = texture2D(u_texture, v_texcoord);\n"
    "    gl_FragColor = vec4(color.rgb, color.a * u_alpha);\n"
    "}\n";

// === Blur Downsample Shader (Dual Filtering) ===
static const char *blur_down_fragment_shader_src =
    "precision mediump float;\n"
    "varying vec2 v_texcoord;\n"
    "uniform sampler2D u_texture;\n"
    "uniform vec2 u_halfpixel;\n" // 0.5 / resolution
    "void main() {\n"
    "    vec2 uv = v_texcoord;\n"
    "    vec4 sum = texture2D(u_texture, uv) * 4.0;\n"
    "    sum += texture2D(u_texture, uv - u_halfpixel.xy);\n"
    "    sum += texture2D(u_texture, uv + u_halfpixel.xy);\n"
    "    sum += texture2D(u_texture, uv + vec2(u_halfpixel.x, "
    "-u_halfpixel.y));\n"
    "    sum += texture2D(u_texture, uv - vec2(u_halfpixel.x, "
    "-u_halfpixel.y));\n"
    "    gl_FragColor = sum / 8.0;\n"
    "}\n";

// === Blur Upsample Shader (Dual Filtering) ===
static const char *blur_up_fragment_shader_src =
    "precision mediump float;\n"
    "varying vec2 v_texcoord;\n"
    "uniform sampler2D u_texture;\n"
    "uniform vec2 u_halfpixel;\n" // 0.5 / resolution
    "void main() {\n"
    "    vec2 uv = v_texcoord;\n"
    "    vec4 sum = vec4(0.0);\n"
    "    sum += texture2D(u_texture, uv + vec2(-u_halfpixel.x * 2.0, 0.0));\n"
    "    sum += texture2D(u_texture, uv + vec2(-u_halfpixel.x, u_halfpixel.y)) "
    "* 2.0;\n"
    "    sum += texture2D(u_texture, uv + vec2(0.0, u_halfpixel.y * 2.0));\n"
    "    sum += texture2D(u_texture, uv + vec2(u_halfpixel.x, u_halfpixel.y)) "
    "* 2.0;\n"
    "    sum += texture2D(u_texture, uv + vec2(u_halfpixel.x * 2.0, 0.0));\n"
    "    sum += texture2D(u_texture, uv + vec2(u_halfpixel.x, -u_halfpixel.y)) "
    "* 2.0;\n"
    "    sum += texture2D(u_texture, uv + vec2(0.0, -u_halfpixel.y * 2.0));\n"
    "    sum += texture2D(u_texture, uv + vec2(-u_halfpixel.x, "
    "-u_halfpixel.y)) * 2.0;\n"
    "    gl_FragColor = sum / 12.0;\n"
    "}\n";

// === SDF Text Shader ===

static const char *text_vertex_shader_src =
    "precision mediump float;\n"
    "attribute vec2 a_pos;\n"
    "attribute vec2 a_texcoord;\n"
    "attribute float a_char_idx;\n" // for ripple effect
    "varying vec2 v_texcoord;\n"
    "uniform mat4 u_projection;\n"
    "uniform float u_time;\n"
    "uniform mediump int u_anim_mode;\n"
    "void main() {\n"
    "    vec2 pos = a_pos;\n"
    "    if (u_anim_mode == 3) {\n" // Ripple
    "        pos.y += sin(u_time * 15.0 - a_char_idx * 0.8) * 8.0 * max(0.0, "
    "1.0 - u_time);\n"
    "    } else if (u_anim_mode == 4) {\n" // Pop & Squeeze
    "        float scaleY = 1.0 - sin(u_time * 10.0) * 0.3 * exp(-u_time * "
    "3.0);\n"
    "        float scaleX = 1.0 + sin(u_time * 10.0) * 0.1 * exp(-u_time * "
    "3.0);\n"
    "        // Assume baseline is roughly 0, scale relative to it\n"
    "        pos.x *= scaleX;\n"
    "        pos.y *= scaleY;\n"
    "    }\n"
    "    gl_Position = u_projection * vec4(pos, 0.0, 1.0);\n"
    "    v_texcoord = a_texcoord;\n"
    "}\n";

static const char *text_fragment_shader_src =
    "precision mediump float;\n"
    "varying vec2 v_texcoord;\n"
    "uniform sampler2D u_font_atlas;\n"
    "uniform vec4 u_text_color;\n"
    "uniform float u_smoothing;\n"
    "uniform float u_time;\n"
    "uniform mediump int u_anim_mode;\n"
    "void main() {\n"
    "    if (u_anim_mode == 2) {\n" // Glitch / Chromatic Aberration
    "        float glitch_amount = exp(-u_time * 8.0) * 0.05 * sin(u_time * "
    "50.0);\n"
    "        float distR = texture2D(u_font_atlas, v_texcoord + "
    "vec2(glitch_amount, 0.0)).a;\n"
    "        float distG = texture2D(u_font_atlas, v_texcoord).a;\n"
    "        float distB = texture2D(u_font_atlas, v_texcoord - "
    "vec2(glitch_amount, 0.0)).a;\n"
    "        float aR = smoothstep(0.5 - u_smoothing, 0.5 + u_smoothing, "
    "distR);\n"
    "        float aG = smoothstep(0.5 - u_smoothing, 0.5 + u_smoothing, "
    "distG);\n"
    "        float aB = smoothstep(0.5 - u_smoothing, 0.5 + u_smoothing, "
    "distB);\n"
    "        gl_FragColor = vec4(u_text_color.r * aR, u_text_color.g * aG, "
    "u_text_color.b * aB, max(max(aR, aG), aB) * u_text_color.a);\n"
    "        return;\n"
    "    }\n"
    "    \n"
    "    float dist = texture2D(u_font_atlas, v_texcoord).a;\n"
    "    float center = 0.5;\n"
    "    if (u_anim_mode == 1) {\n" // Breathing (Variable Weight)
    "        center = 0.5 + sin(u_time * 2.0) * 0.08;\n"
    "    }\n"
    "    \n"
    "    float alpha = smoothstep(center - u_smoothing, center + u_smoothing, "
    "dist);\n"
    "    vec4 final_color = vec4(u_text_color.rgb, u_text_color.a * alpha);\n"
    "    \n"
    "    if (u_anim_mode == 5) {\n" // Neon Glow
    "        float glow = smoothstep(0.3, 0.6, dist);\n"
    "        vec4 glow_color = vec4(u_text_color.rgb, u_text_color.a * 0.4 * "
    "glow);\n"
    "        final_color = mix(glow_color, final_color, alpha);\n"
    "    }\n"
    "    \n"
    "    gl_FragColor = final_color;\n"
    "}\n";

#endif
