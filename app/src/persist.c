/*
 * Copyright (c) 2026 Muhammad Waleed Badar
 * SPDX-License-Identifier: GPL-3.0-only
 */
#include "persist.h"

#include <string.h>
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>

LOG_MODULE_REGISTER(persist, LOG_LEVEL_INF);

static struct persist_data data = {
	.name = "Buddy",
	.species = 0xFF,
	.prefs = {.sound = true, .led = true, .hud = true},
};

static int read_str(settings_read_cb read_cb, void *cb_arg, size_t len, char *dst, size_t dst_len)
{
	ssize_t n;

	if (len >= dst_len) {
		return -EINVAL;
	}
	n = read_cb(cb_arg, dst, len);
	if (n < 0) {
		return (int)n;
	}
	dst[n] = '\0';
	return 0;
}

static int zb_set(const char *name, size_t len, settings_read_cb read_cb, void *cb_arg)
{
	const char *next;

	if (settings_name_steq(name, "name", &next) && next == NULL) {
		return read_str(read_cb, cb_arg, len, data.name, sizeof(data.name));
	}
	if (settings_name_steq(name, "owner", &next) && next == NULL) {
		return read_str(read_cb, cb_arg, len, data.owner, sizeof(data.owner));
	}
	if (settings_name_steq(name, "species", &next) && next == NULL) {
		return (len == sizeof(data.species) &&
			read_cb(cb_arg, &data.species, len) == (ssize_t)len) ? 0 : -EINVAL;
	}
	if (settings_name_steq(name, "prefs", &next) && next == NULL) {
		return (len == sizeof(data.prefs) &&
			read_cb(cb_arg, &data.prefs, len) == (ssize_t)len) ? 0 : -EINVAL;
	}
	if (settings_name_steq(name, "stats", &next) && next == NULL) {
		if (len == sizeof(data.stats) &&
		    read_cb(cb_arg, &data.stats, len) == (ssize_t)len) {
			data.have_stats = true;
			return 0;
		}
		return -EINVAL;
	}
	return -ENOENT;
}

SETTINGS_STATIC_HANDLER_DEFINE(zbuddy, "zbuddy", NULL, zb_set, NULL, NULL);

int persist_init(void)
{
	int rc = settings_subsys_init();

	if (rc != 0) {
		LOG_ERR("settings init failed (%d)", rc);
		return rc;
	}
	rc = settings_load_subtree("zbuddy");
	if (rc != 0) {
		LOG_WRN("settings load failed (%d)", rc);
	}
	LOG_INF("pet '%s', owner '%s', species %u", data.name, data.owner, data.species);
	return rc;
}

struct persist_data *persist_get(void)
{
	return &data;
}

void persist_save_name(void)
{
	settings_save_one("zbuddy/name", data.name, strlen(data.name));
}

void persist_save_owner(void)
{
	settings_save_one("zbuddy/owner", data.owner, strlen(data.owner));
}

void persist_save_species(void)
{
	settings_save_one("zbuddy/species", &data.species, sizeof(data.species));
}

void persist_save_prefs(void)
{
	settings_save_one("zbuddy/prefs", &data.prefs, sizeof(data.prefs));
}

void persist_save_stats(const struct zbuddy_stats_data *stats)
{
	data.stats = *stats;
	data.have_stats = true;
	settings_save_one("zbuddy/stats", stats, sizeof(*stats));
}

void persist_wipe(void)
{
	static const char *const keys[] = {"zbuddy/name", "zbuddy/owner", "zbuddy/species",
					   "zbuddy/prefs", "zbuddy/stats"};

	for (size_t i = 0; i < ARRAY_SIZE(keys); i++) {
		settings_delete(keys[i]);
	}
	strcpy(data.name, "Buddy");
	data.owner[0] = '\0';
	data.species = 0xFF;
	data.prefs.sound = true;
	data.prefs.led = true;
	data.prefs.hud = true;
	memset(&data.stats, 0, sizeof(data.stats));
	data.have_stats = false;
}
