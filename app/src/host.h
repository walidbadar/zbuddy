/*
 * Copyright (c) 2026 Muhammad Waleed Badar
 * SPDX-License-Identifier: GPL-3.0-only
 */
#ifndef ZBUDDY_APP_HOST_H
#define ZBUDDY_APP_HOST_H

#include <stdint.h>

#if defined(CONFIG_BOARD_NATIVE_SIM)
void host_init(void);
void host_poll(uint32_t now_ms);
#else
static inline void host_init(void)
{
}
static inline void host_poll(uint32_t now_ms)
{
	(void)now_ms;
}
#endif

#endif
