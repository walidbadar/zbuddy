/*
 * Copyright (c) 2026 Muhammad Waleed Badar
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * Shared drawing helpers and the registry of the ASCII species.
 */
#include <zbuddy/art.h>

#include <string.h>

static const struct zbuddy_art_ops *ops;

void zbuddy_art_bind(const struct zbuddy_art_ops *canvas)
{
	ops = canvas;
}

void zbuddy_art_line(const char *line, int y_px, uint16_t color, int x_off)
{
	int len = (int)strlen(line);
	int w = len * ZBUDDY_ART_CHAR_W;
	int x = ZBUDDY_ART_X_CENTER - w / 2 + x_off;

	if (ops == NULL) {
		return;
	}
	ops->set_color(color);
	ops->set_cursor(x, y_px);
	ops->print(line);
}

void zbuddy_art_sprite(const char *const *lines, uint8_t n_lines, int y_offset, uint16_t color,
		       int x_off)
{
	for (uint8_t i = 0; i < n_lines; i++) {
		zbuddy_art_line(lines[i], ZBUDDY_ART_Y_BASE + y_offset + i * ZBUDDY_ART_CHAR_H, color,
				x_off);
	}
}

void zbuddy_art_cursor(int x, int y)
{
	if (ops != NULL) {
		ops->set_cursor(x, y);
	}
}

void zbuddy_art_color(uint16_t fg)
{
	if (ops != NULL) {
		ops->set_color(fg);
	}
}

void zbuddy_art_print(const char *s)
{
	if (ops != NULL) {
		ops->print(s);
	}
}

extern const struct zbuddy_species zbuddy_species_capybara;
extern const struct zbuddy_species zbuddy_species_duck;
extern const struct zbuddy_species zbuddy_species_goose;
extern const struct zbuddy_species zbuddy_species_blob;
extern const struct zbuddy_species zbuddy_species_cat;
extern const struct zbuddy_species zbuddy_species_dragon;
extern const struct zbuddy_species zbuddy_species_octopus;
extern const struct zbuddy_species zbuddy_species_owl;
extern const struct zbuddy_species zbuddy_species_penguin;
extern const struct zbuddy_species zbuddy_species_turtle;
extern const struct zbuddy_species zbuddy_species_snail;
extern const struct zbuddy_species zbuddy_species_ghost;
extern const struct zbuddy_species zbuddy_species_axolotl;
extern const struct zbuddy_species zbuddy_species_cactus;
extern const struct zbuddy_species zbuddy_species_robot;
extern const struct zbuddy_species zbuddy_species_rabbit;
extern const struct zbuddy_species zbuddy_species_mushroom;
extern const struct zbuddy_species zbuddy_species_chonk;
extern const struct zbuddy_species zbuddy_species_clawd;

static const struct zbuddy_species *const table[] = {
	&zbuddy_species_clawd,    &zbuddy_species_capybara, &zbuddy_species_duck,    &zbuddy_species_goose,
	&zbuddy_species_blob,     &zbuddy_species_cat,     &zbuddy_species_dragon,
	&zbuddy_species_octopus,  &zbuddy_species_owl,     &zbuddy_species_penguin,
	&zbuddy_species_turtle,   &zbuddy_species_snail,   &zbuddy_species_ghost,
	&zbuddy_species_axolotl,  &zbuddy_species_cactus,  &zbuddy_species_robot,
	&zbuddy_species_rabbit,   &zbuddy_species_mushroom, &zbuddy_species_chonk,
};

size_t zbuddy_species_count(void)
{
	return sizeof(table) / sizeof(table[0]);
}

const struct zbuddy_species *zbuddy_species_get(size_t idx)
{
	return idx < zbuddy_species_count() ? table[idx] : NULL;
}

int zbuddy_species_find(const char *name)
{
	for (size_t i = 0; i < zbuddy_species_count(); i++) {
		if (strcmp(table[i]->name, name) == 0) {
			return (int)i;
		}
	}
	return -1;
}

void zbuddy_art_render(const struct zbuddy_species *sp, enum zbuddy_persona s, uint32_t t)
{
	if (sp == NULL) {
		return;
	}
	if (s >= ZBUDDY_PERSONA_COUNT) {
		s = ZBUDDY_IDLE;
	}
	if (sp->states[s] != NULL) {
		sp->states[s](t);
	}
}
