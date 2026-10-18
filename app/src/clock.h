/*
 * Copyright (c) 2026 Muhammad Waleed Badar
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * Wall clock from the desktop's time sync, kept against the uptime.
 */
#ifndef ZBUDDY_APP_CLOCK_H
#define ZBUDDY_APP_CLOCK_H

#include <stdbool.h>
#include <stdint.h>

struct clock_tm {
	int year;
	int mon;  /* 1..12 */
	int mday; /* 1..31 */
	int wday; /* 0 = Sunday */
	int hour;
	int min;
	int sec;
};

void clock_sync(int64_t epoch_s, int32_t tz_offset_s, uint32_t now_ms);
bool clock_valid(void);
/** Local time now; false before the first sync. */
bool clock_local(uint32_t now_ms, struct clock_tm *out);

extern const char *const clock_month_names[12];
extern const char *const clock_day_names[7];

#endif
