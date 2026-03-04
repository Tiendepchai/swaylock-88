#ifndef _SWAY_WIDGETS_H
#define _SWAY_WIDGETS_H

#include "cairo.h"
#include "swaylock.h"

void render_widgets(cairo_t *cairo, struct swaylock_surface *surface,
                    int buffer_width, int buffer_height);
bool widgets_need_redraw(struct swaylock_state *state);

#endif
