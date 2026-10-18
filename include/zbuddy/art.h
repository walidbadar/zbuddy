/*
 * Copyright (c) 2026 Muhammad Waleed Badar
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * ASCII species: eighteen pets with seven animations each, drawn on a
 * small text canvas of fixed-width 8x8 glyphs that the application
 * provides (an LVGL canvas, a terminal, a test buffer ...).
 */
#ifndef ZBUDDY_ART_H
#define ZBUDDY_ART_H

#include <zbuddy/zbuddy.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Canvas geometry in pixels at 1x (the application may scale the whole canvas) */
#define ZBUDDY_ART_CANVAS_W 160
#define ZBUDDY_ART_CANVAS_H 88
#define ZBUDDY_ART_X_CENTER (ZBUDDY_ART_CANVAS_W / 2)
#define ZBUDDY_ART_Y_BASE 30
#define ZBUDDY_ART_Y_OVERLAY 6
#define ZBUDDY_ART_CHAR_W 8
#define ZBUDDY_ART_CHAR_H 8

/* Colours are RGB565 like the original art */
#define ZBUDDY_ART_BG 0x0000
#define ZBUDDY_ART_HEART 0xF810
#define ZBUDDY_ART_DIM 0x8410
#define ZBUDDY_ART_YEL 0xFFE0
#define ZBUDDY_ART_WHITE 0xFFFF
#define ZBUDDY_ART_CYAN 0x07FF
#define ZBUDDY_ART_GREEN 0x07E0
#define ZBUDDY_ART_PURPLE 0xA01F
#define ZBUDDY_ART_RED 0xF800
#define ZBUDDY_ART_BLUE 0x041F

/** The text canvas the species draw on. */
struct zbuddy_art_ops {
	void (*set_color)(uint16_t rgb565);
	void (*set_cursor)(int x, int y);
	/** Print at the cursor, advancing it by ZBUDDY_ART_CHAR_W per glyph. */
	void (*print)(const char *s);
};

typedef void (*zbuddy_state_fn)(uint32_t t);

struct zbuddy_species {
	const char *name;
	uint16_t body_color;
	zbuddy_state_fn states[ZBUDDY_PERSONA_COUNT];
};

size_t zbuddy_species_count(void);
const struct zbuddy_species *zbuddy_species_get(size_t idx);
int zbuddy_species_find(const char *name);

/** Bind the canvas; must be done before rendering. */
void zbuddy_art_bind(const struct zbuddy_art_ops *ops);

/** Draw one animation frame of @p sp in state @p s at tick @p t (5 ticks
 * per second). The caller clears the canvas first.
 */
void zbuddy_art_render(const struct zbuddy_species *sp, enum zbuddy_persona s, uint32_t t);

/* Helpers used by the species files */
void zbuddy_art_line(const char *line, int y_px, uint16_t color, int x_off);
void zbuddy_art_sprite(const char *const *lines, uint8_t n_lines, int y_offset, uint16_t color,
		       int x_off);
void zbuddy_art_cursor(int x, int y);
void zbuddy_art_color(uint16_t fg);
void zbuddy_art_print(const char *s);

#ifdef __cplusplus
}
#endif

#endif /* ZBUDDY_ART_H */
