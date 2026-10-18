/*
 * Copyright (c) 2026 Muhammad Waleed Badar
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * Persistent pet statistics: approvals, denials, response velocity, naps,
 * tokens and the level derived from them. Saved sparingly through a
 * callback (flash wears), only on significant events.
 */
#ifndef ZBUDDY_STATS_H
#define ZBUDDY_STATS_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ZBUDDY_VELOCITY_RING 8

/** The persisted record; stored as one blob. */
struct zbuddy_stats_data {
	uint32_t nap_seconds;
	uint16_t approvals;
	uint16_t denials;
	uint16_t velocity[ZBUDDY_VELOCITY_RING]; /* seconds to respond per approval */
	uint8_t vel_idx;
	uint8_t vel_count;
	uint8_t level;
	uint32_t tokens; /* cumulative output tokens, drives the level */
};

/** Called whenever the record must be written. */
typedef void (*zbuddy_stats_save_fn)(const struct zbuddy_stats_data *data);

/** Start from a loaded record (or NULL for a fresh pet). */
void zbuddy_stats_init(const struct zbuddy_stats_data *loaded, zbuddy_stats_save_fn save,
		       uint32_t now_ms);

const struct zbuddy_stats_data *zbuddy_stats_get(void);

void zbuddy_stats_on_approval(uint32_t seconds_to_respond);
void zbuddy_stats_on_denial(void);
void zbuddy_stats_on_nap_end(uint32_t seconds, uint32_t now_ms);

/** The desktop sends its cumulative token count; deltas are credited. */
void zbuddy_stats_on_bridge_tokens(uint32_t bridge_total);

/** True once after each level up. */
bool zbuddy_stats_poll_level_up(void);

/** Median seconds-to-respond of the last approvals, 0 without data. */
uint16_t zbuddy_stats_median_velocity(void);

/** Mood 0..4 (velocity sets the base, denials drag it down). */
uint8_t zbuddy_stats_mood_tier(void);

/** Energy 0..5: 3 at boot, 5 after a nap, drains one per two hours. */
uint8_t zbuddy_stats_energy_tier(uint32_t now_ms);

/** 0..9 pips of the current level's progress. */
uint8_t zbuddy_stats_fed_progress(void);

/** Reset everything (factory reset). */
void zbuddy_stats_reset(uint32_t now_ms);

#ifdef __cplusplus
}
#endif

#endif /* ZBUDDY_STATS_H */
