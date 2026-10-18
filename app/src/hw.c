/*
 * Copyright (c) 2026 Muhammad Waleed Badar
 * SPDX-License-Identifier: GPL-3.0-only
 */
#include "hw.h"

#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <zephyr/input/input.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#if defined(CONFIG_ZBUDDY_APP_LED)
#include <zephyr/drivers/led_strip.h>
#endif
#if defined(CONFIG_ZBUDDY_APP_SOUND)
#include <zephyr/drivers/buzzer.h>
#endif
#if DT_HAS_ALIAS(zbuddy_backlight) && defined(CONFIG_REGULATOR)
#include <zephyr/drivers/regulator.h>
#define HAS_BACKLIGHT 1
#endif

LOG_MODULE_REGISTER(hw, LOG_LEVEL_INF);

#define LONG_PRESS_MS 600

static const struct device *const display = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
static volatile bool pressed;
static volatile uint32_t press_ms;
static bool long_sent;
static bool screen_on = true;

#if DT_HAS_COMPAT_STATUS_OKAY(gpio_keys)
static void button_cb(struct input_event *evt, void *user_data)
{
	ARG_UNUSED(user_data);
	if (evt->type != INPUT_EV_KEY || evt->code != INPUT_KEY_0) {
		return;
	}
	if (evt->value != 0) {
		press_ms = k_uptime_get_32();
		pressed = true;
	} else {
		pressed = false;
	}
}

INPUT_CALLBACK_DEFINE(NULL, button_cb, NULL);
#endif

static bool was_pressed;

void hw_poll(uint32_t now_ms, struct hw_events *ev)
{
	bool now_pressed = pressed;

	ev->button_short = false;
	ev->button_long = false;
	if (now_pressed && !long_sent && (now_ms - press_ms) >= LONG_PRESS_MS) {
		long_sent = true;
		ev->button_long = true;
	}
	if (!now_pressed && was_pressed) {
		if (!long_sent) {
			ev->button_short = true;
		}
		long_sent = false;
	}
	was_pressed = now_pressed;
}

#if defined(CONFIG_ZBUDDY_APP_LED) && DT_HAS_ALIAS(led_strip)
static const struct device *const strip = DEVICE_DT_GET(DT_ALIAS(led_strip));
static bool led_state = true; /* force the first write */

void hw_led(bool on_phase)
{
	struct led_rgb px = on_phase ? (struct led_rgb){.r = 64, .g = 16, .b = 0}
				     : (struct led_rgb){0};

	if (on_phase == led_state || !device_is_ready(strip)) {
		return;
	}
	led_state = on_phase;
	led_strip_update_rgb(strip, &px, 1);
}
#else
void hw_led(bool on_phase)
{
	ARG_UNUSED(on_phase);
}
#endif

#if defined(CONFIG_ZBUDDY_APP_SOUND) && DT_HAS_ALIAS(buzzer0)
static const struct device *const buzzer = DEVICE_DT_GET(DT_ALIAS(buzzer0));

void hw_beep(uint32_t ms)
{
	if (device_is_ready(buzzer)) {
		buzzer_beep(buzzer, ms);
	}
}
#else
void hw_beep(uint32_t ms)
{
	ARG_UNUSED(ms);
}
#endif

void hw_screen(bool on)
{
	if (on == screen_on) {
		return;
	}
	screen_on = on;
#if defined(HAS_BACKLIGHT)
	{
		const struct device *bl = DEVICE_DT_GET(DT_ALIAS(zbuddy_backlight));

		if (device_is_ready(bl)) {
			if (on) {
				regulator_enable(bl);
			} else {
				regulator_disable(bl);
			}
		}
	}
#endif
	if (on) {
		display_blanking_off(display);
	} else {
		display_blanking_on(display);
	}
}

void hw_init(void)
{
	hw_led(false);
}
