/*
 * Copyright (c) 2026 Muhammad Waleed Badar
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * What survives a reboot, in the settings subsystem (NVS): the pet and
 * owner names, the species choice, the preferences and the stats.
 */
#ifndef ZBUDDY_APP_PERSIST_H
#define ZBUDDY_APP_PERSIST_H

#include <zbuddy/stats.h>
#include <zbuddy/zbuddy.h>

struct buddy_prefs {
	bool sound;
	bool led;
	bool hud;
};

struct persist_data {
	char name[ZBUDDY_NAME_LEN];
	char owner[ZBUDDY_OWNER_LEN];
	uint8_t species; /* 0xFF: the installed pack */
	struct buddy_prefs prefs;
	struct zbuddy_stats_data stats;
	bool have_stats;
};

int persist_init(void);
struct persist_data *persist_get(void);
void persist_save_name(void);
void persist_save_owner(void);
void persist_save_species(void);
void persist_save_prefs(void);
void persist_save_stats(const struct zbuddy_stats_data *stats);
/** Forget everything (factory reset). */
void persist_wipe(void);

#endif
