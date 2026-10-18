/*
 * Copyright (c) 2026 Muhammad Waleed Badar
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * The installed character pack: GIFs (or text frames) per state, played
 * with LVGL's GIF widget from memory.
 */
#ifndef ZBUDDY_APP_CHARACTER_H
#define ZBUDDY_APP_CHARACTER_H

#include <zbuddy/character.h>
#include <lvgl.h>

/** Create the widgets (hidden until a pack is loaded). */
void character_create(lv_obj_t *parent);
/** Load the pack @p name, or the first installed one when NULL. */
bool character_init(const char *name);
bool character_loaded(void);
const char *character_name(void);
/** True while the built-in pack (not a pushed one) is loaded. */
bool character_builtin(void);
const struct zbuddy_palette *character_palette(void);
void character_close(void);
void character_set_state(enum zbuddy_persona s, uint32_t now_ms);
void character_tick(uint32_t now_ms);
/** Show or hide the widgets. */
void character_show(bool visible);

#endif
