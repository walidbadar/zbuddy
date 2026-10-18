/*
 * Copyright (c) 2026 Muhammad Waleed Badar
 * SPDX-License-Identifier: GPL-3.0-only
 */
#include <zbuddy/stats.h>

#include <string.h>

#define TOKENS_PER_LEVEL ((uint32_t)CONFIG_ZBUDDY_TOKENS_PER_LEVEL)

static struct zbuddy_stats_data stats;
static zbuddy_stats_save_fn save_cb;
static uint32_t last_bridge_tokens;
static bool tokens_synced;
static bool level_up_pending;
static uint32_t last_nap_end_ms;
static uint8_t energy_at_nap = 3;

static void save(void)
{
	if (save_cb != NULL) {
		save_cb(&stats);
	}
}

void zbuddy_stats_init(const struct zbuddy_stats_data *loaded, zbuddy_stats_save_fn cb,
		       uint32_t now_ms)
{
	memset(&stats, 0, sizeof(stats));
	if (loaded != NULL) {
		stats = *loaded;
	}
	save_cb = cb;
	tokens_synced = false;
	last_bridge_tokens = 0;
	level_up_pending = false;
	last_nap_end_ms = now_ms;
	energy_at_nap = 3;
	if (stats.vel_idx >= ZBUDDY_VELOCITY_RING) {
		stats.vel_idx = 0;
	}
	if (stats.vel_count > ZBUDDY_VELOCITY_RING) {
		stats.vel_count = ZBUDDY_VELOCITY_RING;
	}
	/* The level is derived from the tokens; backfill an old record */
	if (stats.tokens == 0 && stats.level > 0) {
		stats.tokens = (uint32_t)stats.level * TOKENS_PER_LEVEL;
	}
}

const struct zbuddy_stats_data *zbuddy_stats_get(void)
{
	return &stats;
}

void zbuddy_stats_on_approval(uint32_t seconds_to_respond)
{
	stats.approvals++;
	stats.velocity[stats.vel_idx] = (uint16_t)(seconds_to_respond > 65535 ? 65535 : seconds_to_respond);
	stats.vel_idx = (uint8_t)((stats.vel_idx + 1) % ZBUDDY_VELOCITY_RING);
	if (stats.vel_count < ZBUDDY_VELOCITY_RING) {
		stats.vel_count++;
	}
	save();
}

void zbuddy_stats_on_denial(void)
{
	stats.denials++;
	save();
}

void zbuddy_stats_on_nap_end(uint32_t seconds, uint32_t now_ms)
{
	stats.nap_seconds += seconds;
	last_nap_end_ms = now_ms;
	energy_at_nap = 5;
	save();
}

void zbuddy_stats_on_bridge_tokens(uint32_t bridge_total)
{
	uint32_t delta;
	uint8_t before, after;

	/* The desktop counts since it started; after a device reboot the first
	 * value would re-credit the whole session, so latch on first sight. A
	 * drop means the desktop restarted: resync without crediting.
	 */
	if (!tokens_synced) {
		last_bridge_tokens = bridge_total;
		tokens_synced = true;
		return;
	}
	if (bridge_total < last_bridge_tokens) {
		last_bridge_tokens = bridge_total;
		return;
	}
	delta = bridge_total - last_bridge_tokens;
	last_bridge_tokens = bridge_total;
	if (delta == 0) {
		return;
	}
	before = (uint8_t)(stats.tokens / TOKENS_PER_LEVEL);
	stats.tokens += delta;
	after = (uint8_t)(stats.tokens / TOKENS_PER_LEVEL);
	/* Heartbeats are telemetry: persist on the milestone only */
	if (after > before) {
		stats.level = after;
		level_up_pending = true;
		save();
	}
}

bool zbuddy_stats_poll_level_up(void)
{
	bool r = level_up_pending;

	level_up_pending = false;
	return r;
}

uint16_t zbuddy_stats_median_velocity(void)
{
	uint16_t tmp[ZBUDDY_VELOCITY_RING];
	uint8_t n = stats.vel_count;

	if (n == 0) {
		return 0;
	}
	memcpy(tmp, stats.velocity, sizeof(tmp));
	for (uint8_t i = 1; i < n; i++) {
		uint16_t k = tmp[i];
		int j = i - 1;

		while (j >= 0 && tmp[j] > k) {
			tmp[j + 1] = tmp[j];
			j--;
		}
		tmp[j + 1] = k;
	}
	return tmp[n / 2];
}

uint8_t zbuddy_stats_mood_tier(void)
{
	uint16_t vel = zbuddy_stats_median_velocity();
	uint16_t a = stats.approvals, d = stats.denials;
	int tier;

	if (vel == 0) {
		tier = 2;
	} else if (vel < 15) {
		tier = 4;
	} else if (vel < 30) {
		tier = 3;
	} else if (vel < 60) {
		tier = 2;
	} else if (vel < 120) {
		tier = 1;
	} else {
		tier = 0;
	}
	if (a + d >= 3) {
		if (d > a) {
			tier -= 2;
		} else if (d * 2 > a) {
			tier -= 1;
		}
	}
	return (uint8_t)(tier < 0 ? 0 : tier);
}

uint8_t zbuddy_stats_energy_tier(uint32_t now_ms)
{
	uint32_t hours = (now_ms - last_nap_end_ms) / 3600000U;
	int e = (int)energy_at_nap - (int)(hours / 2);

	if (e < 0) {
		e = 0;
	}
	if (e > 5) {
		e = 5;
	}
	return (uint8_t)e;
}

uint8_t zbuddy_stats_fed_progress(void)
{
	return (uint8_t)((stats.tokens % TOKENS_PER_LEVEL) / (TOKENS_PER_LEVEL / 10));
}

void zbuddy_stats_reset(uint32_t now_ms)
{
	memset(&stats, 0, sizeof(stats));
	tokens_synced = false;
	last_bridge_tokens = 0;
	level_up_pending = false;
	last_nap_end_ms = now_ms;
	energy_at_nap = 3;
	save();
}
