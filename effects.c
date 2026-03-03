#include "effects.h"
#include <stdlib.h>
#include <math.h>

static void box_blur_h(uint32_t *dest, uint32_t *src, int w, int h, int radius) {
    double iarr = 1.0 / (radius + radius + 1);
    for (int i = 0; i < h; i++) {
        int ti = i * w, li = ti, ri = ti + radius;
        int fv = src[ti] & 0xFF, lv = src[ti + w - 1] & 0xFF, val = (radius + 1) * fv;
        int fva = (src[ti] >> 24) & 0xFF, lva = (src[ti + w - 1] >> 24) & 0xFF, vala = (radius + 1) * fva;
        int fvr = (src[ti] >> 16) & 0xFF, lvr = (src[ti + w - 1] >> 16) & 0xFF, valr = (radius + 1) * fvr;
        int fvg = (src[ti] >> 8) & 0xFF, lvg = (src[ti + w - 1] >> 8) & 0xFF, valg = (radius + 1) * fvg;

        for (int j = 0; j < radius; j++) {
            val += src[ti + j] & 0xFF;
            vala += (src[ti + j] >> 24) & 0xFF;
            valr += (src[ti + j] >> 16) & 0xFF;
            valg += (src[ti + j] >> 8) & 0xFF;
        }
        for (int j = 0; j <= radius; j++) {
            val += (src[ri] & 0xFF) - fv;
            vala += ((src[ri] >> 24) & 0xFF) - fva;
            valr += ((src[ri] >> 16) & 0xFF) - fvr;
            valg += ((src[ri] >> 8) & 0xFF) - fvg;
            dest[ti++] = ((int)(vala * iarr) << 24) | ((int)(valr * iarr) << 16) | ((int)(valg * iarr) << 8) | (int)(val * iarr);
            ri++;
        }
        for (int j = radius + 1; j < w - radius; j++) {
            val += (src[ri] & 0xFF) - (src[li] & 0xFF);
            vala += ((src[ri] >> 24) & 0xFF) - ((src[li] >> 24) & 0xFF);
            valr += ((src[ri] >> 16) & 0xFF) - ((src[li] >> 16) & 0xFF);
            valg += ((src[ri] >> 8) & 0xFF) - ((src[li] >> 8) & 0xFF);
            dest[ti++] = ((int)(vala * iarr) << 24) | ((int)(valr * iarr) << 16) | ((int)(valg * iarr) << 8) | (int)(val * iarr);
            li++;
            ri++;
        }
        for (int j = w - radius; j < w; j++) {
            val += lv - (src[li] & 0xFF);
            vala += lva - ((src[li] >> 24) & 0xFF);
            valr += lvr - ((src[li] >> 16) & 0xFF);
            valg += lvg - ((src[li] >> 8) & 0xFF);
            dest[ti++] = ((int)(vala * iarr) << 24) | ((int)(valr * iarr) << 16) | ((int)(valg * iarr) << 8) | (int)(val * iarr);
            li++;
        }
    }
}

static void box_blur_t(uint32_t *dest, uint32_t *src, int w, int h, int radius) {
    double iarr = 1.0 / (radius + radius + 1);
    for (int i = 0; i < w; i++) {
        int ti = i, li = ti, ri = ti + radius * w;
        int fv = src[ti] & 0xFF, lv = src[ti + w * (h - 1)] & 0xFF, val = (radius + 1) * fv;
        int fva = (src[ti] >> 24) & 0xFF, lva = (src[ti + w * (h - 1)] >> 24) & 0xFF, vala = (radius + 1) * fva;
        int fvr = (src[ti] >> 16) & 0xFF, lvr = (src[ti + w * (h - 1)] >> 16) & 0xFF, valr = (radius + 1) * fvr;
        int fvg = (src[ti] >> 8) & 0xFF, lvg = (src[ti + w * (h - 1)] >> 8) & 0xFF, valg = (radius + 1) * fvg;

        for (int j = 0; j < radius; j++) {
            val += src[ti + j * w] & 0xFF;
            vala += (src[ti + j * w] >> 24) & 0xFF;
            valr += (src[ti + j * w] >> 16) & 0xFF;
            valg += (src[ti + j * w] >> 8) & 0xFF;
        }
        for (int j = 0; j <= radius; j++) {
            val += (src[ri] & 0xFF) - fv;
            vala += ((src[ri] >> 24) & 0xFF) - fva;
            valr += ((src[ri] >> 16) & 0xFF) - fvr;
            valg += ((src[ri] >> 8) & 0xFF) - fvg;
            dest[ti] = ((int)(vala * iarr) << 24) | ((int)(valr * iarr) << 16) | ((int)(valg * iarr) << 8) | (int)(val * iarr);
            ti += w;
            ri += w;
        }
        for (int j = radius + 1; j < h - radius; j++) {
            val += (src[ri] & 0xFF) - (src[li] & 0xFF);
            vala += ((src[ri] >> 24) & 0xFF) - ((src[li] >> 24) & 0xFF);
            valr += ((src[ri] >> 16) & 0xFF) - ((src[li] >> 16) & 0xFF);
            valg += ((src[ri] >> 8) & 0xFF) - ((src[li] >> 8) & 0xFF);
            dest[ti] = ((int)(vala * iarr) << 24) | ((int)(valr * iarr) << 16) | ((int)(valg * iarr) << 8) | (int)(val * iarr);
            ti += w;
            li += w;
            ri += w;
        }
        for (int j = h - radius; j < h; j++) {
            val += lv - (src[li] & 0xFF);
            vala += lva - ((src[li] >> 24) & 0xFF);
            valr += lvr - ((src[li] >> 16) & 0xFF);
            valg += lvg - ((src[li] >> 8) & 0xFF);
            dest[ti] = ((int)(vala * iarr) << 24) | ((int)(valr * iarr) << 16) | ((int)(valg * iarr) << 8) | (int)(val * iarr);
            ti += w;
            li += w;
        }
    }
}

static void box_blur(uint32_t *src, uint32_t *dest, int w, int h, int radius) {
    if (radius <= 0) return;
    for (int i = 0; i < w * h; i++) dest[i] = src[i];
    box_blur_h(dest, src, w, h, radius);
    box_blur_t(src, dest, w, h, radius);
}

void apply_blur(cairo_surface_t *surface, uint32_t radius) {
    if (radius == 0 || cairo_surface_get_type(surface) != CAIRO_SURFACE_TYPE_IMAGE) return;
    int w = cairo_image_surface_get_width(surface);
    int h = cairo_image_surface_get_height(surface);
    cairo_surface_flush(surface);
    uint32_t *src = (uint32_t *)cairo_image_surface_get_data(surface);
    
    // Convert to multiple passes of box blur to approximate gaussian
    int bxs[3];
    double wIdeal = sqrt((12 * radius * radius / 3.0) + 1);
    int wl = floor(wIdeal);
    if (wl % 2 == 0) wl--;
    int wu = wl + 2;
    double mIdeal = (12 * radius * radius - 3 * wl * wl - 12 * wl - 9) / (double)(-4 * wl - 4);
    int m = round(mIdeal);
    for (int i = 0; i < 3; i++) bxs[i] = i < m ? wl : wu;
    
    uint32_t *dest = malloc(w * h * sizeof(uint32_t));
    if (!dest) return;
    
    uint32_t *temp = malloc(w * h * sizeof(uint32_t));
    if (!temp) {
        free(dest);
        return;
    }
    
    // 3 passes
    box_blur(src, dest, w, h, (bxs[0] - 1) / 2);
    box_blur(dest, temp, w, h, (bxs[0] - 1) / 2);
    box_blur(temp, src, w, h, (bxs[1] - 1) / 2);
    box_blur(src, dest, w, h, (bxs[1] - 1) / 2);
    box_blur(dest, temp, w, h, (bxs[2] - 1) / 2);
    box_blur(temp, src, w, h, (bxs[2] - 1) / 2);
    
    free(temp);
    free(dest);
    cairo_surface_mark_dirty(surface);
}

void apply_pixelate(cairo_surface_t *surface, uint32_t block_size) {
    if (block_size <= 1 || cairo_surface_get_type(surface) != CAIRO_SURFACE_TYPE_IMAGE) return;
    int w = cairo_image_surface_get_width(surface);
    int h = cairo_image_surface_get_height(surface);
    
    int sw = (w + block_size - 1) / block_size;
    int sh = (h + block_size - 1) / block_size;
    
    cairo_surface_t *small = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, sw, sh);
    cairo_t *c = cairo_create(small);
    cairo_set_source_surface(c, surface, 0, 0);
    cairo_pattern_t *pattern = cairo_get_source(c);
    cairo_pattern_set_filter(pattern, CAIRO_FILTER_NEAREST);
    cairo_matrix_t matrix;
    cairo_matrix_init_scale(&matrix, block_size, block_size);
    cairo_pattern_set_matrix(pattern, &matrix);
    
    cairo_set_operator(c, CAIRO_OPERATOR_SOURCE);
    cairo_paint(c);
    cairo_destroy(c);
    
    c = cairo_create(surface);
    cairo_set_source_surface(c, small, 0, 0);
    pattern = cairo_get_source(c);
    cairo_pattern_set_filter(pattern, CAIRO_FILTER_NEAREST);
    cairo_matrix_init_scale(&matrix, 1.0 / block_size, 1.0 / block_size);
    cairo_pattern_set_matrix(pattern, &matrix);
    
    cairo_set_operator(c, CAIRO_OPERATOR_SOURCE);
    cairo_paint(c);
    cairo_destroy(c);
    
    cairo_surface_destroy(small);
}
