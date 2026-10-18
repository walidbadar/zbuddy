/*
 * Copyright (c) 2026 Muhammad Waleed Badar
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * Character packs: a folder with manifest.json and one or more GIFs per
 * state (or, in text mode, short strings animated as frames).
 */
#ifndef ZBUDDY_CHARACTER_H
#define ZBUDDY_CHARACTER_H

#include <zbuddy/zbuddy.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ZBUDDY_CHAR_MAX_GIFS 32
#define ZBUDDY_CHAR_FILE_LEN 32
#define ZBUDDY_CHAR_TEXT_FRAMES 8
#define ZBUDDY_CHAR_TEXT_LEN 20

struct zbuddy_palette {
	uint16_t body;
	uint16_t bg;
	uint16_t text;
	uint16_t text_dim;
	uint16_t ink;
};

struct zbuddy_text_state {
	char frames[ZBUDDY_CHAR_TEXT_FRAMES][ZBUDDY_CHAR_TEXT_LEN];
	uint8_t n_frames;
	uint16_t delay_ms;
};

struct zbuddy_character {
	char name[ZBUDDY_NAME_LEN];
	struct zbuddy_palette palette;
	bool text_mode;
	/* GIF mode: files of each state are gifs[state_start[s] .. +state_count[s]) */
	char gifs[ZBUDDY_CHAR_MAX_GIFS][ZBUDDY_CHAR_FILE_LEN];
	uint8_t n_gifs;
	uint8_t state_start[ZBUDDY_PERSONA_COUNT];
	uint8_t state_count[ZBUDDY_PERSONA_COUNT];
	/* text mode */
	struct zbuddy_text_state text[ZBUDDY_PERSONA_COUNT];
};

/** The default palette (used before a pack is loaded and for ASCII pets). */
extern const struct zbuddy_palette zbuddy_default_palette;

/** Parse a manifest. @p json is modified in place. Returns 0 or -EINVAL. */
int zbuddy_character_parse(char *json, size_t len, struct zbuddy_character *out);

/** "#rrggbb" to RGB565; @p fallback when the string is missing or bad. */
uint16_t zbuddy_parse_hex_color(const char *s, uint16_t fallback);

#ifdef __cplusplus
}
#endif

#endif /* ZBUDDY_CHARACTER_H */
