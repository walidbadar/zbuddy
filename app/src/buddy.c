/*
 * Copyright (c) 2026 Muhammad Waleed Badar
 * SPDX-License-Identifier: GPL-3.0-only
 */
#include "buddy.h"
#include "bridge.h"
#include "character.h"
#include "clock.h"
#include "hw.h"
#include "storage.h"

#include <stdio.h>
#include <string.h>
#include <zbuddy/art.h>
#include <zbuddy/stats.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/reboot.h>

LOG_MODULE_REGISTER(buddy, LOG_LEVEL_INF);

#define SPECIES_GIF 0xFF
#define WAKE_TRANSITION_MS 12000
#define RESET_CONFIRM_MS 3000

static struct buddy_model m;
static struct zbuddy_proto proto;
static char last_prompt_id[sizeof(((struct zbuddy_prompt *)0)->id)];
static int forced_persona = -1;

static const char *const menu_items[] = {"settings", "nap", "help", "about", "demo", "close"};
static const char *const settings_items[] = {"sound", "led", "transcript", "ascii pet",
					     "reset", "back"};
static const char *const reset_items[] = {"delete char", "factory reset", "back"};

/* --- protocol callbacks ---------------------------------------------------- */

static void op_send(const char *line, size_t len)
{
	bridge_send(line, len);
}

static void op_set_name(const char *name)
{
	struct persist_data *d = persist_get();

	strncpy(d->name, name, sizeof(d->name) - 1);
	d->name[sizeof(d->name) - 1] = '\0';
	persist_save_name();
}

static const char *op_get_name(void)
{
	return persist_get()->name;
}

static void op_set_owner(const char *name)
{
	struct persist_data *d = persist_get();

	strncpy(d->owner, name, sizeof(d->owner) - 1);
	d->owner[sizeof(d->owner) - 1] = '\0';
	persist_save_owner();
}

static const char *op_get_owner(void)
{
	return persist_get()->owner;
}

static void op_time_sync(int64_t epoch_s, int32_t tz_offset_s)
{
	clock_sync(epoch_s, tz_offset_s, k_uptime_get_32());
}

static void op_sys_info(struct zbuddy_sys_info *info)
{
	info->uptime_s = k_uptime_get_32() / 1000U;
	storage_space(&info->fs_free, &info->fs_total);
}

static const struct zbuddy_proto_ops ops = {
	.send = op_send,
	.set_name = op_set_name,
	.get_name = op_get_name,
	.set_owner = op_set_owner,
	.get_owner = op_get_owner,
	.set_species = buddy_set_species,
	.unpair = bridge_ble_unpair,
	.time_sync = op_time_sync,
	.sys_info = op_sys_info,
	.secure = bridge_ble_secure,
	/* .store is set at init: the storage may be absent */
};

static struct zbuddy_proto_ops live_ops;

/* --- helpers ----------------------------------------------------------------- */

static void beep(uint32_t ms)
{
	if (persist_get()->prefs.sound) {
		hw_beep(ms);
	}
}

static void one_shot(enum zbuddy_persona s, uint32_t dur_ms, uint32_t now_ms)
{
	m.active = s;
	m.one_shot_until = now_ms + dur_ms;
	m.anim_gen++;
}

void buddy_wake(uint32_t now_ms)
{
	m.last_interact_ms = now_ms;
	if (m.napping) {
		m.napping = false;
		zbuddy_stats_on_nap_end((now_ms - m.nap_start_ms) / 1000U, now_ms);
	}
	if (m.screen_off) {
		m.screen_off = false;
		hw_screen(true);
		m.wake_transition_until = now_ms + WAKE_TRANSITION_MS;
		m.anim_gen++;
	}
}

static void close_overlays(void)
{
	m.overlay = OVERLAY_NONE;
	m.sel = 0;
	m.reset_armed = 0xFF;
}

static void next_pet(void)
{
	struct persist_data *d = persist_get();
	uint8_t n = (uint8_t)zbuddy_species_count();

	if (!m.ascii_mode) { /* pack -> first species */
		m.ascii_mode = true;
		m.species_idx = 0;
	} else if (m.species_idx + 1 >= n && m.gif_available) { /* last species -> pack */
		m.ascii_mode = false;
	} else {
		m.species_idx = (uint8_t)((m.species_idx + 1) % n);
	}
	d->species = m.ascii_mode ? m.species_idx : SPECIES_GIF;
	persist_save_species();
	m.anim_gen++;
}

void buddy_set_species(uint8_t idx)
{
	struct persist_data *d = persist_get();

	m.gif_available = character_loaded();
	d->species = idx;
	persist_save_species();
	m.ascii_mode = !(m.gif_available && idx == SPECIES_GIF);
	if (m.ascii_mode && idx < zbuddy_species_count()) {
		m.species_idx = idx;
	}
	m.anim_gen++;
}

const char *buddy_species_name(void)
{
	if (m.ascii_mode) {
		const struct zbuddy_species *sp = zbuddy_species_get(m.species_idx);

		return sp != NULL ? sp->name : "?";
	}
	return character_loaded() ? character_name() : "none";
}

/* --- menus ------------------------------------------------------------------- */

static void apply_setting(uint8_t idx, uint32_t now_ms)
{
	struct persist_data *d = persist_get();

	ARG_UNUSED(now_ms);
	switch (idx) {
	case 0:
		d->prefs.sound = !d->prefs.sound;
		break;
	case 1:
		d->prefs.led = !d->prefs.led;
		if (!d->prefs.led) {
			hw_led(false);
		}
		break;
	case 2:
		d->prefs.hud = !d->prefs.hud;
		break;
	case 3:
		next_pet();
		return;
	case 4:
		m.overlay = OVERLAY_RESET;
		m.sel = 0;
		m.reset_armed = 0xFF;
		return;
	default:
		close_overlays();
		return;
	}
	persist_save_prefs();
}

/* Tap-twice confirm: the first activation arms ("really?"), the second
 * within three seconds executes.
 */
static void apply_reset(uint8_t idx, uint32_t now_ms)
{
	bool armed = (m.reset_armed == idx) && (int32_t)(now_ms - m.reset_armed_until) < 0;

	if (idx == 2) {
		close_overlays();
		return;
	}
	if (!armed) {
		m.reset_armed = idx;
		m.reset_armed_until = now_ms + RESET_CONFIRM_MS;
		beep(60);
		return;
	}
	beep(200);
	if (idx == 0) {
		character_close();
		storage_wipe_characters();
		character_init(NULL); /* the built-in pack, when there is one */
		m.gif_available = character_loaded();
		m.ascii_mode = !m.gif_available;
		persist_get()->species = m.ascii_mode ? m.species_idx : SPECIES_GIF;
		persist_save_species();
		close_overlays();
		m.anim_gen++;
		return;
	}
	/* factory reset: names, stats, preferences, packs and bonds */
	character_close();
	storage_wipe_characters();
	persist_wipe();
	zbuddy_stats_reset(now_ms);
	bridge_ble_unpair();
	k_msleep(300);
	sys_reboot(SYS_REBOOT_COLD);
}

static void menu_confirm(uint8_t idx, uint32_t now_ms)
{
	switch (idx) {
	case 0:
		m.overlay = OVERLAY_SETTINGS;
		m.sel = 0;
		break;
	case 1: /* nap: the screen goes dark until the next touch */
		close_overlays();
		m.napping = true;
		m.nap_start_ms = now_ms;
		m.screen_off = true;
		hw_screen(false);
		break;
	case 2:
	case 3:
		close_overlays();
		m.screen = SCREEN_INFO;
		m.info_page = (idx == 2) ? INFO_PG_BUTTONS : INFO_PG_CREDITS;
		break;
	case 4:
		zbuddy_proto_set_demo(&proto, !zbuddy_proto_demo(&proto), now_ms);
		break;
	default:
		close_overlays();
		break;
	}
}

void buddy_pick(uint8_t idx, uint32_t now_ms)
{
	buddy_wake(now_ms);
	beep(30);
	m.sel = idx;
	switch (m.overlay) {
	case OVERLAY_MENU:
		menu_confirm(idx, now_ms);
		break;
	case OVERLAY_SETTINGS:
		apply_setting(idx, now_ms);
		break;
	case OVERLAY_RESET:
		apply_reset(idx, now_ms);
		break;
	default:
		break;
	}
}

const char *buddy_overlay_title(void)
{
	switch (m.overlay) {
	case OVERLAY_MENU:
		return "menu";
	case OVERLAY_SETTINGS:
		return "settings";
	case OVERLAY_RESET:
		return "reset";
	default:
		return "";
	}
}

uint8_t buddy_overlay_items(void)
{
	switch (m.overlay) {
	case OVERLAY_MENU:
		return ARRAY_SIZE(menu_items);
	case OVERLAY_SETTINGS:
		return ARRAY_SIZE(settings_items);
	case OVERLAY_RESET:
		return ARRAY_SIZE(reset_items);
	default:
		return 0;
	}
}

void buddy_overlay_item(uint8_t idx, char *buf, size_t len, uint32_t now_ms)
{
	const struct buddy_prefs *p = &persist_get()->prefs;

	switch (m.overlay) {
	case OVERLAY_MENU:
		if (idx == 4) {
			snprintf(buf, len, "%-12s%s", menu_items[idx],
				 zbuddy_proto_demo(&proto) ? "on" : "off");
		} else {
			snprintf(buf, len, "%s", menu_items[idx]);
		}
		break;
	case OVERLAY_SETTINGS:
		switch (idx) {
		case 0:
		case 1:
		case 2: {
			bool v = idx == 0 ? p->sound : idx == 1 ? p->led : p->hud;

			snprintf(buf, len, "%-12s%s", settings_items[idx], v ? "on" : "off");
			break;
		}
		case 3: {
			uint8_t total = (uint8_t)zbuddy_species_count() + (m.gif_available ? 1 : 0);
			uint8_t pos = m.ascii_mode ? m.species_idx + 1 : total;

			snprintf(buf, len, "%-12s%u/%u %s", settings_items[idx], pos, total,
				 buddy_species_name());
			break;
		}
		default:
			snprintf(buf, len, "%s", settings_items[idx]);
			break;
		}
		break;
	case OVERLAY_RESET: {
		bool armed = (m.reset_armed == idx) && (int32_t)(now_ms - m.reset_armed_until) < 0;

		snprintf(buf, len, "%s", armed ? "really?" : reset_items[idx]);
		break;
	}
	default:
		buf[0] = '\0';
		break;
	}
}

/* --- inputs ------------------------------------------------------------------ */

static void send_decision(bool approve, uint32_t now_ms)
{
	const struct zbuddy_snapshot *s = &proto.snap;

	if (!m.in_prompt) {
		return;
	}
	zbuddy_proto_permission(&proto, s->prompt.id, approve);
	LOG_INF("prompt %s: %s", s->prompt.id, approve ? "approved" : "denied");
	m.response_sent = true;
	m.in_prompt = false;
	if (approve) {
		uint32_t took_s = (now_ms - m.prompt_arrived_ms) / 1000U;

		zbuddy_stats_on_approval(took_s);
		beep(60);
		if (took_s < 5) {
			one_shot(ZBUDDY_HEART, 2000, now_ms);
		}
	} else {
		zbuddy_stats_on_denial();
		beep(60);
	}
}

void buddy_approve(uint32_t now_ms)
{
	buddy_wake(now_ms);
	send_decision(true, now_ms);
}

void buddy_deny(uint32_t now_ms)
{
	buddy_wake(now_ms);
	send_decision(false, now_ms);
}

void buddy_tap_pet(uint32_t now_ms)
{
	bool was_off = m.screen_off;

	buddy_wake(now_ms);
	if (was_off || m.overlay != OVERLAY_NONE) {
		return;
	}
	if (m.in_prompt) {
		send_decision(true, now_ms);
		return;
	}
	beep(30);
	m.screen = (enum buddy_screen)((m.screen + 1) % SCREEN_COUNT);
}

void buddy_next_page(uint32_t now_ms)
{
	bool was_off = m.screen_off;

	buddy_wake(now_ms);
	if (was_off || m.overlay != OVERLAY_NONE) {
		return;
	}
	beep(30);
	if (m.screen == SCREEN_INFO) {
		m.info_page = (uint8_t)((m.info_page + 1) % INFO_PAGES);
	} else if (m.screen == SCREEN_PET) {
		m.pet_page = (uint8_t)((m.pet_page + 1) % PET_PAGES);
	}
}

void buddy_long_press(uint32_t now_ms)
{
	bool was_off = m.screen_off;

	buddy_wake(now_ms);
	if (was_off) {
		return;
	}
	beep(60);
	if (m.overlay != OVERLAY_NONE) {
		buddy_pick(m.sel, now_ms);
		return;
	}
	m.overlay = OVERLAY_MENU;
	m.sel = 0;
}

void buddy_button_short(uint32_t now_ms)
{
	bool was_off = m.screen_off;

	buddy_wake(now_ms);
	if (was_off) {
		return;
	}
	if (m.overlay != OVERLAY_NONE) {
		beep(30);
		m.sel = (uint8_t)((m.sel + 1) % buddy_overlay_items());
		if (m.overlay == OVERLAY_RESET) {
			m.reset_armed = 0xFF;
		}
		return;
	}
	buddy_tap_pet(now_ms);
}

void buddy_shake(uint32_t now_ms)
{
	if (m.overlay != OVERLAY_NONE || m.screen_off) {
		return;
	}
	buddy_wake(now_ms);
	if ((int32_t)(now_ms - m.one_shot_until) >= 0) {
		one_shot(ZBUDDY_DIZZY, 2000, now_ms);
		LOG_INF("shake: dizzy");
	}
}

/* --- the loop ---------------------------------------------------------------- */

void buddy_init(uint32_t now_ms)
{
	struct persist_data *d = persist_get();

	live_ops = ops;
	live_ops.store = storage_ready() ? storage_store() : NULL;
	zbuddy_proto_init(&proto, &live_ops);
	memset(&m, 0, sizeof(m));
	m.reset_armed = 0xFF;
	m.last_interact_ms = now_ms;

	character_init(NULL);
	m.gif_available = character_loaded();
	/* 0..N-1 = ASCII species, 0xFF = the installed pack (the default, so
	 * a freshly pushed pack is what shows); without a pack, ASCII */
	m.ascii_mode = !(m.gif_available && d->species == SPECIES_GIF);
	m.species_idx = d->species < zbuddy_species_count() ? d->species : 0;
	m.base = ZBUDDY_SLEEP;
	m.active = ZBUDDY_SLEEP;
	LOG_INF("buddy: %s mode", m.ascii_mode ? "ASCII" : "GIF character");
}

static enum zbuddy_persona clock_mood(const struct clock_tm *t, uint32_t now_ms)
{
	bool weekend = (t->wday == 0 || t->wday == 6);
	bool friday = (t->wday == 5);
	int h = t->hour;

	if (h >= 1 && h < 7) {
		return ZBUDDY_SLEEP;
	}
	if (weekend) {
		return (now_ms / 8000 % 6 == 0) ? ZBUDDY_HEART : ZBUDDY_SLEEP;
	}
	if (h < 9) {
		return (now_ms / 6000 % 4 == 0) ? ZBUDDY_IDLE : ZBUDDY_SLEEP;
	}
	if (h == 12) {
		return (now_ms / 5000 % 3 == 0) ? ZBUDDY_HEART : ZBUDDY_IDLE;
	}
	if (friday && h >= 15) {
		return (now_ms / 4000 % 3 == 0) ? ZBUDDY_CELEBRATE : ZBUDDY_IDLE;
	}
	if (h >= 22 || h == 0) {
		return (now_ms / 7000 % 3 == 0) ? ZBUDDY_DIZZY : ZBUDDY_SLEEP;
	}
	return (now_ms / 10000 % 5 == 0) ? ZBUDDY_SLEEP : ZBUDDY_IDLE;
}

void buddy_tick(uint32_t now_ms)
{
	const struct zbuddy_snapshot *s = &proto.snap;
	enum zbuddy_persona before = m.active;
	static uint32_t last_passkey;
	uint32_t passkey;

	zbuddy_proto_poll(&proto, now_ms);
	if (zbuddy_stats_poll_level_up()) {
		one_shot(ZBUDDY_CELEBRATE, 3000, now_ms);
	}
	m.base = zbuddy_derive(s);
	/* after a wake, hold sleep for a while so the wake-up animation shows;
	 * urgent states override it */
	if (m.base == ZBUDDY_IDLE && (int32_t)(now_ms - m.wake_transition_until) < 0) {
		m.base = ZBUDDY_SLEEP;
	}
	if ((int32_t)(now_ms - m.one_shot_until) >= 0) {
		m.active = m.base;
	}

	/* prompt arrival: chirp, jump to the home screen */
	if (strcmp(s->prompt.id, last_prompt_id) != 0) {
		strncpy(last_prompt_id, s->prompt.id, sizeof(last_prompt_id) - 1);
		last_prompt_id[sizeof(last_prompt_id) - 1] = '\0';
		m.response_sent = false;
		m.in_prompt = s->prompt.id[0] != '\0';
		if (m.in_prompt) {
			m.prompt_arrived_ms = now_ms;
			buddy_wake(now_ms);
			beep(80);
			m.screen = SCREEN_HOME;
			close_overlays();
		}
	}

	/* the clock face when nothing is happening and the time is known */
	m.clocking = m.screen == SCREEN_HOME && m.overlay == OVERLAY_NONE && !m.in_prompt &&
		     s->sessions_running == 0 && s->sessions_waiting == 0 && clock_valid();
	if (m.clocking && (int32_t)(now_ms - m.one_shot_until) >= 0) {
		struct clock_tm t;

		if (clock_local(now_ms, &t)) {
			m.active = clock_mood(&t, now_ms);
		}
	}

	/* the pairing passkey wakes the screen */
	passkey = bridge_ble_passkey();
	if (passkey != 0 && last_passkey == 0) {
		buddy_wake(now_ms);
		beep(60);
	}
	last_passkey = passkey;

	/* LED: pulse on attention */
	hw_led(m.active == ZBUDDY_ATTENTION && persist_get()->prefs.led && (now_ms / 400) % 2);

	/* idle screen timeout (never during a prompt) */
	if (CONFIG_ZBUDDY_APP_SCREEN_OFF_S > 0 && !m.screen_off && !m.in_prompt &&
	    now_ms - m.last_interact_ms > (uint32_t)CONFIG_ZBUDDY_APP_SCREEN_OFF_S * 1000U) {
		m.screen_off = true;
		hw_screen(false);
	}
	if (forced_persona >= 0) {
		m.active = (enum zbuddy_persona)forced_persona;
	}
	if (m.active != before) {
		m.anim_gen++;
	}
}

void buddy_force_persona(int s)
{
	forced_persona = (s >= 0 && s < ZBUDDY_PERSONA_COUNT) ? s : -1;
}

void buddy_debug_show(enum buddy_screen screen, enum buddy_overlay overlay, uint8_t page)
{
	m.screen = screen;
	m.overlay = overlay;
	m.sel = 0;
	m.info_page = page % INFO_PAGES;
	m.pet_page = page % PET_PAGES;
}

const struct buddy_model *buddy_model(void)
{
	return &m;
}

struct zbuddy_proto *buddy_proto(void)
{
	return &proto;
}

const struct zbuddy_snapshot *buddy_snapshot(void)
{
	return &proto.snap;
}

const struct buddy_prefs *buddy_prefs(void)
{
	return &persist_get()->prefs;
}
