/*
 * Copyright (c) 2026 Muhammad Waleed Badar
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * BLE peripheral for Claude Desktop: the Nordic UART Service UUIDs with
 * encrypted-only characteristics, LE Secure Connections bonding with the
 * passkey shown on the display (DisplayOnly), advertised as "Claude-XXXX".
 */
#include "bridge.h"

#include <stdio.h>
#include <string.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>

#if defined(CONFIG_ZBUDDY_APP_BRIDGE_DEBUG)
LOG_MODULE_REGISTER(bridge_ble, LOG_LEVEL_DBG);
#else
LOG_MODULE_REGISTER(bridge_ble, LOG_LEVEL_INF);
#endif

#define NUS_SRV_VAL BT_UUID_128_ENCODE(0x6e400001, 0xb5a3, 0xf393, 0xe0a9, 0xe50e24dcca9e)
#define NUS_RX_VAL BT_UUID_128_ENCODE(0x6e400002, 0xb5a3, 0xf393, 0xe0a9, 0xe50e24dcca9e)
#define NUS_TX_VAL BT_UUID_128_ENCODE(0x6e400003, 0xb5a3, 0xf393, 0xe0a9, 0xe50e24dcca9e)

static struct bt_conn *current;
static bool notify_enabled;
static bool secure;
static uint32_t passkey;
static char dev_name[24] = CONFIG_ZBUDDY_APP_BLE_PREFIX;
static char dev_addr[BT_ADDR_LE_STR_LEN] = "-";

static ssize_t rx_write(struct bt_conn *conn, const struct bt_gatt_attr *attr, const void *buf,
			uint16_t len, uint16_t offset, uint8_t flags)
{
	ARG_UNUSED(conn);
	ARG_UNUSED(attr);
	ARG_UNUSED(offset);
	ARG_UNUSED(flags);
	LOG_DBG("rx %u bytes: %.*s", len, (int)MIN(len, 60), (const char *)buf);
	bridge_rx_put(buf, len);
	return len;
}

static void tx_ccc_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
	ARG_UNUSED(attr);
	notify_enabled = (value == BT_GATT_CCC_NOTIFY);
	LOG_INF("notifications %s", notify_enabled ? "on" : "off");
}

BT_GATT_SERVICE_DEFINE(nus_svc, BT_GATT_PRIMARY_SERVICE(BT_UUID_DECLARE_128(NUS_SRV_VAL)),
		       BT_GATT_CHARACTERISTIC(BT_UUID_DECLARE_128(NUS_TX_VAL), BT_GATT_CHRC_NOTIFY,
					      BT_GATT_PERM_READ_ENCRYPT, NULL, NULL, NULL),
		       BT_GATT_CCC(tx_ccc_changed,
				   BT_GATT_PERM_READ_ENCRYPT | BT_GATT_PERM_WRITE_ENCRYPT),
		       BT_GATT_CHARACTERISTIC(BT_UUID_DECLARE_128(NUS_RX_VAL),
					      BT_GATT_CHRC_WRITE | BT_GATT_CHRC_WRITE_WITHOUT_RESP,
					      BT_GATT_PERM_WRITE_ENCRYPT, NULL, rx_write, NULL), );

#define TX_ATTR (&nus_svc.attrs[2])

static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA(BT_DATA_NAME_COMPLETE, dev_name, 0), /* length patched at start */
};

static const struct bt_data sd[] = {
	BT_DATA_BYTES(BT_DATA_UUID128_ALL, NUS_SRV_VAL),
};

static bool advertising;

static void start_adv(void)
{
	struct bt_data ad_now[2] = {ad[0], ad[1]};
	int err;

	ad_now[1].data_len = (uint8_t)strlen(dev_name);
	err = bt_le_adv_start(BT_LE_ADV_CONN_FAST_1, ad_now, ARRAY_SIZE(ad_now), sd, ARRAY_SIZE(sd));
	if (err == -EALREADY) {
		advertising = true;
	} else if (err != 0) {
		advertising = false;
		LOG_ERR("advertising failed (%d)", err);
	} else {
		advertising = true;
		LOG_INF("advertising as '%s'", dev_name);
	}
}

/* Advertising must be running whenever nobody is connected: a failed
 * pairing followed by a disconnect from both sides can leave it stopped,
 * so the main loop checks it every few seconds.
 */
void bridge_ble_poll(uint32_t now_ms)
{
	static uint32_t next_check;

	if ((int32_t)(now_ms - next_check) < 0) {
		return;
	}
	next_check = now_ms + 5000;
	if (current == NULL && !advertising) {
		LOG_WRN("advertising stopped, restarting");
		start_adv();
	}
}

static void connected(struct bt_conn *conn, uint8_t err)
{
	advertising = false; /* a connectable advertiser stops on connection */
	if (err != 0) {
		LOG_WRN("connection failed (%u)", err);
		start_adv();
		return;
	}
	current = bt_conn_ref(conn);
	secure = false;
	passkey = 0;
	{
		char addr[BT_ADDR_LE_STR_LEN];

		bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
		LOG_INF("connected to %s", addr);
	}
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	LOG_INF("disconnected (%u)", reason);
	if (current == conn) {
		bt_conn_unref(current);
		current = NULL;
	}
	notify_enabled = false;
	secure = false;
	passkey = 0;
}

static void recycled(void)
{
	start_adv();
}

static void security_changed(struct bt_conn *conn, bt_security_t level, enum bt_security_err err)
{
	ARG_UNUSED(conn);
	secure = (err == BT_SECURITY_ERR_SUCCESS && level >= BT_SECURITY_L2);
	LOG_INF("security level %d (%d), mtu %u", level, err, bt_gatt_get_mtu(conn));
}

static void le_param_updated(struct bt_conn *conn, uint16_t interval, uint16_t latency,
			     uint16_t timeout)
{
	ARG_UNUSED(conn);
	LOG_INF("conn params: interval %u.%02u ms, latency %u, timeout %u ms", interval * 125 / 100,
		(interval * 125) % 100, latency, timeout * 10);
}

BT_CONN_CB_DEFINE(conn_cbs) = {
	.connected = connected,
	.disconnected = disconnected,
	.recycled = recycled,
	.security_changed = security_changed,
	.le_param_updated = le_param_updated,
};

static void passkey_display(struct bt_conn *conn, unsigned int key)
{
	ARG_UNUSED(conn);
	passkey = key;
	LOG_INF("passkey %06u", key);
}

static void auth_cancel(struct bt_conn *conn)
{
	ARG_UNUSED(conn);
	passkey = 0;
	LOG_INF("pairing cancelled");
}

static const struct bt_conn_auth_cb auth_cb = {
	.passkey_display = passkey_display,
	.cancel = auth_cancel,
};

static void pairing_complete(struct bt_conn *conn, bool bonded)
{
	ARG_UNUSED(conn);
	passkey = 0;
	LOG_INF("pairing complete, %s", bonded ? "bonded" : "not bonded");
}

static void pairing_failed(struct bt_conn *conn, enum bt_security_err reason)
{
	passkey = 0;
	LOG_WRN("pairing failed (%d): %s", reason,
		reason == BT_SECURITY_ERR_AUTH_REQUIREMENT
			? "the host offers no passkey entry (pair it with an agent)"
		: reason == BT_SECURITY_ERR_PIN_OR_KEY_MISSING
			? "stale bond on the host, pair again"
			: "");
	bt_conn_disconnect(conn, BT_HCI_ERR_AUTH_FAIL);
}

static struct bt_conn_auth_info_cb auth_info_cb = {
	.pairing_complete = pairing_complete,
	.pairing_failed = pairing_failed,
};

int bridge_ble_init(void)
{
	bt_addr_le_t addrs[CONFIG_BT_ID_MAX];
	size_t count = ARRAY_SIZE(addrs);
	int err;

	err = bt_enable(NULL);
	if (err != 0) {
		LOG_ERR("bt_enable failed (%d)", err);
		return err;
	}
	if (IS_ENABLED(CONFIG_BT_SETTINGS)) {
		settings_load_subtree("bt");
	}
	bt_conn_auth_cb_register(&auth_cb);
	bt_conn_auth_info_cb_register(&auth_info_cb);

	/* "Claude-XXXX" with the last two bytes of the address, so several
	 * buddies in one room are distinguishable in the desktop picker.
	 */
	bt_id_get(addrs, &count);
	if (count > 0) {
		snprintf(dev_name, sizeof(dev_name), "%s-%02X%02X", CONFIG_ZBUDDY_APP_BLE_PREFIX,
			 addrs[0].a.val[1], addrs[0].a.val[0]);
		bt_addr_le_to_str(&addrs[0], dev_addr, sizeof(dev_addr));
		dev_addr[17] = '\0'; /* drop the " (public)" suffix */
	}
	bt_set_name(dev_name);
	start_adv();
	return 0;
}

void bridge_ble_send(const char *line, size_t len)
{
	size_t sent = 0;

	if (current == NULL || !notify_enabled) {
		return;
	}
	while (sent < len) {
		uint16_t mtu = bt_gatt_get_mtu(current);
		size_t chunk = mtu > 3 ? mtu - 3 : 20;
		size_t n = len - sent;
		int err;

		if (chunk > 244) {
			chunk = 244;
		}
		if (n > chunk) {
			n = chunk;
		}
		LOG_DBG("tx %u bytes: %.*s", (unsigned int)n, (int)MIN(n, 60), line + sent);
		err = bt_gatt_notify(current, TX_ATTR, line + sent, (uint16_t)n);
		if (err == -ENOMEM || err == -EAGAIN) {
			k_msleep(4);
			continue;
		}
		if (err != 0) {
			LOG_WRN("notify failed (%d)", err);
			return;
		}
		sent += n;
	}
}

bool bridge_ble_available(void)
{
	return true;
}

bool bridge_ble_connected(void)
{
	return current != NULL;
}

bool bridge_ble_secure(void)
{
	return secure;
}

uint32_t bridge_ble_passkey(void)
{
	return passkey != 0 ? passkey : bridge_debug_passkey_get();
}

const char *bridge_ble_name(void)
{
	return dev_name;
}

const char *bridge_ble_addr(void)
{
	return dev_addr;
}

void bridge_ble_unpair(void)
{
	int err = bt_unpair(BT_ID_DEFAULT, BT_ADDR_LE_ANY);

	LOG_INF("bonds cleared (%d)", err);
}
