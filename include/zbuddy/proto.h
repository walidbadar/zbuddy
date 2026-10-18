/*
 * Copyright (c) 2026 Muhammad Waleed Badar
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * The Claude Desktop hardware buddy wire protocol: UTF-8 JSON, one object
 * per line, over the Nordic UART Service (or any byte stream). This module
 * parses the lines, keeps the snapshot, answers the commands with acks and
 * receives character packs; the application supplies the transport, the
 * storage and the persisted names through callbacks.
 */
#ifndef ZBUDDY_PROTO_H
#define ZBUDDY_PROTO_H

#include <zbuddy/zbuddy.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Where pushed character packs go. All paths are relative to the pack
 * being installed; only one pack lives on the device at a time.
 */
struct zbuddy_store {
	/** Free and total bytes of the storage (both may be 0 when unknown). */
	void (*space)(uint32_t *free_bytes, uint32_t *total_bytes);
	/** Bytes that wiping the installed pack(s) would reclaim. */
	uint32_t (*reclaimable)(void);
	/** Start a pack: close the loaded character, wipe old packs, make the
	 * directory. Returns 0 or a negative errno.
	 */
	int (*begin)(const char *name, uint32_t total);
	/** Open file @p path (already validated, no directories) for writing. */
	int (*open)(const char *path, uint32_t size);
	int (*write)(const void *data, size_t len);
	int (*close)(void);
	/** All files written: load the pack. Returns 0 when it is usable. */
	int (*end)(const char *name);
};

/** Device facts for the status ack; fields left at 0/false are omitted
 * where the protocol allows it.
 */
struct zbuddy_sys_info {
	uint32_t uptime_s;
	uint32_t heap_free;
	uint32_t fs_free;
	uint32_t fs_total;
	bool has_battery;
	int bat_pct;
	int bat_mv;
	int bat_ma; /* negative while charging */
	bool usb;
};

struct zbuddy_proto_ops {
	/** Send one line (already newline terminated) to the bridge. */
	void (*send)(const char *line, size_t len);
	/** Pet and owner names (persisted by the application). */
	void (*set_name)(const char *name);
	const char *(*get_name)(void);
	void (*set_owner)(const char *name);
	const char *(*get_owner)(void);
	/** Species choice: 0..N-1 an ASCII species, 0xFF the installed pack. */
	void (*set_species)(uint8_t idx);
	/** Forget all bonds. */
	void (*unpair)(void);
	/** Wall clock from the desktop: UTC seconds and the zone offset. */
	void (*time_sync)(int64_t epoch_s, int32_t tz_offset_s);
	/** Fill the device facts for the status ack. */
	void (*sys_info)(struct zbuddy_sys_info *info);
	/** Whether the link is encrypted ("sec" in the status data). */
	bool (*secure)(void);
	const struct zbuddy_store *store;
};

/** Parser state. Zero it, then zbuddy_proto_init(). */
struct zbuddy_proto {
	const struct zbuddy_proto_ops *ops;
	struct zbuddy_snapshot snap;
	char line[CONFIG_ZBUDDY_LINE_MAX];
	uint16_t line_len;
	bool line_overflow;
	uint32_t last_live_ms;
	uint32_t last_byte_ms;
	/* folder push */
	bool xfer_active;
	bool xfer_file_open;
	char xfer_name[ZBUDDY_NAME_LEN];
	uint32_t xfer_total;
	uint32_t xfer_total_written;
	uint32_t xfer_file_expected;
	uint32_t xfer_file_written;
	/* demo scenarios */
	bool demo;
	uint8_t demo_idx;
	uint32_t demo_next_ms;
	bool rtc_valid;
};

void zbuddy_proto_init(struct zbuddy_proto *p, const struct zbuddy_proto_ops *ops);

/** Feed bytes from the bridge. Complete lines are parsed right away. */
void zbuddy_proto_feed(struct zbuddy_proto *p, const uint8_t *data, size_t len, uint32_t now_ms);

/** Handle one complete JSON line (no newline needed). Returns 0 when the
 * line was understood, -EINVAL when it was not JSON or not for us.
 */
int zbuddy_proto_line(struct zbuddy_proto *p, char *line, size_t len, uint32_t now_ms);

/** Refresh the snapshot: connection timeout, demo scenarios. Call it
 * every loop iteration before reading p->snap.
 */
void zbuddy_proto_poll(struct zbuddy_proto *p, uint32_t now_ms);

/** True while the desktop sent something in the last 30 s. */
bool zbuddy_proto_connected(const struct zbuddy_proto *p, uint32_t now_ms);

/** True while bytes arrived in the last 15 s (the keepalive is 10 s). */
bool zbuddy_proto_link_active(const struct zbuddy_proto *p, uint32_t now_ms);

/** "demo: <scenario>", "live" or "none" for the info screens. */
const char *zbuddy_proto_source(const struct zbuddy_proto *p, uint32_t now_ms);

/** Demo mode cycles fake scenarios every 8 s and ignores live data. */
void zbuddy_proto_set_demo(struct zbuddy_proto *p, bool on, uint32_t now_ms);
bool zbuddy_proto_demo(const struct zbuddy_proto *p);

/** Answer a permission prompt: decision "once" or "deny". */
void zbuddy_proto_permission(struct zbuddy_proto *p, const char *id, bool approve);

/** Folder push progress for the "installing" screen. */
bool zbuddy_proto_xfer_active(const struct zbuddy_proto *p);
uint32_t zbuddy_proto_xfer_done(const struct zbuddy_proto *p);
uint32_t zbuddy_proto_xfer_total(const struct zbuddy_proto *p);

/** Whether a time sync arrived since boot. */
bool zbuddy_proto_rtc_valid(const struct zbuddy_proto *p);

#ifdef __cplusplus
}
#endif

#endif /* ZBUDDY_PROTO_H */
