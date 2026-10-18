/*
 * Copyright (c) 2026 Muhammad Waleed Badar
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * The screens, rendered with LVGL from the pet model (buddy.h).
 */
#ifndef ZBUDDY_APP_UI_H
#define ZBUDDY_APP_UI_H

#include <stdint.h>

void ui_init(void);
/** Refresh what changed; call every loop iteration. */
void ui_update(uint32_t now_ms);

#endif
