/*
 * Copyright (c) 2026 Muhammad Waleed Badar
 * SPDX-License-Identifier: GPL-3.0-only
 */
#include "clock.h"

static bool valid;
static int64_t local_at_sync; /* local seconds (epoch + zone offset) */
static uint32_t uptime_at_sync;

const char *const clock_month_names[12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
					   "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
const char *const clock_day_names[7] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};

void clock_sync(int64_t epoch_s, int32_t tz_offset_s, uint32_t now_ms)
{
	local_at_sync = epoch_s + tz_offset_s;
	uptime_at_sync = now_ms;
	valid = true;
}

bool clock_valid(void)
{
	return valid;
}

/* days since 1970-01-01 to a civil date (Howard Hinnant's algorithm) */
static void civil_from_days(int64_t z, int *y, int *m, int *d)
{
	int64_t era, doe, yoe, doy, mp;

	z += 719468;
	era = (z >= 0 ? z : z - 146096) / 146097;
	doe = z - era * 146097;
	yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
	doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
	mp = (5 * doy + 2) / 153;
	*d = (int)(doy - (153 * mp + 2) / 5 + 1);
	*m = (int)(mp < 10 ? mp + 3 : mp - 9);
	*y = (int)(yoe + era * 400 + (*m <= 2 ? 1 : 0));
}

bool clock_local(uint32_t now_ms, struct clock_tm *out)
{
	int64_t t, days, rem;

	if (!valid) {
		return false;
	}
	t = local_at_sync + (int64_t)((now_ms - uptime_at_sync) / 1000U);
	days = t / 86400;
	rem = t % 86400;
	if (rem < 0) {
		rem += 86400;
		days--;
	}
	civil_from_days(days, &out->year, &out->mon, &out->mday);
	out->wday = (int)((days % 7 + 11) % 7); /* 1970-01-01 was a Thursday */
	out->hour = (int)(rem / 3600);
	out->min = (int)((rem % 3600) / 60);
	out->sec = (int)(rem % 60);
	return true;
}
