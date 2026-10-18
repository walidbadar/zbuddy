/*
 * Copyright (c) 2026 Muhammad Waleed Badar
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * zbuddy: common types of the Claude Desktop hardware buddy.
 */
#ifndef ZBUDDY_ZBUDDY_H
#define ZBUDDY_ZBUDDY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** The seven animation states of a pet, in the order of the character
 * manifest and of the species state tables.
 */
enum zbuddy_persona {
	ZBUDDY_SLEEP,
	ZBUDDY_IDLE,
	ZBUDDY_BUSY,
	ZBUDDY_ATTENTION,
	ZBUDDY_CELEBRATE,
	ZBUDDY_DIZZY,
	ZBUDDY_HEART,
	ZBUDDY_PERSONA_COUNT
};

/** Name of a persona state ("sleep", "idle", ...). */
const char *zbuddy_persona_name(enum zbuddy_persona s);

#define ZBUDDY_NAME_LEN 24
#define ZBUDDY_OWNER_LEN 32

/** A pending permission prompt; id[0] == 0 means none. */
struct zbuddy_prompt {
	char id[48];
	char tool[32];
	char hint[96];
};

/** What the desktop reports about the sessions: the last heartbeat
 * snapshot, plus the transcript lines and the pending prompt.
 */
struct zbuddy_snapshot {
	uint8_t sessions_total;
	uint8_t sessions_running;
	uint8_t sessions_waiting;
	bool recently_completed;
	uint32_t tokens_today;
	uint32_t last_update_ms;
	char msg[32];
	bool connected;
	char lines[CONFIG_ZBUDDY_LINES][CONFIG_ZBUDDY_LINE_LEN];
	uint8_t n_lines;
	uint16_t line_gen; /* bumps when the lines change: the UI resets its scroll */
	struct zbuddy_prompt prompt;
};

/** Base persona for a snapshot (the one-shots dizzy/heart/celebrate are
 * layered on top by the application).
 */
enum zbuddy_persona zbuddy_derive(const struct zbuddy_snapshot *s);

#ifdef __cplusplus
}
#endif

#endif /* ZBUDDY_ZBUDDY_H */
