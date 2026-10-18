/*
 * Copyright (c) 2026 Muhammad Waleed Badar
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * zbuddy: a desk pet for Claude Desktop on Zephyr. It sleeps when nothing
 * is happening, wakes when sessions start, gets impatient when an approval
 * is pending, and lets you approve or deny from the device.
 */
#include "bridge.h"
#include "buddy.h"
#include "host.h"
#include "hw.h"
#include "persist.h"
#include "storage.h"
#include "ui.h"

#include <zbuddy/stats.h>
#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <lvgl.h>

LOG_MODULE_REGISTER(zbuddy_app, LOG_LEVEL_INF);

int main(void)
{
	const struct device *display = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
	struct persist_data *d;
	uint32_t now = k_uptime_get_32();

	if (!device_is_ready(display)) {
		LOG_ERR("display not ready");
		return 0;
	}
	hw_init();
	persist_init();
	d = persist_get();
	zbuddy_stats_init(d->have_stats ? &d->stats : NULL, persist_save_stats, now);
	if (storage_init() != 0) {
		LOG_WRN("character storage unavailable");
	}
	ui_init();
	buddy_init(now);
	bridge_init();
	host_init();
	display_blanking_off(display);
	LOG_INF("zbuddy ready");

	for (;;) {
		struct hw_events ev;
		uint32_t next;

		now = k_uptime_get_32();
		bridge_poll(buddy_proto(), now);
		buddy_tick(now);
		hw_poll(now, &ev);
		if (ev.button_long) {
			buddy_long_press(now);
		} else if (ev.button_short) {
			buddy_button_short(now);
		}
		ui_update(now);
		host_poll(now);
		next = lv_timer_handler();
		/* at least a few ms: the lower-priority log and shell threads need it */
		k_msleep(CLAMP(next, 5, 20));
	}
	return 0;
}
