/*
 * Copyright (c) 2026 Muhammad Waleed Badar
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * The screens, laid out from the display's resolution: a title row, the
 * pet, the HUD card (transcript bubbles, approval, pet and info pages,
 * clock) and a status row. On a portrait display the HUD is under the
 * pet, on a landscape one beside it; the pet area takes a quarter of the
 * screen and the pet is drawn at the largest whole pixel scale that fits
 * it. The menus and the pairing passkey are overlays on a dimmed backdrop. The accent colour is the pet's body colour.
 * Touch: tap the pet for the next screen, hold for the menu, swipe it to
 * make it dizzy, tap a page to flip it.
 */
#include "ui.h"
#include "art_canvas.h"
#include "bridge.h"
#include "buddy.h"
#include "character.h"
#include "clock.h"
#include "storage.h"

#include <stdio.h>
#include <string.h>
#include <zbuddy/stats.h>
#include <zephyr/kernel.h>
#include <lvgl.h>

#define OVERLAY_MAX 10
#define KV_ROWS 9

#define COL_HOT lv_color_hex(0xFF5533)
#define COL_GREEN lv_color_hex(0x3DDC84)
#define COL_YELLOW lv_color_hex(0xFFD23F)
#define COL_CYAN lv_color_hex(0x4FD6FF)
#define COL_PINK lv_color_hex(0xFF5A8C)
#define COL_CARD lv_color_hex(0x161616)
#define COL_CARD2 lv_color_hex(0x242424)
#define COL_LINE lv_color_hex(0x2E2E2E)
#define COL_DIM lv_color_hex(0x8C8C8C)
#define COL_TEXT lv_color_hex(0xF2F2F2)

/* --- layout from the resolution --------------------------------------------- */

static struct {
	int32_t w, h;
	bool landscape;
	bool compact;
	int32_t top_h, bottom_h;
	int32_t pet_x, pet_y, pet_w, pet_h;
	int32_t hud_x, hud_y, hud_w, hud_h; /* the card, margins already taken */
	int32_t scale;
	int32_t pad;
	int32_t row;
	int32_t item_h;
	int32_t radius;
} lay;

static const lv_font_t *f_small, *f_body, *f_title, *f_big, *f_clock;

static void layout_compute(void)
{
	int32_t avail_w, avail_h, margin;

	lay.w = lv_display_get_horizontal_resolution(NULL);
	lay.h = lv_display_get_vertical_resolution(NULL);
	lay.landscape = lay.w > lay.h;
	lay.compact = LV_MIN(lay.w, lay.h) < 300 || LV_MAX(lay.w, lay.h) < 400;

	if (lay.compact) {
		f_small = &lv_font_montserrat_12;
		f_body = &lv_font_montserrat_12;
		f_title = &lv_font_montserrat_14;
		f_big = &lv_font_montserrat_20;
		f_clock = &lv_font_montserrat_28;
	} else {
		f_small = &lv_font_montserrat_12;
		f_body = &lv_font_montserrat_14;
		f_title = &lv_font_montserrat_16;
		f_big = &lv_font_montserrat_28;
		f_clock = &lv_font_montserrat_48;
	}
	lay.top_h = lv_font_get_line_height(f_title) + 14;
	lay.bottom_h = lv_font_get_line_height(f_small) + 16;
	lay.pad = lay.compact ? 8 : 12;
	lay.row = lv_font_get_line_height(f_body) + 4;
	lay.item_h = lay.compact ? 30 : 36;
	lay.radius = lay.compact ? 10 : 14;
	margin = lay.compact ? 6 : 8;
	avail_w = lay.w;
	avail_h = lay.h - lay.top_h - lay.bottom_h;

	if (lay.landscape) {
		lay.scale = LV_MAX(1, LV_MIN((lay.w / 4 - 2 * lay.pad) / ZBUDDY_ART_CANVAS_W,
					     avail_h / ZBUDDY_ART_CANVAS_H));
		lay.pet_x = 0;
		lay.pet_y = lay.top_h;
		lay.pet_w = LV_MAX(lay.w / 4, ZBUDDY_ART_CANVAS_W * lay.scale + 2 * lay.pad);
		lay.pet_h = avail_h;
		lay.hud_x = lay.pet_w;
		lay.hud_y = lay.top_h + margin;
		lay.hud_w = avail_w - lay.pet_w - margin;
		lay.hud_h = avail_h - 2 * margin;
	} else {
		lay.scale = LV_MAX(1, LV_MIN(avail_w / ZBUDDY_ART_CANVAS_W,
					     (lay.h / 4 - 2 * lay.pad) / ZBUDDY_ART_CANVAS_H));
		lay.pet_x = 0;
		lay.pet_y = lay.top_h;
		lay.pet_w = avail_w;
		lay.pet_h = LV_MAX(lay.h / 4, ZBUDDY_ART_CANVAS_H * lay.scale + 2 * lay.pad);
		lay.hud_x = margin;
		lay.hud_y = lay.pet_y + lay.pet_h;
		lay.hud_w = avail_w - 2 * margin;
		lay.hud_h = avail_h - lay.pet_h - margin;
	}
}

static lv_obj_t *scr;
static lv_obj_t *top_name, *top_pill, *top_pill_lbl, *top_menu;
static lv_obj_t *pet_area, *canvas, *pet_msg, *install_bar;
static lv_obj_t *hud;
static lv_obj_t *transcript, *bubbles[CONFIG_ZBUDDY_LINES], *bubble_lbl[CONFIG_ZBUDDY_LINES];
static lv_obj_t *waiting, *wait_icon, *wait_title, *wait_sub;
static lv_obj_t *approval, *ap_head, *ap_timer, *ap_tool, *ap_hint, *ap_ok, *ap_no, *ap_sent;
static lv_obj_t *page, *page_title, *page_counter, *page_body;
static lv_obj_t *kv_key[KV_ROWS], *kv_val[KV_ROWS];
static lv_obj_t *stat_rows[3], *stat_cells[3][10], *stat_val[3], *stat_badge, *fed_bar;
static lv_obj_t *clock_panel, *clk_time, *clk_sec, *clk_date;
static lv_obj_t *bottom_left, *bottom_pill, *bottom_pill_lbl;
static lv_obj_t *scrim, *overlay, *ov_title, *ov_btn[OVERLAY_MAX], *ov_lbl[OVERLAY_MAX];
static lv_obj_t *passkey_panel, *pk_digits, *pk_name;

static uint32_t drawn_anim_gen = UINT32_MAX;
static uint32_t anim_tick, next_tick_ms;
static uint16_t drawn_line_gen = UINT16_MAX;
static uint16_t cur_bg = 0xFFFF, cur_body = 0xFFFF;
static lv_color_t accent;
static int32_t stats_body_y;

/* --- helpers ----------------------------------------------------------------- */

static void set_text(lv_obj_t *lbl, const char *txt)
{
	if (strcmp(lv_label_get_text(lbl), txt) != 0) {
		lv_label_set_text(lbl, txt);
	}
}

static void show(lv_obj_t *obj, bool on)
{
	if (on == lv_obj_is_hidden(obj)) {
		lv_obj_set_hidden(obj, !on);
	}
}

static lv_obj_t *label(lv_obj_t *parent, const lv_font_t *font, lv_color_t color)
{
	lv_obj_t *l = lv_label_create(parent);

	lv_obj_set_style_text_font(l, font, 0);
	lv_obj_set_style_text_color(l, color, 0);
	lv_label_set_text(l, "");
	return l;
}

static lv_obj_t *panel(lv_obj_t *parent, int32_t x, int32_t y, int32_t w, int32_t h)
{
	lv_obj_t *p = lv_obj_create(parent);

	lv_obj_remove_style_all(p);
	lv_obj_set_pos(p, x, y);
	lv_obj_set_size(p, w, h);
	lv_obj_set_scrollable(p, false);
	return p;
}

static void card(lv_obj_t *obj, lv_color_t bg, int32_t radius)
{
	lv_obj_set_style_bg_color(obj, bg, 0);
	lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
	lv_obj_set_style_radius(obj, radius, 0);
}

/* A rounded chip with a label inside; the label is returned through @p lbl */
static lv_obj_t *pill(lv_obj_t *parent, const lv_font_t *font, lv_obj_t **lbl)
{
	lv_obj_t *p = lv_obj_create(parent);

	lv_obj_remove_style_all(p);
	card(p, COL_CARD2, LV_RADIUS_CIRCLE);
	lv_obj_set_style_pad_hor(p, 10, 0);
	lv_obj_set_style_pad_ver(p, 4, 0);
	lv_obj_set_size(p, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
	lv_obj_set_scrollable(p, false);
	*lbl = label(p, font, COL_DIM);
	return p;
}

static uint32_t now_ms(void)
{
	return k_uptime_get_32();
}

/* --- events ------------------------------------------------------------------ */

static void pet_event(lv_event_t *e)
{
	switch (lv_event_get_code(e)) {
	case LV_EVENT_SHORT_CLICKED:
		buddy_tap_pet(now_ms());
		break;
	case LV_EVENT_LONG_PRESSED:
		buddy_long_press(now_ms());
		break;
	case LV_EVENT_GESTURE:
		buddy_shake(now_ms());
		break;
	default:
		break;
	}
}

static void hud_event(lv_event_t *e)
{
	switch (lv_event_get_code(e)) {
	case LV_EVENT_SHORT_CLICKED:
		buddy_next_page(now_ms());
		break;
	case LV_EVENT_LONG_PRESSED:
		buddy_long_press(now_ms());
		break;
	default:
		break;
	}
}

static void menu_button_event(lv_event_t *e)
{
	ARG_UNUSED(e);
	buddy_long_press(now_ms());
}

static void approve_event(lv_event_t *e)
{
	ARG_UNUSED(e);
	buddy_approve(now_ms());
}

static void deny_event(lv_event_t *e)
{
	ARG_UNUSED(e);
	buddy_deny(now_ms());
}

static void overlay_event(lv_event_t *e)
{
	uintptr_t idx = (uintptr_t)lv_event_get_user_data(e);

	buddy_pick((uint8_t)idx, now_ms());
}

static void scrim_event(lv_event_t *e)
{
	ARG_UNUSED(e);
	/* a tap outside the menu closes it */
	if (buddy_model()->overlay != OVERLAY_NONE) {
		buddy_pick(buddy_overlay_items() - 1, now_ms());
	}
}

static void wake_event(lv_event_t *e)
{
	ARG_UNUSED(e);
	buddy_wake(now_ms());
}

/* --- construction ------------------------------------------------------------ */

static void build_top(void)
{
	lv_obj_t *top = panel(scr, 0, 0, lay.w, lay.top_h);
	int32_t menu_w = lv_font_get_line_height(f_big) + 8;

	top_name = label(top, f_title, COL_TEXT);
	lv_obj_set_width(top_name, lay.w / 2);
	lv_label_set_long_mode(top_name, LV_LABEL_LONG_DOT);
	lv_obj_align(top_name, LV_ALIGN_LEFT_MID, 10, 0);
	top_pill = pill(top, f_small, &top_pill_lbl);
	lv_obj_align(top_pill, LV_ALIGN_RIGHT_MID, -menu_w - 8, 0);
	top_menu = label(top, f_big, COL_DIM);
	lv_label_set_text(top_menu, LV_SYMBOL_LIST);
	lv_obj_align(top_menu, LV_ALIGN_RIGHT_MID, -10, 0);
	lv_obj_set_clickable(top_menu, true);
	lv_obj_set_ext_click_area(top_menu, 14);
	lv_obj_add_event_cb(top_menu, menu_button_event, LV_EVENT_CLICKED, NULL);
}

static void build_pet(void)
{
	pet_area = panel(scr, lay.pet_x, lay.pet_y, lay.pet_w, lay.pet_h);
	lv_obj_set_clickable(pet_area, true);
	lv_obj_add_event_cb(pet_area, pet_event, LV_EVENT_ALL, NULL);

	canvas = art_canvas_create(pet_area, lay.scale);
	lv_obj_set_pos(canvas, (lay.pet_w - ZBUDDY_ART_CANVAS_W * lay.scale) / 2,
		       (lay.pet_h - ZBUDDY_ART_CANVAS_H * lay.scale) / 2);
	lv_obj_set_clickable(canvas, false);

	character_create(pet_area);

	pet_msg = label(pet_area, f_body, COL_DIM);
	lv_obj_set_style_text_align(pet_msg, LV_TEXT_ALIGN_CENTER, 0);
	lv_obj_set_width(pet_msg, lay.pet_w - 2 * lay.pad);
	lv_obj_align(pet_msg, LV_ALIGN_CENTER, 0, -lay.row);
	install_bar = lv_bar_create(pet_area);
	lv_obj_set_size(install_bar, lay.pet_w * 3 / 4, 8);
	lv_obj_align(install_bar, LV_ALIGN_CENTER, 0, lay.row);
	lv_bar_set_range(install_bar, 0, 100);
	lv_obj_set_style_bg_color(install_bar, COL_CARD2, 0);
	lv_obj_set_hidden(install_bar, true);
}

static void build_transcript(void)
{
	transcript = lv_obj_create(hud);
	lv_obj_remove_style_all(transcript);
	lv_obj_set_pos(transcript, 0, 0);
	lv_obj_set_size(transcript, lay.hud_w, lay.hud_h);
	lv_obj_set_style_pad_all(transcript, lay.pad - 2, 0);
	lv_obj_set_scrollbar_mode(transcript, LV_SCROLLBAR_MODE_OFF);
	lv_obj_set_scroll_dir(transcript, LV_DIR_VER);
	for (int i = 0; i < CONFIG_ZBUDDY_LINES; i++) {
		bubbles[i] = lv_obj_create(transcript);
		lv_obj_remove_style_all(bubbles[i]);
		card(bubbles[i], COL_CARD2, lay.radius);
		lv_obj_set_style_pad_hor(bubbles[i], 10, 0);
		lv_obj_set_style_pad_ver(bubbles[i], 6, 0);
		lv_obj_set_size(bubbles[i], LV_SIZE_CONTENT, LV_SIZE_CONTENT);
		lv_obj_set_scrollable(bubbles[i], false);
		bubble_lbl[i] = label(bubbles[i], f_body, COL_TEXT);
		lv_label_set_long_mode(bubble_lbl[i], LV_LABEL_LONG_WRAP);
		lv_obj_set_hidden(bubbles[i], true);
	}

	/* nothing to show yet: a hint with the device name */
	waiting = panel(hud, 0, 0, lay.hud_w, lay.hud_h);
	wait_icon = label(waiting, f_big, COL_DIM);
	lv_label_set_text(wait_icon, LV_SYMBOL_BLUETOOTH);
	lv_obj_align(wait_icon, LV_ALIGN_CENTER, 0, -lay.row * 2);
	wait_title = label(waiting, f_title, COL_TEXT);
	lv_obj_align(wait_title, LV_ALIGN_CENTER, 0, 0);
	wait_sub = label(waiting, f_small, COL_DIM);
	lv_obj_set_style_text_align(wait_sub, LV_TEXT_ALIGN_CENTER, 0);
	lv_obj_set_width(wait_sub, lay.hud_w - 2 * lay.pad);
	lv_obj_align(wait_sub, LV_ALIGN_CENTER, 0, lay.row + 4);
}

static lv_obj_t *button(lv_obj_t *parent, const char *txt, lv_color_t bg, lv_color_t fg,
			int32_t w, lv_event_cb_t cb)
{
	lv_obj_t *b = lv_button_create(parent);
	lv_obj_t *l = lv_label_create(b);

	lv_obj_set_style_bg_color(b, bg, 0);
	lv_obj_set_style_bg_color(b, lv_color_darken(bg, LV_OPA_30), LV_STATE_PRESSED);
	lv_obj_set_style_radius(b, lay.radius, 0);
	lv_obj_set_style_shadow_width(b, 0, 0);
	lv_label_set_text(l, txt);
	/* the big font only where the symbol and the word fit */
	lv_obj_set_style_text_font(l, (lay.compact || w < 150) ? f_title : &lv_font_montserrat_20, 0);
	lv_obj_set_style_text_color(l, fg, 0);
	lv_obj_center(l);
	lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, NULL);
	return b;
}

static void build_approval(void)
{
	int32_t btn_h = LV_CLAMP(lay.hud_h / 4, 40, 60);
	int32_t btn_w = (lay.hud_w - 3 * lay.pad) / 2;
	int32_t y = lay.pad;

	approval = panel(hud, 0, 0, lay.hud_w, lay.hud_h);
	ap_head = label(approval, f_small, COL_DIM);
	lv_obj_set_pos(ap_head, lay.pad, y);
	ap_timer = label(approval, f_small, COL_DIM);
	lv_obj_align(ap_timer, LV_ALIGN_TOP_RIGHT, -lay.pad, y);
	y += lay.row + 2;
	ap_tool = label(approval, f_big, COL_TEXT);
	lv_obj_set_pos(ap_tool, lay.pad, y);
	lv_obj_set_width(ap_tool, lay.hud_w - 2 * lay.pad);
	lv_label_set_long_mode(ap_tool, LV_LABEL_LONG_DOT);
	y += lv_font_get_line_height(f_big) + 8;
	ap_hint = label(approval, f_body, COL_TEXT);
	lv_obj_set_pos(ap_hint, lay.pad, y);
	lv_obj_set_width(ap_hint, lay.hud_w - 2 * lay.pad);
	lv_label_set_long_mode(ap_hint, LV_LABEL_LONG_WRAP);
	lv_obj_set_style_bg_color(ap_hint, COL_CARD2, 0);
	lv_obj_set_style_bg_opa(ap_hint, LV_OPA_COVER, 0);
	lv_obj_set_style_radius(ap_hint, 8, 0);
	lv_obj_set_style_pad_all(ap_hint, 8, 0);
	lv_obj_set_style_text_color(ap_hint, COL_YELLOW, 0);
	ap_ok = button(approval, LV_SYMBOL_OK "  Approve", COL_GREEN, lv_color_black(), btn_w, approve_event);
	lv_obj_set_size(ap_ok, btn_w, btn_h);
	lv_obj_align(ap_ok, LV_ALIGN_BOTTOM_LEFT, lay.pad, -lay.pad);
	ap_no = button(approval, LV_SYMBOL_CLOSE "  Deny", COL_HOT, lv_color_black(), btn_w, deny_event);
	lv_obj_set_size(ap_no, btn_w, btn_h);
	lv_obj_align(ap_no, LV_ALIGN_BOTTOM_RIGHT, -lay.pad, -lay.pad);
	ap_sent = label(approval, f_title, COL_DIM);
	lv_label_set_text(ap_sent, LV_SYMBOL_OK "  sent");
	lv_obj_align(ap_sent, LV_ALIGN_BOTTOM_LEFT, lay.pad, -lay.pad - 8);
}

static void build_page(void)
{
	static const char *const names[3] = {"mood", "fed", "energy"};
	static const int counts[3] = {4, 10, 5};
	int32_t label_w = lv_font_get_line_height(f_body) * 9 / 2;
	int32_t val_w = lv_font_get_line_height(f_body) * 3;
	int32_t cells_w = lay.hud_w - 2 * lay.pad - label_w - val_w;
	int32_t cell_h = lay.compact ? 10 : 12;
	int32_t row_h = lay.row + 6;
	int32_t y = lay.row + 10;

	page = panel(hud, 0, 0, lay.hud_w, lay.hud_h);
	page_title = label(page, f_title, COL_TEXT);
	lv_obj_set_pos(page_title, lay.pad, lay.pad);
	lv_obj_set_width(page_title, lay.hud_w - 2 * lay.pad - 48);
	lv_label_set_long_mode(page_title, LV_LABEL_LONG_DOT);
	page_counter = label(page, f_small, COL_DIM);
	lv_obj_align(page_counter, LV_ALIGN_TOP_RIGHT, -lay.pad, lay.pad + 2);
	page_body = label(page, f_body, COL_DIM);
	lv_obj_set_pos(page_body, lay.pad, y + 4);
	lv_obj_set_width(page_body, lay.hud_w - 2 * lay.pad);
	lv_label_set_long_mode(page_body, LV_LABEL_LONG_WRAP);
	lv_obj_set_style_text_line_space(page_body, lay.compact ? 2 : 4, 0);

	/* aligned key / value rows of the info pages */
	for (int i = 0; i < KV_ROWS; i++) {
		kv_key[i] = label(page, f_body, COL_DIM);
		lv_obj_set_pos(kv_key[i], lay.pad, y + 4 + i * lay.row);
		lv_obj_set_width(kv_key[i], label_w + 8);
		lv_label_set_long_mode(kv_key[i], LV_LABEL_LONG_CLIP);
		kv_val[i] = label(page, f_body, COL_TEXT);
		lv_obj_set_pos(kv_val[i], lay.pad + label_w + 16, y + 4 + i * lay.row);
		lv_obj_set_width(kv_val[i], lay.hud_w - 2 * lay.pad - label_w - 16);
		lv_label_set_long_mode(kv_val[i], LV_LABEL_LONG_DOT);
	}

	/* the pet stats: segmented bars for mood and energy, a progress bar for fed */
	for (int r = 0; r < 3; r++) {
		lv_obj_t *name;
		int32_t gap = 4;
		int32_t cw = (cells_w - (counts[r] - 1) * gap) / counts[r];

		stat_rows[r] = panel(page, lay.pad, y + 4 + r * row_h, lay.hud_w - 2 * lay.pad, row_h);
		name = label(stat_rows[r], f_body, COL_DIM);
		lv_label_set_text(name, names[r]);
		lv_obj_align(name, LV_ALIGN_LEFT_MID, 0, 0);
		stat_val[r] = label(stat_rows[r], f_small, COL_DIM);
		lv_obj_align(stat_val[r], LV_ALIGN_RIGHT_MID, 0, 0);
		if (r == 1) {
			fed_bar = lv_bar_create(stat_rows[r]);
			lv_obj_set_size(fed_bar, cells_w, cell_h - 2);
			lv_obj_align(fed_bar, LV_ALIGN_LEFT_MID, label_w, 0);
			lv_bar_set_range(fed_bar, 0, 100);
			lv_obj_set_style_bg_color(fed_bar, COL_CARD2, 0);
			lv_obj_set_style_radius(fed_bar, LV_RADIUS_CIRCLE, 0);
			lv_obj_set_style_radius(fed_bar, LV_RADIUS_CIRCLE, LV_PART_INDICATOR);
			continue;
		}
		for (int i = 0; i < counts[r]; i++) {
			lv_obj_t *c = panel(stat_rows[r], label_w + i * (cw + gap), (row_h - cell_h) / 2,
					    cw, cell_h);

			card(c, COL_CARD2, 4);
			stat_cells[r][i] = c;
		}
	}
	y += 3 * row_h + 10;
	stat_badge = label(page, f_body, lv_color_black());
	lv_obj_set_style_bg_opa(stat_badge, LV_OPA_COVER, 0);
	lv_obj_set_style_radius(stat_badge, LV_RADIUS_CIRCLE, 0);
	lv_obj_set_style_pad_hor(stat_badge, 10, 0);
	lv_obj_set_style_pad_ver(stat_badge, 3, 0);
	lv_obj_set_pos(stat_badge, lay.pad, y);
	stats_body_y = y + lay.row + 12;
}

static void build_clock(void)
{
	clock_panel = panel(hud, 0, 0, lay.hud_w, lay.hud_h);
	clk_time = label(clock_panel, f_clock, COL_TEXT);
	clk_sec = label(clock_panel, lay.compact ? f_body : &lv_font_montserrat_20, COL_DIM);
	clk_date = label(clock_panel, f_title, COL_DIM);
}

static void build_hud(void)
{
	hud = panel(scr, lay.hud_x, lay.hud_y, lay.hud_w, lay.hud_h);
	card(hud, COL_CARD, lay.radius);
	lv_obj_set_style_clip_corner(hud, true, 0);
	lv_obj_set_clickable(hud, true);
	lv_obj_add_event_cb(hud, hud_event, LV_EVENT_ALL, NULL);
	build_transcript();
	build_approval();
	build_page();
	build_clock();
}

static void build_bottom(void)
{
	lv_obj_t *bottom = panel(scr, 0, lay.h - lay.bottom_h, lay.w, lay.bottom_h);

	bottom_left = label(bottom, f_small, COL_DIM);
	lv_obj_set_width(bottom_left, lay.w * 3 / 5);
	lv_label_set_long_mode(bottom_left, LV_LABEL_LONG_CLIP);
	lv_obj_align(bottom_left, LV_ALIGN_LEFT_MID, 10, 0);
	bottom_pill = pill(bottom, f_small, &bottom_pill_lbl);
	lv_obj_align(bottom_pill, LV_ALIGN_RIGHT_MID, -8, 0);
}

static void build_overlay(void)
{
	int32_t ow = LV_MIN(260, lay.w - 24);
	int32_t step = lay.item_h + 4;

	scrim = panel(scr, 0, 0, lay.w, lay.h);
	lv_obj_set_style_bg_color(scrim, lv_color_black(), 0);
	lv_obj_set_style_bg_opa(scrim, LV_OPA_60, 0);
	lv_obj_set_clickable(scrim, true);
	lv_obj_add_event_cb(scrim, scrim_event, LV_EVENT_CLICKED, NULL);
	lv_obj_set_hidden(scrim, true);

	overlay = lv_obj_create(scr);
	lv_obj_remove_style_all(overlay);
	lv_obj_set_size(overlay, ow, 44 + OVERLAY_MAX * step);
	card(overlay, COL_CARD, lay.radius);
	lv_obj_set_style_border_width(overlay, 1, 0);
	lv_obj_set_style_border_color(overlay, COL_LINE, 0);
	lv_obj_set_style_shadow_width(overlay, 30, 0);
	lv_obj_set_style_shadow_color(overlay, lv_color_black(), 0);
	lv_obj_set_style_shadow_opa(overlay, LV_OPA_50, 0);
	lv_obj_set_scrollable(overlay, false);
	ov_title = label(overlay, f_small, COL_DIM);
	lv_obj_set_pos(ov_title, 16, 12);
	for (int i = 0; i < OVERLAY_MAX; i++) {
		ov_btn[i] = lv_button_create(overlay);
		lv_obj_remove_style_all(ov_btn[i]);
		lv_obj_set_pos(ov_btn[i], 8, lay.row + 20 + i * step);
		lv_obj_set_size(ov_btn[i], ow - 16, lay.item_h);
		card(ov_btn[i], COL_CARD, lay.radius - 4);
		lv_obj_add_event_cb(ov_btn[i], overlay_event, LV_EVENT_CLICKED,
				    (void *)(uintptr_t)i);
		ov_lbl[i] = label(ov_btn[i], f_title, COL_DIM);
		lv_obj_align(ov_lbl[i], LV_ALIGN_LEFT_MID, 10, 0);
	}
	lv_obj_set_hidden(overlay, true);
}

static void build_passkey(void)
{
	lv_obj_t *l, *box;
	int32_t gap = lay.h / 8;

	passkey_panel = panel(scr, 0, 0, lay.w, lay.h);
	lv_obj_set_style_bg_color(passkey_panel, lv_color_black(), 0);
	lv_obj_set_style_bg_opa(passkey_panel, LV_OPA_COVER, 0);
	l = label(passkey_panel, f_small, COL_DIM);
	lv_label_set_text(l, LV_SYMBOL_BLUETOOTH "  BLUETOOTH PAIRING");
	lv_obj_align(l, LV_ALIGN_CENTER, 0, -gap - lay.row * 2);
	box = lv_obj_create(passkey_panel);
	lv_obj_remove_style_all(box);
	card(box, COL_CARD, lay.radius);
	lv_obj_set_style_pad_hor(box, 20, 0);
	lv_obj_set_style_pad_ver(box, 12, 0);
	lv_obj_set_size(box, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
	lv_obj_set_scrollable(box, false);
	lv_obj_align(box, LV_ALIGN_CENTER, 0, -lay.row);
	pk_digits = label(box, f_clock, COL_TEXT);
	lv_obj_set_style_text_letter_space(pk_digits, 4, 0);
	l = label(passkey_panel, f_title, COL_DIM);
	lv_label_set_text(l, "enter this on the desktop");
	lv_obj_align(l, LV_ALIGN_CENTER, 0, gap + 4);
	pk_name = label(passkey_panel, f_body, COL_DIM);
	lv_obj_align(pk_name, LV_ALIGN_CENTER, 0, gap + lay.row * 2);
	lv_obj_set_hidden(passkey_panel, true);
}

void ui_init(void)
{
	layout_compute();
	accent = lv_color_hex(0xDA7756);
	scr = lv_screen_active();
	lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
	lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
	lv_obj_set_scrollable(scr, false);
	lv_obj_add_event_cb(scr, wake_event, LV_EVENT_PRESSED, NULL);
	build_top();
	build_pet();
	build_hud();
	build_bottom();
	build_overlay();
	build_passkey();
}

/* --- update ------------------------------------------------------------------ */

static void fmt_tokens(char *buf, size_t len, uint32_t v)
{
	if (v >= 1000000) {
		snprintf(buf, len, "%lu.%luM", (unsigned long)(v / 1000000),
			 (unsigned long)((v / 100000) % 10));
	} else if (v >= 1000) {
		snprintf(buf, len, "%lu.%luK", (unsigned long)(v / 1000),
			 (unsigned long)((v / 100) % 10));
	} else {
		snprintf(buf, len, "%lu", (unsigned long)v);
	}
}

static void apply_accent(uint16_t body565)
{
	if (body565 == cur_body) {
		return;
	}
	cur_body = body565;
	accent = art_color_from_565(body565);
	lv_obj_set_style_text_color(top_name, accent, 0);
	lv_obj_set_style_bg_color(install_bar, accent, LV_PART_INDICATOR);
	lv_obj_set_style_bg_color(fed_bar, accent, LV_PART_INDICATOR);
	lv_obj_set_style_bg_color(stat_badge, accent, 0);
	lv_obj_set_style_bg_color(bottom_pill, accent, 0);
	lv_obj_set_style_text_color(bottom_pill_lbl, lv_color_black(), 0);
	lv_obj_set_style_text_color(wait_icon, accent, 0);
	lv_obj_set_style_text_color(ov_title, accent, 0);
	lv_obj_set_style_text_color(ap_head, accent, 0);
	lv_obj_set_style_text_color(pk_digits, accent, 0);
}

static void kv_hide(void)
{
	for (int i = 0; i < KV_ROWS; i++) {
		show(kv_key[i], false);
		show(kv_val[i], false);
	}
}

static void kv_set(int i, const char *key, const char *val)
{
	if (i >= KV_ROWS) {
		return;
	}
	set_text(kv_key[i], key);
	set_text(kv_val[i], val);
	show(kv_key[i], true);
	show(kv_val[i], true);
}

static void stats_hide(void)
{
	for (int r = 0; r < 3; r++) {
		show(stat_rows[r], false);
	}
	show(stat_badge, false);
}

static void update_pet_page(const struct buddy_model *m, uint32_t now)
{
	const struct zbuddy_stats_data *st = zbuddy_stats_get();
	const struct zbuddy_snapshot *snap = buddy_snapshot();
	const struct persist_data *d = persist_get();
	char buf[200], tok[16], today[16];

	kv_hide();
	if (d->owner[0] != '\0') {
		snprintf(buf, sizeof(buf), "%s's %s", d->owner, d->name);
	} else {
		snprintf(buf, sizeof(buf), "%s", d->name);
	}
	set_text(page_title, buf);
	snprintf(buf, sizeof(buf), "%u / %u", m->pet_page + 1, PET_PAGES);
	set_text(page_counter, buf);

	if (m->pet_page == 0) {
		uint8_t mood = zbuddy_stats_mood_tier();
		uint8_t fed = zbuddy_stats_fed_progress();
		uint8_t en = zbuddy_stats_energy_tier(now);
		uint32_t into = st->tokens % CONFIG_ZBUDDY_TOKENS_PER_LEVEL;
		lv_color_t mood_col = mood >= 3 ? COL_PINK : mood >= 2 ? accent : COL_DIM;
		lv_color_t en_col = en >= 4 ? COL_CYAN : en >= 2 ? COL_YELLOW : COL_HOT;

		for (int i = 0; i < 4; i++) {
			lv_obj_set_style_bg_color(stat_cells[0][i], i < mood ? mood_col : COL_CARD2, 0);
		}
		for (int i = 0; i < 5; i++) {
			lv_obj_set_style_bg_color(stat_cells[2][i], i < en ? en_col : COL_CARD2, 0);
		}
		lv_bar_set_value(fed_bar, (int32_t)(fed * 10), LV_ANIM_OFF);
		snprintf(buf, sizeof(buf), "%u/4", mood);
		set_text(stat_val[0], buf);
		fmt_tokens(tok, sizeof(tok), into);
		fmt_tokens(today, sizeof(today), CONFIG_ZBUDDY_TOKENS_PER_LEVEL);
		snprintf(buf, sizeof(buf), "%s / %s", tok, today);
		set_text(stat_val[1], buf);
		snprintf(buf, sizeof(buf), "%u/5", en);
		set_text(stat_val[2], buf);
		for (int r = 0; r < 3; r++) {
			show(stat_rows[r], true);
		}
		snprintf(buf, sizeof(buf), "Level %u", st->level);
		set_text(stat_badge, buf);
		show(stat_badge, true);
		fmt_tokens(tok, sizeof(tok), st->tokens);
		fmt_tokens(today, sizeof(today), snap->tokens_today);
		snprintf(buf, sizeof(buf),
			 "approved %u   denied %u\nnapped %luh %02lum\ntokens %s   today %s",
			 st->approvals, st->denials, (unsigned long)(st->nap_seconds / 3600),
			 (unsigned long)((st->nap_seconds / 60) % 60), tok, today);
		set_text(page_body, buf);
		lv_obj_set_pos(page_body, lay.pad, stats_body_y);
	} else {
		stats_hide();
		lv_obj_set_pos(page_body, lay.pad, lay.row + 14);
		set_text(page_body,
			 "Mood: approve fast to raise it, deny a lot to lower it.\n"
			 "Fed: 50K tokens fill the bar, then a level up.\n"
			 "Energy: a nap (menu) refills it.\n\n"
			 "Tap the pet for the next screen, hold it for the menu, "
			 "swipe it to shake it, tap a page to flip it.");
	}
}

static void update_info_page(const struct buddy_model *m, uint32_t now)
{
	const struct zbuddy_snapshot *snap = buddy_snapshot();
	const struct persist_data *d = persist_get();
	struct zbuddy_proto *p = buddy_proto();
	char buf[320];
	static const char *const sections[INFO_PAGES] = {"About", "Controls", "Claude",
							  "Device", "Bluetooth", "Credits"};

	stats_hide();
	kv_hide();
	set_text(page_title, sections[m->info_page]);
	snprintf(buf, sizeof(buf), "%u / %u", m->info_page + 1, INFO_PAGES);
	set_text(page_counter, buf);
	lv_obj_set_pos(page_body, lay.pad, lay.row + 14);

	switch (m->info_page) {
	case 0:
		snprintf(buf, sizeof(buf),
			 "I watch your Claude sessions. I sleep when nothing happens, wake when "
			 "you start working and get impatient when approvals pile up.\n\n"
			 "Tap me on a prompt to approve from here. "
			 "%u species in settings.",
			 (unsigned int)zbuddy_species_count());
		set_text(page_body, buf);
		show(page_body, true);
		break;
	case 1:
		set_text(page_body, "");
		show(page_body, false);
		kv_set(0, "tap pet", "next screen / approve");
		kv_set(1, "hold pet", "menu (or the icon)");
		kv_set(2, "swipe pet", "shake it");
		kv_set(3, "tap page", "next page");
		kv_set(4, "buttons", "approve / deny");
		break;
	case 2: {
		uint32_t age = (now - snap->last_update_ms) / 1000U;
		char v1[16], v2[16], v3[16], v4[24];

		set_text(page_body, "");
		show(page_body, false);
		snprintf(v1, sizeof(v1), "%u", snap->sessions_total);
		snprintf(v2, sizeof(v2), "%u", snap->sessions_running);
		snprintf(v3, sizeof(v3), "%u", snap->sessions_waiting);
		snprintf(v4, sizeof(v4), "%lu s ago", (unsigned long)age);
		kv_set(0, "sessions", v1);
		kv_set(1, "running", v2);
		kv_set(2, "waiting", v3);
		kv_set(3, "source", zbuddy_proto_source(p, now));
		kv_set(4, "link", !bridge_ble_connected() ? "-"
				  : bridge_ble_secure() ? "encrypted" : "open");
		kv_set(5, "last msg", v4);
		kv_set(6, "mood", zbuddy_persona_name(m->active));
		break;
	}
	case 3: {
		uint32_t up = now / 1000U, fs_free, fs_total;
		char v1[24], v2[32], v3[24];
		int i = 0;

		set_text(page_body, "");
		show(page_body, false);
		storage_space(&fs_free, &fs_total);
		snprintf(v1, sizeof(v1), "%luh %02lum", (unsigned long)(up / 3600),
			 (unsigned long)((up / 60) % 60));
		snprintf(v2, sizeof(v2), "%luK of %luK free", (unsigned long)(fs_free / 1024),
			 (unsigned long)(fs_total / 1024));
		snprintf(v3, sizeof(v3), "%dx%d", (int)lay.w, (int)lay.h);
		if (d->owner[0] != '\0') {
			kv_set(i++, "owner", d->owner);
		}
		kv_set(i++, "uptime", v1);
		kv_set(i++, "storage", v2);
		kv_set(i++, "pet", buddy_species_name());
		kv_set(i++, "bluetooth",
		       !bridge_ble_available() ? "none"
		       : zbuddy_proto_link_active(p, now) ? "linked"
		       : bridge_ble_connected() ? "connected" : "advertising");
		kv_set(i++, "board", CONFIG_BOARD);
		kv_set(i++, "display", v3);
		break;
	}
	case 4: {
		bool linked = bridge_ble_connected() && zbuddy_proto_link_active(p, now);

		kv_set(0, "state", !bridge_ble_available() ? "no bluetooth here"
				   : linked ? "linked" : "discoverable");
		kv_set(1, "name", bridge_ble_name());
		kv_set(2, "address", bridge_ble_addr());
		set_text(page_body, linked ? "The desktop is talking to me."
					   : "To pair: open Claude Desktop, Developer > Hardware "
					     "Buddy, pick this name and type the passkey I show.");
		lv_obj_set_pos(page_body, lay.pad, lay.row + 14 + 3 * lay.row + 8);
		show(page_body, true);
		break;
	}
	default:
		set_text(page_body, "");
		show(page_body, false);
		kv_set(0, "original", "Felix Rieseberg, Anthropic");
		kv_set(1, "", "github.com/anthropics/claude-desktop-buddy");
		kv_set(2, "port", "Muhammad Waleed Badar");
		kv_set(3, "", "github.com/walidbadar/zbuddy");
		kv_set(4, "runs on", "Zephyr RTOS, LVGL");
		kv_set(5, "board", CONFIG_BOARD_TARGET);
		break;
	}
}

/* Chat bubbles: user lines on the right, assistant lines on the left, the
 * newest brightest; stacked from the top, scrolled to the bottom.
 */
static void update_transcript(const struct zbuddy_snapshot *snap)
{
	int32_t inner_w = lay.hud_w - 2 * (lay.pad - 2);
	int32_t y = 0;

	if (snap->line_gen == drawn_line_gen) {
		return;
	}
	drawn_line_gen = snap->line_gen;
	for (int i = 0; i < CONFIG_ZBUDDY_LINES; i++) {
		bool on = i < snap->n_lines;
		const char *text = snap->lines[i];
		bool user = strncmp(text, "user:", 5) == 0;
		bool assistant = strncmp(text, "assistant:", 10) == 0;
		bool newest = i == snap->n_lines - 1;

		show(bubbles[i], on);
		if (!on) {
			continue;
		}
		if (user) {
			text += 5;
		} else if (assistant) {
			text += 10;
		}
		while (*text == ' ') {
			text++;
		}
		set_text(bubble_lbl[i], text);
		lv_obj_set_style_text_color(bubble_lbl[i], newest ? COL_TEXT : COL_DIM, 0);
		lv_obj_set_style_bg_color(bubbles[i], user ? COL_CARD2 : lv_color_mix(accent, COL_CARD, LV_OPA_30), 0);
		lv_obj_set_style_max_width(bubble_lbl[i], inner_w * 4 / 5, 0);
		lv_obj_update_layout(bubbles[i]);
		lv_obj_set_pos(bubbles[i], user ? inner_w - lv_obj_get_width(bubbles[i]) : 0, y);
		y += lv_obj_get_height(bubbles[i]) + 6;
	}
	lv_obj_update_layout(transcript);
	lv_obj_scroll_to_y(transcript, LV_COORD_MAX, LV_ANIM_OFF);
}

static void update_waiting(const struct zbuddy_snapshot *snap)
{
	if (bridge_ble_connected()) {
		set_text(wait_title, "Paired, waiting for Claude");
		set_text(wait_sub, "open the Hardware Buddy window");
	} else if (bridge_ble_available()) {
		char buf[64];

		set_text(wait_title, "Waiting for Claude");
		snprintf(buf, sizeof(buf), "advertising as %s", bridge_ble_name());
		set_text(wait_sub, buf);
	} else {
		set_text(wait_title, snap->msg);
		set_text(wait_sub, "no Bluetooth on this board");
	}
}

static void update_approval(const struct buddy_model *m, const struct zbuddy_snapshot *snap,
			    uint32_t now)
{
	uint32_t waited = (now - m->prompt_arrived_ms) / 1000U;
	char buf[48];

	set_text(ap_head, LV_SYMBOL_WARNING "  PERMISSION");
	snprintf(buf, sizeof(buf), "%lu s", (unsigned long)waited);
	set_text(ap_timer, buf);
	lv_obj_set_style_text_color(ap_timer, waited >= 10 ? COL_HOT : COL_DIM, 0);
	set_text(ap_tool, snap->prompt.tool);
	set_text(ap_hint, snap->prompt.hint[0] ? snap->prompt.hint : "(no details)");
	show(ap_ok, !m->response_sent);
	show(ap_no, !m->response_sent);
	show(ap_sent, m->response_sent);
}

static void update_clock(uint32_t now)
{
	struct clock_tm t;
	char buf[32];

	if (!clock_local(now, &t)) {
		return;
	}
	snprintf(buf, sizeof(buf), "%02d:%02d", t.hour, t.min);
	set_text(clk_time, buf);
	snprintf(buf, sizeof(buf), ":%02d", t.sec);
	set_text(clk_sec, buf);
	snprintf(buf, sizeof(buf), "%s %s %02d", clock_day_names[t.wday],
		 clock_month_names[t.mon - 1], t.mday);
	set_text(clk_date, buf);
	lv_obj_align(clk_time, LV_ALIGN_CENTER, -lv_font_get_line_height(f_clock) / 3,
		     -lay.hud_h / 8);
	lv_obj_align_to(clk_sec, clk_time, LV_ALIGN_OUT_RIGHT_BOTTOM, 6, -4);
	lv_obj_align_to(clk_date, clk_time, LV_ALIGN_OUT_BOTTOM_MID,
			lv_font_get_line_height(f_clock) / 3, 10);
}

static void update_overlay(const struct buddy_model *m, uint32_t now)
{
	uint8_t n = buddy_overlay_items();
	char buf[48];

	if (m->overlay == OVERLAY_NONE) {
		show(overlay, false);
		show(scrim, false);
		return;
	}
	set_text(ov_title, buddy_overlay_title());
	for (uint8_t i = 0; i < OVERLAY_MAX; i++) {
		bool on = i < n;
		bool sel = i == m->sel;
		bool armed;

		show(ov_btn[i], on);
		if (!on) {
			continue;
		}
		buddy_overlay_item(i, buf, sizeof(buf), now);
		armed = m->overlay == OVERLAY_RESET && strcmp(buf, "really?") == 0;
		set_text(ov_lbl[i], buf);
		lv_obj_set_style_bg_color(ov_btn[i], sel ? accent : COL_CARD, 0);
		lv_obj_set_style_text_color(ov_lbl[i], armed ? COL_HOT
						       : sel ? lv_color_black() : COL_TEXT, 0);
	}
	lv_obj_set_height(overlay, lay.row + 28 + n * (lay.item_h + 4));
	lv_obj_align(overlay, LV_ALIGN_CENTER, 0, 0);
	show(scrim, true);
	show(overlay, true);
	lv_obj_move_foreground(scrim);
	lv_obj_move_foreground(overlay);
}

void ui_update(uint32_t now)
{
	const struct buddy_model *m = buddy_model();
	const struct zbuddy_snapshot *snap = buddy_snapshot();
	const struct persist_data *d = persist_get();
	const struct zbuddy_palette *pal = m->ascii_mode ? &zbuddy_default_palette : character_palette();
	struct zbuddy_proto *p = buddy_proto();
	uint32_t passkey = bridge_ble_passkey();
	bool prompt = snap->prompt.id[0] != '\0';
	bool home = m->screen == SCREEN_HOME;
	char buf[96];

	/* colours follow the pet: its background and its body as the accent */
	if (pal->bg != cur_bg) {
		cur_bg = pal->bg;
		lv_obj_set_style_bg_color(scr, art_color_from_565(pal->bg), 0);
		drawn_anim_gen = UINT32_MAX;
	}
	apply_accent(m->ascii_mode ? zbuddy_species_get(m->species_idx)->body_color : pal->body);

	/* title row */
	if (d->owner[0] != '\0') {
		snprintf(buf, sizeof(buf), "%s's %s", d->owner, d->name);
	} else {
		snprintf(buf, sizeof(buf), "%s", d->name);
	}
	set_text(top_name, buf);
	if (zbuddy_proto_demo(p)) {
		set_text(top_pill_lbl, "demo");
		lv_obj_set_style_text_color(top_pill_lbl, COL_YELLOW, 0);
	} else if (snap->connected) {
		set_text(top_pill_lbl, LV_SYMBOL_OK "  Claude");
		lv_obj_set_style_text_color(top_pill_lbl, COL_GREEN, 0);
	} else if (bridge_ble_connected()) {
		set_text(top_pill_lbl, LV_SYMBOL_BLUETOOTH "  paired");
		lv_obj_set_style_text_color(top_pill_lbl, COL_CYAN, 0);
	} else if (bridge_ble_available()) {
		set_text(top_pill_lbl, LV_SYMBOL_BLUETOOTH "  " CONFIG_ZBUDDY_APP_BLE_PREFIX);
		lv_obj_set_style_text_color(top_pill_lbl, COL_DIM, 0);
	} else {
		set_text(top_pill_lbl, "offline");
		lv_obj_set_style_text_color(top_pill_lbl, COL_DIM, 0);
	}

	/* the pet */
	if (m->ascii_mode) {
		bool ticked = (int32_t)(now - next_tick_ms) >= 0;

		if (ticked) {
			next_tick_ms = now + CONFIG_ZBUDDY_APP_TICK_MS;
			anim_tick++;
		}
		if (ticked || m->anim_gen != drawn_anim_gen) {
			drawn_anim_gen = m->anim_gen;
			art_canvas_render(zbuddy_species_get(m->species_idx), m->active, anim_tick,
					  pal->bg);
		}
		show(canvas, true);
		character_show(false);
		show(pet_msg, false);
		show(install_bar, false);
	} else if (character_loaded()) {
		show(canvas, false);
		character_show(true);
		character_set_state(m->active, now);
		character_tick(now);
		show(pet_msg, false);
		show(install_bar, false);
	} else {
		show(canvas, false);
		character_show(false);
		if (zbuddy_proto_xfer_active(p)) {
			uint32_t done = zbuddy_proto_xfer_done(p), total = zbuddy_proto_xfer_total(p);

			snprintf(buf, sizeof(buf), "installing\n%luK / %luK", (unsigned long)(done / 1024),
				 (unsigned long)(total / 1024));
			lv_bar_set_value(install_bar, total ? (int32_t)(100ULL * done / total) : 0,
					 LV_ANIM_OFF);
			show(install_bar, true);
		} else {
			snprintf(buf, sizeof(buf), "no character loaded");
			show(install_bar, false);
		}
		set_text(pet_msg, buf);
		show(pet_msg, true);
	}
	if (zbuddy_proto_xfer_active(p) && m->ascii_mode) {
		uint32_t done = zbuddy_proto_xfer_done(p), total = zbuddy_proto_xfer_total(p);

		snprintf(buf, sizeof(buf), "installing %lu%%",
			 (unsigned long)(total ? 100ULL * done / total : 0));
		set_text(top_pill_lbl, buf);
	}

	/* the HUD card */
	{
		bool idle = home && !prompt && !m->clocking && snap->n_lines == 0;

		show(approval, prompt && home);
		show(page, !prompt && !home);
		show(clock_panel, !prompt && home && m->clocking);
		show(transcript, !prompt && home && !m->clocking && !idle && d->prefs.hud);
		show(waiting, !prompt && home && !m->clocking && idle);
		if (prompt && home) {
			update_approval(m, snap, now);
		} else if (m->screen == SCREEN_PET) {
			update_pet_page(m, now);
		} else if (m->screen == SCREEN_INFO) {
			update_info_page(m, now);
		} else if (m->clocking) {
			update_clock(now);
		} else if (idle) {
			update_waiting(snap);
		} else if (d->prefs.hud) {
			update_transcript(snap);
		}
	}

	/* status row */
	if (snap->connected) {
		snprintf(buf, sizeof(buf), "%u running %s %u waiting %s %u total",
			 snap->sessions_running, LV_SYMBOL_BULLET, snap->sessions_waiting,
			 LV_SYMBOL_BULLET, snap->sessions_total);
	} else {
		snprintf(buf, sizeof(buf), "%s", snap->msg);
	}
	set_text(bottom_left, buf);
	snprintf(buf, sizeof(buf), "Lv %u  %s  %s", zbuddy_stats_get()->level, LV_SYMBOL_BULLET,
		 buddy_species_name());
	set_text(bottom_pill_lbl, buf);

	update_overlay(m, now);

	/* pairing */
	if (passkey != 0) {
		snprintf(buf, sizeof(buf), "%03lu %03lu", (unsigned long)(passkey / 1000),
			 (unsigned long)(passkey % 1000));
		set_text(pk_digits, buf);
		set_text(pk_name, bridge_ble_name());
		show(passkey_panel, true);
		lv_obj_move_foreground(passkey_panel);
	} else {
		show(passkey_panel, false);
	}
}
