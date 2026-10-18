/*
 * Copyright (c) 2026 Muhammad Waleed Badar
 * SPDX-License-Identifier: GPL-3.0-only
 */
#include <zbuddy/character.h>
#include "zbuddy_json.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

const struct zbuddy_palette zbuddy_default_palette = {
	.body = 0xC2A6, .bg = 0x0000, .text = 0xFFFF, .text_dim = 0x8410, .ink = 0x0000,
};

uint16_t zbuddy_parse_hex_color(const char *s, uint16_t fallback)
{
	char *end;
	unsigned long v;

	if (s == NULL || *s == '\0') {
		return fallback;
	}
	if (*s == '#') {
		s++;
	}
	v = strtoul(s, &end, 16);
	if (end == s) {
		return fallback;
	}
	return (uint16_t)(((v >> 19) & 0x1F) << 11 | ((v >> 10) & 0x3F) << 5 | ((v >> 3) & 0x1F));
}

static const char *const state_names[ZBUDDY_PERSONA_COUNT] = {
	"sleep", "idle", "busy", "attention", "celebrate", "dizzy", "heart",
};

static bool take_color(const struct zbuddy_jkv *kv, void *ctx)
{
	struct zbuddy_palette *pal = ctx;
	char hex[12];
	uint16_t *dst = NULL;

	if (zbuddy_json_key_is(kv, "body")) {
		dst = &pal->body;
	} else if (zbuddy_json_key_is(kv, "bg")) {
		dst = &pal->bg;
	} else if (zbuddy_json_key_is(kv, "text")) {
		dst = &pal->text;
	} else if (zbuddy_json_key_is(kv, "textDim")) {
		dst = &pal->text_dim;
	} else if (zbuddy_json_key_is(kv, "ink")) {
		dst = &pal->ink;
	}
	if (dst != NULL) {
		zbuddy_json_str(&kv->value, hex, sizeof(hex));
		*dst = zbuddy_parse_hex_color(hex, *dst);
	}
	return true;
}

struct state_ctx {
	struct zbuddy_character *ch;
	int state;
};

static bool take_gif(const struct zbuddy_jtok *elem, size_t idx, void *ctx)
{
	struct state_ctx *sc = ctx;
	struct zbuddy_character *ch = sc->ch;

	(void)idx;
	if (ch->n_gifs >= ZBUDDY_CHAR_MAX_GIFS || elem->type != ZBUDDY_J_STRING) {
		return false;
	}
	zbuddy_json_str(elem, ch->gifs[ch->n_gifs], ZBUDDY_CHAR_FILE_LEN);
	ch->n_gifs++;
	ch->state_count[sc->state]++;
	return true;
}

static bool take_frame(const struct zbuddy_jtok *elem, size_t idx, void *ctx)
{
	struct zbuddy_text_state *ts = ctx;

	(void)idx;
	if (ts->n_frames >= ZBUDDY_CHAR_TEXT_FRAMES) {
		return false;
	}
	zbuddy_json_str(elem, ts->frames[ts->n_frames], ZBUDDY_CHAR_TEXT_LEN);
	ts->n_frames++;
	return true;
}

static bool take_text_member(const struct zbuddy_jkv *kv, void *ctx)
{
	struct zbuddy_text_state *ts = ctx;

	if (zbuddy_json_key_is(kv, "delay")) {
		ts->delay_ms = (uint16_t)zbuddy_json_int(&kv->value, 200);
	} else if (zbuddy_json_key_is(kv, "frames") && kv->value.type == ZBUDDY_J_ARRAY) {
		zbuddy_json_arr_each_tok(&kv->value, take_frame, ts);
	}
	return true;
}

static bool take_state(const struct zbuddy_jkv *kv, void *ctx)
{
	struct zbuddy_character *ch = ctx;
	struct state_ctx sc = {.ch = ch, .state = -1};

	for (int i = 0; i < ZBUDDY_PERSONA_COUNT; i++) {
		if (zbuddy_json_key_is(kv, state_names[i])) {
			sc.state = i;
			break;
		}
	}
	if (sc.state < 0) {
		return true;
	}
	if (ch->text_mode) {
		struct zbuddy_text_state *ts = &ch->text[sc.state];

		ts->n_frames = 0;
		ts->delay_ms = 200;
		if (kv->value.type == ZBUDDY_J_OBJECT) {
			zbuddy_json_obj_each_tok(&kv->value, take_text_member, ts);
		}
		return true;
	}
	ch->state_start[sc.state] = ch->n_gifs;
	ch->state_count[sc.state] = 0;
	if (kv->value.type == ZBUDDY_J_ARRAY) {
		zbuddy_json_arr_each_tok(&kv->value, take_gif, &sc);
	} else if (kv->value.type == ZBUDDY_J_STRING) {
		take_gif(&kv->value, 0, &sc);
	}
	return true;
}

struct manifest_ctx {
	struct zbuddy_character *ch;
	struct zbuddy_jtok states;
};

static bool take_manifest(const struct zbuddy_jkv *kv, void *ctx)
{
	struct manifest_ctx *m = ctx;

	if (zbuddy_json_key_is(kv, "name")) {
		zbuddy_json_str(&kv->value, m->ch->name, sizeof(m->ch->name));
	} else if (zbuddy_json_key_is(kv, "colors") && kv->value.type == ZBUDDY_J_OBJECT) {
		zbuddy_json_obj_each_tok(&kv->value, take_color, &m->ch->palette);
	} else if (zbuddy_json_key_is(kv, "mode")) {
		m->ch->text_mode = zbuddy_json_str_is(&kv->value, "text");
	} else if (zbuddy_json_key_is(kv, "states")) {
		m->states = kv->value;
	}
	return true;
}

int zbuddy_character_parse(char *json, size_t len, struct zbuddy_character *out)
{
	struct manifest_ctx m = {.ch = out, .states = {.type = ZBUDDY_J_NONE}};

	memset(out, 0, sizeof(*out));
	out->palette = zbuddy_default_palette;
	if (zbuddy_json_obj_each(json, len, take_manifest, &m) < 0) {
		return -EINVAL;
	}
	/* The states come after "mode", whatever the manifest order */
	if (m.states.type == ZBUDDY_J_OBJECT) {
		zbuddy_json_obj_each_tok(&m.states, take_state, out);
	}
	if (!out->text_mode && out->n_gifs == 0) {
		return -EINVAL;
	}
	return 0;
}
