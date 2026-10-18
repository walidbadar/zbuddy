/*
 * Copyright (c) 2026 Muhammad Waleed Badar
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * The text canvas of the ASCII species as an LVGL canvas: 160x88 at 1x,
 * shown at a whole pixel scale with nearest-neighbour scaling for the
 * pixel look.
 */
#ifndef ZBUDDY_APP_ART_CANVAS_H
#define ZBUDDY_APP_ART_CANVAS_H

#include <zbuddy/art.h>
#include <lvgl.h>

/** @p scale is the whole pixel scale the canvas is shown at. */
lv_obj_t *art_canvas_create(lv_obj_t *parent, int32_t scale);
/** Draw one frame; @p bg565 is the background colour. */
void art_canvas_render(const struct zbuddy_species *sp, enum zbuddy_persona s, uint32_t t,
		       uint16_t bg565);
lv_color_t art_color_from_565(uint16_t c);

#endif
