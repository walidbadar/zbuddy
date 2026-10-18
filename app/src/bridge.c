/*
 * Copyright (c) 2026 Muhammad Waleed Badar
 * SPDX-License-Identifier: GPL-3.0-only
 */
#include "bridge.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/ring_buffer.h>

#if defined(CONFIG_ZBUDDY_APP_BRIDGE_DEBUG)
LOG_MODULE_REGISTER(bridge, LOG_LEVEL_DBG);
#else
LOG_MODULE_REGISTER(bridge, LOG_LEVEL_INF);
#endif

RING_BUF_DECLARE(rx_ring, 8192);
static struct k_spinlock rx_lock;

void bridge_rx_put(const uint8_t *data, size_t len)
{
	k_spinlock_key_t key = k_spin_lock(&rx_lock);
	uint32_t n = ring_buf_put(&rx_ring, data, (uint32_t)len);

	k_spin_unlock(&rx_lock, key);
	if (n < len) {
		LOG_WRN("rx ring full, %u bytes dropped", (unsigned int)(len - n));
	}
}

void bridge_poll(struct zbuddy_proto *p, uint32_t now_ms)
{
	uint8_t buf[256];

#if defined(CONFIG_BT)
	bridge_ble_poll(now_ms);
#endif
	for (;;) {
		k_spinlock_key_t key = k_spin_lock(&rx_lock);
		uint32_t n = ring_buf_get(&rx_ring, buf, sizeof(buf));

		k_spin_unlock(&rx_lock, key);
		if (n == 0) {
			return;
		}
		LOG_DBG("feeding %u bytes", n);
		zbuddy_proto_feed(p, buf, n, now_ms);
	}
}

void bridge_send(const char *line, size_t len)
{
#if defined(CONFIG_BT)
	bridge_ble_send(line, len);
#endif
#if defined(CONFIG_ZBUDDY_APP_UART_BRIDGE)
	bridge_uart_send(line, len);
#endif
}

void bridge_init(void)
{
#if defined(CONFIG_ZBUDDY_APP_UART_BRIDGE)
	if (bridge_uart_init() != 0) {
		LOG_WRN("uart bridge unavailable");
	}
#endif
#if defined(CONFIG_BT)
	if (bridge_ble_init() != 0) {
		LOG_WRN("bluetooth unavailable");
	}
#endif
}

static uint32_t debug_passkey;

void bridge_debug_passkey(uint32_t passkey)
{
	debug_passkey = passkey;
}

uint32_t bridge_debug_passkey_get(void)
{
	return debug_passkey;
}

#if !defined(CONFIG_BT)
bool bridge_ble_available(void)
{
	return false;
}
bool bridge_ble_connected(void)
{
	return false;
}
bool bridge_ble_secure(void)
{
	return false;
}
uint32_t bridge_ble_passkey(void)
{
	return debug_passkey;
}
const char *bridge_ble_name(void)
{
	return "(no bluetooth)";
}
const char *bridge_ble_addr(void)
{
	return "-";
}
void bridge_ble_unpair(void)
{
}
#endif
