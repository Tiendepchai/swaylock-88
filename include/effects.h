#ifndef _SWAYLOCK_EFFECTS_H
#define _SWAYLOCK_EFFECTS_H

#include <cairo.h>
#include <stdint.h>
#include <stdbool.h>

void apply_blur(cairo_surface_t *surface, uint32_t radius);
void apply_pixelate(cairo_surface_t *surface, uint32_t block_size);

#endif
