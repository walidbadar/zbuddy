/*
 * Copyright (c) 2026 Muhammad Waleed Badar
 * SPDX-License-Identifier: GPL-3.0-only
 */
#include "art_canvas.h"

#include <string.h>

LV_DRAW_BUF_DEFINE_STATIC(art_buf, ZBUDDY_ART_CANVAS_W, ZBUDDY_ART_CANVAS_H,
			  LV_COLOR_FORMAT_RGB565);

static lv_obj_t *canvas;
static lv_layer_t layer;
static lv_color_t cur_color;
static int cur_x, cur_y;

lv_color_t art_color_from_565(uint16_t c)
{
	uint8_t r = (uint8_t)(((c >> 11) & 0x1F) << 3);
	uint8_t g = (uint8_t)(((c >> 5) & 0x3F) << 2);
	uint8_t b = (uint8_t)((c & 0x1F) << 3);

	return lv_color_make((uint8_t)(r | (r >> 5)), (uint8_t)(g | (g >> 6)), (uint8_t)(b | (b >> 5)));
}

static void op_color(uint16_t rgb565)
{
	cur_color = art_color_from_565(rgb565);
}

static void op_cursor(int x, int y)
{
	cur_x = x;
	cur_y = y;
}

static void op_print(const char *s)
{
	lv_draw_label_dsc_t dsc;
	lv_area_t area;
	int len = (int)strlen(s);

	if (len == 0) {
		return;
	}
	lv_draw_label_dsc_init(&dsc);
	dsc.font = &lv_font_unscii_8;
	dsc.color = cur_color;
	dsc.text = s;
	dsc.text_local = 1;
	area.x1 = cur_x;
	area.y1 = cur_y;
	area.x2 = cur_x + len * ZBUDDY_ART_CHAR_W - 1;
	area.y2 = cur_y + ZBUDDY_ART_CHAR_H - 1;
	lv_draw_label(&layer, &dsc, &area);
	cur_x += len * ZBUDDY_ART_CHAR_W;
}

static const struct zbuddy_art_ops ops = {
	.set_color = op_color,
	.set_cursor = op_cursor,
	.print = op_print,
};

lv_obj_t *art_canvas_create(lv_obj_t *parent, int32_t scale)
{
	LV_DRAW_BUF_INIT_STATIC(art_buf);
	canvas = lv_canvas_create(parent);
	lv_canvas_set_draw_buf(canvas, &art_buf);
	lv_image_set_pivot(canvas, 0, 0);
	lv_image_set_scale(canvas, (uint32_t)LV_MAX(1, scale) * LV_SCALE_NONE);
	lv_image_set_antialias(canvas, false);
	lv_canvas_fill_bg(canvas, lv_color_black(), LV_OPA_COVER);
	zbuddy_art_bind(&ops);
	return canvas;
}

void art_canvas_render(const struct zbuddy_species *sp, enum zbuddy_persona s, uint32_t t,
		       uint16_t bg565)
{
	if (canvas == NULL) {
		return;
	}
	lv_canvas_fill_bg(canvas, art_color_from_565(bg565), LV_OPA_COVER);
	lv_canvas_init_layer(canvas, &layer);
	zbuddy_art_render(sp, s, t);
	lv_canvas_finish_layer(canvas, &layer);
	lv_obj_invalidate(canvas);
}
