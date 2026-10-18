/*
 * Copyright (c) 2026 Muhammad Waleed Badar
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * The board's extras: the button, the RGB LED, the buzzer, the backlight.
 */
#ifndef ZBUDDY_APP_HW_H
#define ZBUDDY_APP_HW_H

#include <stdbool.h>
#include <stdint.h>

struct hw_events {
	bool button_short;
	bool button_long;
};

void hw_init(void);
/** Button edge detection; call every loop. */
void hw_poll(uint32_t now_ms, struct hw_events *ev);
/** The LED shows the attention blink (on_phase alternates) or is off. */
void hw_led(bool on_phase);
void hw_beep(uint32_t ms);
/** Backlight and panel on/off. */
void hw_screen(bool on);

#endif
