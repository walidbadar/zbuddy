/*
 * Copyright (c) 2026 Muhammad Waleed Badar
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * The protocol over the chosen zbuddy,bridge-uart: interrupt driven
 * receive into the bridge ring, polled transmit (lines are short).
 */
#include "bridge.h"

#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(bridge_uart, LOG_LEVEL_INF);

static const struct device *const uart = DEVICE_DT_GET(DT_CHOSEN(zbuddy_bridge_uart));
static bool ready;

static void uart_isr(const struct device *dev, void *user_data)
{
	uint8_t buf[64];

	ARG_UNUSED(user_data);
	uart_irq_update(dev);
	while (uart_irq_rx_ready(dev)) {
		int n = uart_fifo_read(dev, buf, sizeof(buf));

		if (n <= 0) {
			break;
		}
		bridge_rx_put(buf, (size_t)n);
	}
}

int bridge_uart_init(void)
{
	if (!device_is_ready(uart)) {
		return -ENODEV;
	}
	uart_irq_callback_user_data_set(uart, uart_isr, NULL);
	uart_irq_rx_enable(uart);
	ready = true;
	LOG_INF("bridge on %s", uart->name);
	return 0;
}

void bridge_uart_send(const char *line, size_t len)
{
	if (!ready) {
		return;
	}
	for (size_t i = 0; i < len; i++) {
		uart_poll_out(uart, (unsigned char)line[i]);
	}
}
