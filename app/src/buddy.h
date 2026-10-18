/*
 * Copyright (c) 2026 Muhammad Waleed Badar
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * The pet: what it feels (persona), what the screen shows (mode, pages,
 * menus) and what the inputs do. A port of the original device's loop,
 * without the drawing: ui.c renders this model.
 */
#ifndef ZBUDDY_APP_BUDDY_H
#define ZBUDDY_APP_BUDDY_H

#include "persist.h"
#include <zbuddy/proto.h>

enum buddy_screen {
	SCREEN_HOME,
	SCREEN_PET,
	SCREEN_INFO,
	SCREEN_COUNT,
};

enum buddy_overlay {
	OVERLAY_NONE,
	OVERLAY_MENU,
	OVERLAY_SETTINGS,
	OVERLAY_RESET,
};

#define INFO_PAGES 6
#define PET_PAGES 2
#define INFO_PG_BUTTONS 1
#define INFO_PG_CREDITS 5

struct buddy_model {
	enum zbuddy_persona base;
	enum zbuddy_persona active;
	uint32_t one_shot_until;
	enum buddy_screen screen;
	uint8_t info_page;
	uint8_t pet_page;
	enum buddy_overlay overlay;
	uint8_t sel; /* highlighted item of the open overlay (button navigation) */
	uint8_t reset_armed; /* 0xFF: nothing armed */
	uint32_t reset_armed_until;
	/* prompt */
	bool in_prompt; /* a prompt is pending and not yet answered */
	bool response_sent;
	uint32_t prompt_arrived_ms;
	/* sleep */
	bool napping;
	uint32_t nap_start_ms;
	bool screen_off;
	uint32_t last_interact_ms;
	uint32_t wake_transition_until;
	/* pet */
	bool ascii_mode;
	uint8_t species_idx;
	bool gif_available;
	bool clocking;
	uint32_t anim_gen; /* bumps when the pet must be redrawn at once */
};

void buddy_init(uint32_t now_ms);
/** Run the state machine; call every loop before the UI update. */
void buddy_tick(uint32_t now_ms);

const struct buddy_model *buddy_model(void);
struct zbuddy_proto *buddy_proto(void);
const struct zbuddy_snapshot *buddy_snapshot(void);

/* Inputs */
void buddy_tap_pet(uint32_t now_ms);        /* next screen, or approve during a prompt */
void buddy_long_press(uint32_t now_ms);     /* menu (in a menu: activate the highlighted item) */
void buddy_button_short(uint32_t now_ms);   /* the BOOT button: like a tap, in a menu: next item */
void buddy_next_page(uint32_t now_ms);      /* flip the PET / INFO page */
void buddy_pick(uint8_t idx, uint32_t now_ms); /* activate item idx of the open overlay */
void buddy_approve(uint32_t now_ms);
void buddy_deny(uint32_t now_ms);
void buddy_shake(uint32_t now_ms);
void buddy_wake(uint32_t now_ms);

/* Overlay contents for the UI */
const char *buddy_overlay_title(void);
uint8_t buddy_overlay_items(void);
/** Label of item @p idx, with its value ("sound   on"). */
void buddy_overlay_item(uint8_t idx, char *buf, size_t len, uint32_t now_ms);

/* Species, used by the protocol's "species" command too */
void buddy_set_species(uint8_t idx);
const char *buddy_species_name(void);

const struct buddy_prefs *buddy_prefs(void);

/* Debug hooks (native_sim screenshots): force a persona (-1 = off), open a screen */
void buddy_force_persona(int s);
void buddy_debug_show(enum buddy_screen screen, enum buddy_overlay overlay, uint8_t page);

#endif
