/*
 * Copyright (c) 2026 Muhammad Waleed Badar
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * The bridge: bytes between the desktop and the protocol parser, over BLE
 * (Nordic UART Service) and optionally a UART. Both transports push into
 * one receive ring; the main loop drains it into the parser. Lines sent by
 * the parser go to every transport that is up.
 */
#ifndef ZBUDDY_APP_BRIDGE_H
#define ZBUDDY_APP_BRIDGE_H

#include <zbuddy/proto.h>

void bridge_init(void);
/** Drain received bytes into the parser. */
void bridge_poll(struct zbuddy_proto *p, uint32_t now_ms);
/** Send one line to all transports. */
void bridge_send(const char *line, size_t len);
/** Transports call this from any context. */
void bridge_rx_put(const uint8_t *data, size_t len);

/* Bluetooth facts for the screens; stubs without CONFIG_BT */
bool bridge_ble_available(void);
bool bridge_ble_connected(void);
bool bridge_ble_secure(void);
/** Non-zero while a passkey must be shown to the user. */
uint32_t bridge_ble_passkey(void);
const char *bridge_ble_name(void);
const char *bridge_ble_addr(void);
void bridge_ble_unpair(void);

/* Implemented by the transports */
int bridge_ble_init(void);
void bridge_ble_send(const char *line, size_t len);
/** Keeps advertising alive while disconnected; call every loop. */
void bridge_ble_poll(uint32_t now_ms);
int bridge_uart_init(void);
void bridge_uart_send(const char *line, size_t len);

/** Debug: pretend a passkey is being shown (screenshots). */
void bridge_debug_passkey(uint32_t passkey);
uint32_t bridge_debug_passkey_get(void);

#endif
