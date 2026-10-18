/*
 * Copyright (c) 2026 Muhammad Waleed Badar
 * SPDX-License-Identifier: GPL-3.0-only
 */
#include "character.h"
#include "art_canvas.h"
#include "storage.h"
#if defined(CONFIG_ZBUDDY_APP_BUILTIN_CHARACTER)
#include "builtin_pack.h"
#endif

#include <string.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(character, LOG_LEVEL_INF);

#define VARIANT_DWELL_MS 5000
#define ANIM_PAUSE_MS 800

struct gif_blob {
	void *data;   /* owned (storage_free) ... */
	const void *rodata; /* ... or built into the image */
	size_t len;
	lv_image_dsc_t dsc;
};

static struct zbuddy_character ch;
static struct gif_blob gifs[ZBUDDY_CHAR_MAX_GIFS];
static char cur_name[ZBUDDY_NAME_LEN];
static bool loaded;
static lv_obj_t *gif_obj;
static lv_obj_t *text_obj;
static uint8_t cur_state = 0xFF;
static uint8_t rot[ZBUDDY_PERSONA_COUNT];
static uint32_t variant_started_ms;
static uint32_t pause_until_ms;
static uint8_t text_frame;
static uint32_t text_next_ms;
static bool visible;
static bool builtin;

#if defined(CONFIG_ZBUDDY_APP_BUILTIN_CHARACTER)
static const struct builtin_file *builtin_find(const char *name)
{
	for (size_t i = 0; i < builtin_pack_count; i++) {
		if (strcmp(builtin_pack_files[i].name, name) == 0) {
			return &builtin_pack_files[i];
		}
	}
	return NULL;
}
#endif

void character_create(lv_obj_t *parent)
{
	gif_obj = lv_gif_create(parent);
	lv_image_set_antialias(gif_obj, false);
	lv_image_set_pivot(gif_obj, 0, 0);
	lv_obj_set_hidden(gif_obj, true);

	text_obj = lv_label_create(parent);
	lv_obj_set_style_text_font(text_obj, &lv_font_unscii_16, 0);
	lv_obj_set_style_text_align(text_obj, LV_TEXT_ALIGN_CENTER, 0);
	lv_obj_set_width(text_obj, LV_PCT(100));
	lv_obj_align(text_obj, LV_ALIGN_CENTER, 0, 0);
	lv_label_set_text(text_obj, "");
	lv_obj_set_hidden(text_obj, true);
}

static void free_gifs(void)
{
	for (int i = 0; i < ZBUDDY_CHAR_MAX_GIFS; i++) {
		if (gifs[i].data != NULL) {
			storage_free(gifs[i].data);
		}
		gifs[i].data = NULL;
		gifs[i].rodata = NULL;
	}
}

static void set_blob(struct gif_blob *g, const void *data, size_t len)
{
	memset(&g->dsc, 0, sizeof(g->dsc));
	g->len = len;
	g->dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
	g->dsc.header.cf = LV_COLOR_FORMAT_RAW;
	g->dsc.data = data;
	g->dsc.data_size = (uint32_t)len;
}

#if defined(CONFIG_ZBUDDY_APP_BUILTIN_CHARACTER)
static bool init_builtin(void)
{
	const struct builtin_file *mf = builtin_find("manifest.json");
	char manifest[1024];

	if (mf == NULL || mf->len >= sizeof(manifest)) {
		return false;
	}
	memcpy(manifest, mf->data, mf->len);
	manifest[mf->len] = '\0';
	if (zbuddy_character_parse(manifest, mf->len, &ch) != 0) {
		return false;
	}
	for (uint8_t i = 0; i < ch.n_gifs && !ch.text_mode; i++) {
		const struct builtin_file *f = builtin_find(ch.gifs[i]);

		if (f != NULL) {
			gifs[i].rodata = f->data;
			set_blob(&gifs[i], f->data, f->len);
		}
	}
	strncpy(cur_name, builtin_pack_name, sizeof(cur_name) - 1);
	cur_name[sizeof(cur_name) - 1] = '\0';
	memset(rot, 0, sizeof(rot));
	loaded = true;
	builtin = true;
	LOG_INF("built-in character '%s' (%u gifs)", ch.name, ch.n_gifs);
	return true;
}
#endif

/* lv_gif_set_src() resumes the frame timer before it opens the new source
 * and leaves it running when that fails (or the source is NULL); the next
 * tick then plays a closed GIF through its NULL read callback. Pause the
 * timer whenever no GIF is open.
 */
static bool gif_set_src(const lv_image_dsc_t *dsc)
{
	lv_gif_set_src(gif_obj, dsc);
	if (dsc == NULL || !lv_gif_is_loaded(gif_obj)) {
		lv_gif_pause(gif_obj);
		return false;
	}
	return true;
}

void character_close(void)
{
	if (gif_obj != NULL && loaded && !ch.text_mode) {
		gif_set_src(NULL);
	}
	loaded = false;
	cur_state = 0xFF;
	free_gifs();
	character_show(false);
}

bool character_init(const char *name)
{
	char scanned[ZBUDDY_NAME_LEN];
	void *manifest;
	size_t len;

	character_close();
	if (name == NULL && (!storage_ready() || !storage_first_character(scanned, sizeof(scanned)))) {
		LOG_INF("no character installed");
#if defined(CONFIG_ZBUDDY_APP_BUILTIN_CHARACTER)
		return init_builtin();
#else
		return false;
#endif
	}
	if (!storage_ready()) {
		return false;
	}
	if (name == NULL) {
		name = scanned;
	}
	if (storage_read_file(name, "manifest.json", &manifest, &len) != 0) {
		LOG_WRN("no manifest in '%s'", name);
		return false;
	}
	if (zbuddy_character_parse(manifest, len, &ch) != 0) {
		LOG_WRN("bad manifest in '%s'", name);
		storage_free(manifest);
		return false;
	}
	storage_free(manifest);
	strncpy(cur_name, name, sizeof(cur_name) - 1);
	cur_name[sizeof(cur_name) - 1] = '\0';
	if (!ch.text_mode) {
		for (uint8_t i = 0; i < ch.n_gifs; i++) {
			struct gif_blob *g = &gifs[i];

			if (storage_read_file(name, ch.gifs[i], &g->data, &g->len) != 0) {
				LOG_WRN("missing %s", ch.gifs[i]);
				continue;
			}
			set_blob(g, g->data, g->len);
		}
	}
	memset(rot, 0, sizeof(rot));
	loaded = true;
	builtin = false;
	LOG_INF("character '%s' (%s, %u gifs)", ch.name, ch.text_mode ? "text" : "gif", ch.n_gifs);
	return true;
}

bool character_loaded(void)
{
	return loaded;
}

const char *character_name(void)
{
	return loaded ? ch.name : "";
}

bool character_builtin(void)
{
	return loaded && builtin;
}

const struct zbuddy_palette *character_palette(void)
{
	return loaded ? &ch.palette : &zbuddy_default_palette;
}

void character_show(bool on)
{
	visible = on;
	if (gif_obj == NULL) {
		return;
	}
	if (on && loaded && !ch.text_mode) {
		lv_obj_set_hidden(gif_obj, false);
	} else {
		lv_obj_set_hidden(gif_obj, true);
	}
	if (on && loaded && ch.text_mode) {
		lv_obj_set_hidden(text_obj, false);
	} else {
		lv_obj_set_hidden(text_obj, true);
	}
}

static void open_variant(uint32_t now_ms)
{
	uint8_t idx = ch.state_start[cur_state] + rot[cur_state];
	struct gif_blob *g = &gifs[idx];

	if (g->dsc.data == NULL) {
		return;
	}
	if (!gif_set_src(&g->dsc)) {
		/* a broken GIF: try the next variant after the usual dwell */
		variant_started_ms = now_ms;
		pause_until_ms = 0;
		return;
	}
	/* a single GIF plays once and freezes on its last frame; the variants
	 * of a rotation loop for a while, then the next one is shown */
	lv_gif_set_loop_count(gif_obj, ch.state_count[cur_state] == 1 ? 1 : -1);
	/* the largest whole pixel scale that fits the pet area, centred */
	lv_obj_update_layout(gif_obj);
	{
		lv_obj_t *parent = lv_obj_get_parent(gif_obj);
		int32_t pw = lv_obj_get_width(parent), ph = lv_obj_get_height(parent);
		int32_t w = lv_obj_get_width(gif_obj), h = lv_obj_get_height(gif_obj);
		int32_t k = (w > 0 && h > 0) ? LV_MAX(1, LV_MIN(pw / w, ph / h)) : 1;

		lv_image_set_scale(gif_obj, (uint32_t)k * LV_SCALE_NONE);
		lv_obj_set_pos(gif_obj, (pw - k * w) / 2, (ph - k * h) / 2);
	}
	variant_started_ms = now_ms;
	pause_until_ms = 0;
	lv_obj_set_style_bg_color(gif_obj, art_color_from_565(ch.palette.bg), 0);
}

void character_set_state(enum zbuddy_persona s, uint32_t now_ms)
{
	if (!loaded || s >= ZBUDDY_PERSONA_COUNT || s == cur_state) {
		return;
	}
	cur_state = (uint8_t)s;
	if (ch.text_mode) {
		text_frame = 0;
		text_next_ms = 0;
		lv_obj_set_style_text_color(text_obj, art_color_from_565(ch.palette.body), 0);
		return;
	}
	if (ch.state_count[s] == 0) {
		LOG_DBG("no gif for state %d", s);
		return;
	}
	open_variant(now_ms);
}

void character_tick(uint32_t now_ms)
{
	if (!loaded || cur_state == 0xFF) {
		return;
	}
	if (ch.text_mode) {
		struct zbuddy_text_state *ts = &ch.text[cur_state];

		if (ts->n_frames == 0 || (int32_t)(now_ms - text_next_ms) < 0) {
			return;
		}
		text_next_ms = now_ms + ts->delay_ms;
		lv_label_set_text(text_obj, ts->frames[text_frame]);
		text_frame = (uint8_t)((text_frame + 1) % ts->n_frames);
		return;
	}
	if (ch.state_count[cur_state] <= 1) {
		return;
	}
	if (pause_until_ms != 0) {
		if ((int32_t)(now_ms - pause_until_ms) >= 0) {
			rot[cur_state] = (uint8_t)((rot[cur_state] + 1) % ch.state_count[cur_state]);
			open_variant(now_ms);
		}
		return;
	}
	if (now_ms - variant_started_ms >= VARIANT_DWELL_MS) {
		lv_gif_pause(gif_obj);
		pause_until_ms = now_ms + ANIM_PAUSE_MS;
	}
}
