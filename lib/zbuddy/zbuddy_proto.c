/*
 * Copyright (c) 2026 Muhammad Waleed Badar
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * The hardware buddy protocol (see REFERENCE.md of claude-desktop-buddy):
 * the desktop writes newline-terminated JSON objects, the device answers
 * commands with {"ack":...} lines. Everything here is transport agnostic.
 */
#include <zbuddy/proto.h>
#include <zbuddy/stats.h>
#include "zbuddy_json.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <zephyr/sys/base64.h>

#define CONNECT_TIMEOUT_MS 30000U
#define LINK_TIMEOUT_MS 15000U
#define DEMO_PERIOD_MS 8000U
#define XFER_HEADROOM 4096U

static const char *const persona_names[ZBUDDY_PERSONA_COUNT] = {
	"sleep", "idle", "busy", "attention", "celebrate", "dizzy", "heart",
};

const char *zbuddy_persona_name(enum zbuddy_persona s)
{
	return (s < ZBUDDY_PERSONA_COUNT) ? persona_names[s] : "?";
}

enum zbuddy_persona zbuddy_derive(const struct zbuddy_snapshot *s)
{
	if (!s->connected) {
		return ZBUDDY_IDLE;
	}
	if (s->sessions_waiting > 0) {
		return ZBUDDY_ATTENTION;
	}
	if (s->recently_completed) {
		return ZBUDDY_CELEBRATE;
	}
	if (s->sessions_running >= 3) {
		return ZBUDDY_BUSY;
	}
	return ZBUDDY_IDLE;
}

struct demo_scenario {
	const char *name;
	uint8_t total, running, waiting;
	bool completed;
	uint32_t tokens;
};

static const struct demo_scenario demos[] = {
	{"asleep", 0, 0, 0, false, 0},
	{"one idle", 1, 0, 0, false, 12000},
	{"busy", 4, 3, 0, false, 89000},
	{"attention", 2, 1, 1, false, 45000},
	{"completed", 1, 0, 0, true, 142000},
};

void zbuddy_proto_init(struct zbuddy_proto *p, const struct zbuddy_proto_ops *ops)
{
	memset(p, 0, sizeof(*p));
	p->ops = ops;
	strncpy(p->snap.msg, "No Claude connected", sizeof(p->snap.msg) - 1);
}

/* --- output ---------------------------------------------------------------- */

static void send_line(struct zbuddy_proto *p, const char *line)
{
	if (p->ops != NULL && p->ops->send != NULL) {
		p->ops->send(line, strlen(line));
	}
}

static void ack(struct zbuddy_proto *p, const char *what, bool ok, uint32_t n, const char *error)
{
	char b[128];

	if (error != NULL) {
		snprintf(b, sizeof(b), "{\"ack\":\"%s\",\"ok\":%s,\"n\":%lu,\"error\":\"%s\"}\n", what,
			 ok ? "true" : "false", (unsigned long)n, error);
	} else {
		snprintf(b, sizeof(b), "{\"ack\":\"%s\",\"ok\":%s,\"n\":%lu}\n", what,
			 ok ? "true" : "false", (unsigned long)n);
	}
	send_line(p, b);
}

void zbuddy_proto_permission(struct zbuddy_proto *p, const char *id, bool approve)
{
	char esc[64];
	char b[128];

	zbuddy_json_escape(id, esc, sizeof(esc));
	snprintf(b, sizeof(b), "{\"cmd\":\"permission\",\"id\":\"%s\",\"decision\":\"%s\"}\n", esc,
		 approve ? "once" : "deny");
	send_line(p, b);
}

static void send_status(struct zbuddy_proto *p)
{
	const struct zbuddy_proto_ops *ops = p->ops;
	const struct zbuddy_stats_data *st = zbuddy_stats_get();
	struct zbuddy_sys_info info;
	char name[ZBUDDY_NAME_LEN * 2], owner[ZBUDDY_OWNER_LEN * 2];
	char bat[80] = "";
	char b[512];

	memset(&info, 0, sizeof(info));
	if (ops->sys_info != NULL) {
		ops->sys_info(&info);
	}
	zbuddy_json_escape(ops->get_name != NULL ? ops->get_name() : "Buddy", name, sizeof(name));
	zbuddy_json_escape(ops->get_owner != NULL ? ops->get_owner() : "", owner, sizeof(owner));
	if (info.has_battery) {
		snprintf(bat, sizeof(bat), "\"bat\":{\"pct\":%d,\"mV\":%d,\"mA\":%d,\"usb\":%s},",
			 info.bat_pct, info.bat_mv, info.bat_ma, info.usb ? "true" : "false");
	}
	snprintf(b, sizeof(b),
		 "{\"ack\":\"status\",\"ok\":true,\"n\":0,\"data\":{"
		 "\"name\":\"%s\",\"owner\":\"%s\",\"sec\":%s,%s"
		 "\"sys\":{\"up\":%lu,\"heap\":%lu,\"fsFree\":%lu,\"fsTotal\":%lu},"
		 "\"stats\":{\"appr\":%u,\"deny\":%u,\"vel\":%u,\"nap\":%lu,\"lvl\":%u}"
		 "}}\n",
		 name, owner, (ops->secure != NULL && ops->secure()) ? "true" : "false", bat,
		 (unsigned long)info.uptime_s, (unsigned long)info.heap_free,
		 (unsigned long)info.fs_free, (unsigned long)info.fs_total, st->approvals, st->denials,
		 zbuddy_stats_median_velocity(), (unsigned long)st->nap_seconds, st->level);
	send_line(p, b);
}

/* --- one parsed line ------------------------------------------------------- */

struct line_ctx {
	struct zbuddy_proto *p;
	struct zbuddy_jtok cmd, name, idx, total, running, waiting, completed, tokens, tokens_today,
		msg, entries, prompt, time, evt, role, content, path, size, data;
};

static bool take_member(const struct zbuddy_jkv *kv, void *ctx)
{
	struct line_ctx *c = ctx;
	static const struct {
		const char *key;
		size_t off;
	} map[] = {
		{"cmd", offsetof(struct line_ctx, cmd)},
		{"name", offsetof(struct line_ctx, name)},
		{"idx", offsetof(struct line_ctx, idx)},
		{"total", offsetof(struct line_ctx, total)},
		{"running", offsetof(struct line_ctx, running)},
		{"waiting", offsetof(struct line_ctx, waiting)},
		{"completed", offsetof(struct line_ctx, completed)},
		{"tokens", offsetof(struct line_ctx, tokens)},
		{"tokens_today", offsetof(struct line_ctx, tokens_today)},
		{"msg", offsetof(struct line_ctx, msg)},
		{"entries", offsetof(struct line_ctx, entries)},
		{"prompt", offsetof(struct line_ctx, prompt)},
		{"time", offsetof(struct line_ctx, time)},
		{"evt", offsetof(struct line_ctx, evt)},
		{"role", offsetof(struct line_ctx, role)},
		{"content", offsetof(struct line_ctx, content)},
		{"path", offsetof(struct line_ctx, path)},
		{"size", offsetof(struct line_ctx, size)},
		{"d", offsetof(struct line_ctx, data)},
	};

	for (size_t i = 0; i < sizeof(map) / sizeof(map[0]); i++) {
		if (zbuddy_json_key_is(kv, map[i].key)) {
			*(struct zbuddy_jtok *)((char *)c + map[i].off) = kv->value;
			break;
		}
	}
	return true;
}

/* entries: ["line", ...] */
struct entries_ctx {
	struct zbuddy_snapshot *snap;
	uint8_t n;
};

static bool take_entry(const struct zbuddy_jtok *elem, size_t idx, void *ctx)
{
	struct entries_ctx *e = ctx;

	(void)idx;
	if (e->n >= CONFIG_ZBUDDY_LINES) {
		return false;
	}
	zbuddy_json_str(elem, e->snap->lines[e->n], CONFIG_ZBUDDY_LINE_LEN);
	e->n++;
	return true;
}

static bool take_prompt(const struct zbuddy_jkv *kv, void *ctx)
{
	struct zbuddy_prompt *pr = ctx;

	if (zbuddy_json_key_is(kv, "id")) {
		zbuddy_json_str(&kv->value, pr->id, sizeof(pr->id));
	} else if (zbuddy_json_key_is(kv, "tool")) {
		zbuddy_json_str(&kv->value, pr->tool, sizeof(pr->tool));
	} else if (zbuddy_json_key_is(kv, "hint")) {
		zbuddy_json_str(&kv->value, pr->hint, sizeof(pr->hint));
	}
	return true;
}

struct time_ctx {
	int64_t v[2];
	int n;
};

static bool take_time(const struct zbuddy_jtok *elem, size_t idx, void *ctx)
{
	struct time_ctx *t = ctx;

	if (idx < 2) {
		t->v[idx] = zbuddy_json_int(elem, 0);
		t->n++;
	}
	return idx < 1;
}

/* content: [{"type":"text","text":"..."}, ...]: the first text block */
struct content_ctx {
	struct zbuddy_jtok text;
};

static bool take_content_member(const struct zbuddy_jkv *kv, void *ctx)
{
	struct content_ctx *c = ctx;

	if (zbuddy_json_key_is(kv, "text") && kv->value.type == ZBUDDY_J_STRING) {
		c->text = kv->value;
	}
	return true;
}

static bool take_content(const struct zbuddy_jtok *elem, size_t idx, void *ctx)
{
	struct content_ctx *c = ctx;

	(void)idx;
	if (elem->type == ZBUDDY_J_OBJECT) {
		zbuddy_json_obj_each_tok(elem, take_content_member, c);
	} else if (elem->type == ZBUDDY_J_STRING) {
		c->text = *elem;
	}
	return c->text.type == ZBUDDY_J_NONE;
}

static void push_line(struct zbuddy_snapshot *snap, const char *text)
{
	if (snap->n_lines >= CONFIG_ZBUDDY_LINES) {
		memmove(snap->lines[0], snap->lines[1],
			(size_t)(CONFIG_ZBUDDY_LINES - 1) * CONFIG_ZBUDDY_LINE_LEN);
		snap->n_lines = CONFIG_ZBUDDY_LINES - 1;
	}
	strncpy(snap->lines[snap->n_lines], text, CONFIG_ZBUDDY_LINE_LEN - 1);
	snap->lines[snap->n_lines][CONFIG_ZBUDDY_LINE_LEN - 1] = '\0';
	snap->n_lines++;
	snap->line_gen++;
}

static void apply_turn(struct zbuddy_proto *p, struct line_ctx *c)
{
	struct content_ctx cc = {.text = {.type = ZBUDDY_J_NONE}};
	char role[16] = "";
	char text[CONFIG_ZBUDDY_LINE_LEN];
	char line[CONFIG_ZBUDDY_LINE_LEN + sizeof(role) + 2];

	zbuddy_json_str(&c->role, role, sizeof(role));
	if (c->content.type == ZBUDDY_J_ARRAY) {
		zbuddy_json_arr_each_tok(&c->content, take_content, &cc);
	} else if (c->content.type == ZBUDDY_J_STRING) {
		cc.text = c->content;
	}
	if (cc.text.type == ZBUDDY_J_NONE) {
		return;
	}
	zbuddy_json_str(&cc.text, text, sizeof(text));
	for (char *q = text; *q != '\0'; q++) {
		if (*q == '\n' || *q == '\r' || *q == '\t') {
			*q = ' ';
		}
	}
	if (role[0] != '\0') {
		snprintf(line, sizeof(line), "%s: %s", role, text);
	} else {
		snprintf(line, sizeof(line), "%s", text);
	}
	push_line(&p->snap, line);
}

static void apply_snapshot(struct zbuddy_proto *p, struct line_ctx *c, uint32_t now)
{
	struct zbuddy_snapshot *s = &p->snap;

	s->sessions_total = (uint8_t)zbuddy_json_int(&c->total, s->sessions_total);
	s->sessions_running = (uint8_t)zbuddy_json_int(&c->running, s->sessions_running);
	s->sessions_waiting = (uint8_t)zbuddy_json_int(&c->waiting, s->sessions_waiting);
	s->recently_completed = zbuddy_json_bool(&c->completed, false);
	if (c->tokens.type == ZBUDDY_J_NUMBER) {
		zbuddy_stats_on_bridge_tokens((uint32_t)zbuddy_json_int(&c->tokens, 0));
	}
	s->tokens_today = (uint32_t)zbuddy_json_int(&c->tokens_today, s->tokens_today);
	if (c->msg.type == ZBUDDY_J_STRING) {
		zbuddy_json_str(&c->msg, s->msg, sizeof(s->msg));
	}
	if (c->entries.type == ZBUDDY_J_ARRAY) {
		struct entries_ctx e = {.snap = s, .n = 0};
		char last[CONFIG_ZBUDDY_LINE_LEN];
		uint8_t old_n = s->n_lines;

		strncpy(last, old_n > 0 ? s->lines[old_n - 1] : "", sizeof(last) - 1);
		last[sizeof(last) - 1] = '\0';
		zbuddy_json_arr_each_tok(&c->entries, take_entry, &e);
		if (e.n != old_n || (e.n > 0 && strcmp(s->lines[e.n - 1], last) != 0)) {
			s->line_gen++;
		}
		s->n_lines = e.n;
	}
	if (c->prompt.type == ZBUDDY_J_OBJECT) {
		struct zbuddy_prompt pr;

		memset(&pr, 0, sizeof(pr));
		zbuddy_json_obj_each_tok(&c->prompt, take_prompt, &pr);
		s->prompt = pr;
	} else {
		memset(&s->prompt, 0, sizeof(s->prompt));
	}
	s->last_update_ms = now;
}

/* --- folder push ----------------------------------------------------------- */

static bool path_ok(const char *path)
{
	size_t n = strlen(path);

	if (n == 0 || n >= 64 || path[0] == '/' || path[0] == '\\' || path[0] == '.') {
		return false;
	}
	for (size_t i = 0; i < n; i++) {
		char ch = path[i];

		if (ch == '/' || ch == '\\' || ch == ':' || (unsigned char)ch < 0x20) {
			return false;
		}
	}
	return strstr(path, "..") == NULL;
}

static void xfer_begin(struct zbuddy_proto *p, struct line_ctx *c)
{
	const struct zbuddy_store *st = p->ops->store;
	char name[ZBUDDY_NAME_LEN] = "pet";
	uint32_t total = (uint32_t)zbuddy_json_int(&c->total, 0);
	uint32_t free_b = 0, total_b = 0, reclaim = 0, avail;
	int rc;

	if (st == NULL || st->begin == NULL) {
		/* No storage: stay silent, the desktop times out. */
		return;
	}
	if (c->name.type == ZBUDDY_J_STRING) {
		zbuddy_json_str(&c->name, name, sizeof(name));
	}
	for (char *q = name; *q != '\0'; q++) {
		if (!((*q >= 'a' && *q <= 'z') || (*q >= 'A' && *q <= 'Z') ||
		      (*q >= '0' && *q <= '9') || *q == '-' || *q == '_')) {
			*q = '_';
		}
	}
	if (name[0] == '\0') {
		strcpy(name, "pet");
	}
	if (total > CONFIG_ZBUDDY_CHARACTER_MAX_BYTES) {
		ack(p, "char_begin", false, total, "too large");
		return;
	}
	if (st->space != NULL) {
		st->space(&free_b, &total_b);
	}
	if (st->reclaimable != NULL) {
		reclaim = st->reclaimable();
	}
	avail = free_b + reclaim;
	if (total_b > 0 && total > 0 && total + XFER_HEADROOM > avail) {
		char err[64];

		snprintf(err, sizeof(err), "need %luK, have %luK", (unsigned long)(total / 1024),
			 (unsigned long)(avail / 1024));
		ack(p, "char_begin", false, avail, err);
		return;
	}
	strncpy(p->xfer_name, name, sizeof(p->xfer_name) - 1);
	p->xfer_name[sizeof(p->xfer_name) - 1] = '\0';
	rc = st->begin(p->xfer_name, total);
	if (rc != 0) {
		ack(p, "char_begin", false, 0, "storage");
		return;
	}
	p->xfer_total = total;
	p->xfer_total_written = 0;
	p->xfer_active = true;
	p->xfer_file_open = false;
	ack(p, "char_begin", true, 0, NULL);
}

static void xfer_file(struct zbuddy_proto *p, struct line_ctx *c)
{
	const struct zbuddy_store *st = p->ops->store;
	char path[64];

	if (p->xfer_file_open) {
		st->close();
		p->xfer_file_open = false;
	}
	p->xfer_file_expected = (uint32_t)zbuddy_json_int(&c->size, 0);
	p->xfer_file_written = 0;
	zbuddy_json_str(&c->path, path, sizeof(path));
	if (c->path.type != ZBUDDY_J_STRING || !path_ok(path)) {
		ack(p, "file", false, 0, "bad path");
		return;
	}
	if (st->open(path, p->xfer_file_expected) != 0) {
		ack(p, "file", false, 0, NULL);
		return;
	}
	p->xfer_file_open = true;
	ack(p, "file", true, 0, NULL);
}

static void xfer_chunk(struct zbuddy_proto *p, struct line_ctx *c)
{
	static char b64[CONFIG_ZBUDDY_CHUNK_MAX * 4 / 3 + 8];
	static uint8_t raw[CONFIG_ZBUDDY_CHUNK_MAX];
	size_t out_len = 0;
	size_t n;

	if (!p->xfer_file_open || c->data.type != ZBUDDY_J_STRING) {
		ack(p, "chunk", false, 0, NULL);
		return;
	}
	n = zbuddy_json_str(&c->data, b64, sizeof(b64));
	if (n >= sizeof(b64)) {
		ack(p, "chunk", false, 0, "chunk too large");
		return;
	}
	if (base64_decode(raw, sizeof(raw), &out_len, (const uint8_t *)b64, n) != 0) {
		ack(p, "chunk", false, 0, "base64");
		return;
	}
	if (p->ops->store->write(raw, out_len) != 0) {
		ack(p, "chunk", false, 0, "write");
		return;
	}
	p->xfer_file_written += out_len;
	p->xfer_total_written += out_len;
	/* Ack every chunk: the sender waits for it, which is the flow control */
	ack(p, "chunk", true, p->xfer_file_written, NULL);
}

static void xfer_file_end(struct zbuddy_proto *p)
{
	bool ok = p->xfer_file_open &&
		  (p->xfer_file_written == p->xfer_file_expected || p->xfer_file_expected == 0);

	if (p->xfer_file_open) {
		p->ops->store->close();
		p->xfer_file_open = false;
	}
	ack(p, "file_end", ok, p->xfer_file_written, NULL);
}

static void xfer_end(struct zbuddy_proto *p)
{
	const struct zbuddy_store *st = p->ops->store;
	bool ok;

	if (p->xfer_file_open) {
		st->close();
		p->xfer_file_open = false;
	}
	p->xfer_active = false;
	ok = st->end != NULL && st->end(p->xfer_name) == 0;
	if (ok && p->ops->set_species != NULL) {
		p->ops->set_species(0xFF);
	}
	ack(p, "char_end", ok, 0, NULL);
}

/* --- commands -------------------------------------------------------------- */

static int command(struct zbuddy_proto *p, struct line_ctx *c)
{
	const struct zbuddy_proto_ops *ops = p->ops;
	char cmd[16];

	zbuddy_json_str(&c->cmd, cmd, sizeof(cmd));

	if (strcmp(cmd, "name") == 0 || strcmp(cmd, "owner") == 0) {
		bool is_name = cmd[0] == 'n';
		bool ok = c->name.type == ZBUDDY_J_STRING;

		if (ok) {
			char name[ZBUDDY_OWNER_LEN];

			zbuddy_json_str(&c->name, name, is_name ? ZBUDDY_NAME_LEN : ZBUDDY_OWNER_LEN);
			if (is_name && ops->set_name != NULL) {
				ops->set_name(name);
			} else if (!is_name && ops->set_owner != NULL) {
				ops->set_owner(name);
			}
		}
		ack(p, cmd, ok, 0, NULL);
		return 0;
	}
	if (strcmp(cmd, "species") == 0) {
		uint8_t idx = (uint8_t)zbuddy_json_int(&c->idx, 0xFF);

		if (ops->set_species != NULL) {
			ops->set_species(idx);
		}
		ack(p, cmd, true, 0, NULL);
		return 0;
	}
	if (strcmp(cmd, "unpair") == 0) {
		if (ops->unpair != NULL) {
			ops->unpair();
		}
		ack(p, cmd, true, 0, NULL);
		return 0;
	}
	if (strcmp(cmd, "status") == 0) {
		send_status(p);
		return 0;
	}
	if (strcmp(cmd, "char_begin") == 0) {
		xfer_begin(p, c);
		return 0;
	}
	if (!p->xfer_active) {
		/* "permission" is ours to send, not to receive */
		return 0;
	}
	if (strcmp(cmd, "file") == 0) {
		xfer_file(p, c);
	} else if (strcmp(cmd, "chunk") == 0) {
		xfer_chunk(p, c);
	} else if (strcmp(cmd, "file_end") == 0) {
		xfer_file_end(p);
	} else if (strcmp(cmd, "char_end") == 0) {
		xfer_end(p);
	}
	return 0;
}

int zbuddy_proto_line(struct zbuddy_proto *p, char *line, size_t len, uint32_t now_ms)
{
	struct line_ctx c;

	memset(&c, 0, sizeof(c));
	c.p = p;
	if (zbuddy_json_obj_each(line, len, take_member, &c) < 0) {
		return -EINVAL;
	}
	if (c.cmd.type == ZBUDDY_J_STRING) {
		p->last_live_ms = now_ms ? now_ms : 1;
		return command(p, &c);
	}
	if (c.time.type == ZBUDDY_J_ARRAY) {
		struct time_ctx t = {.n = 0};

		zbuddy_json_arr_each_tok(&c.time, take_time, &t);
		if (t.n == 2) {
			if (p->ops->time_sync != NULL) {
				p->ops->time_sync(t.v[0], (int32_t)t.v[1]);
			}
			p->rtc_valid = true;
		}
		p->last_live_ms = now_ms ? now_ms : 1;
		return 0;
	}
	if (c.evt.type == ZBUDDY_J_STRING) {
		if (zbuddy_json_str_is(&c.evt, "turn")) {
			apply_turn(p, &c);
		}
		p->last_live_ms = now_ms ? now_ms : 1;
		return 0;
	}
	apply_snapshot(p, &c, now_ms);
	p->last_live_ms = now_ms ? now_ms : 1;
	return 0;
}

void zbuddy_proto_feed(struct zbuddy_proto *p, const uint8_t *data, size_t len, uint32_t now_ms)
{
	p->last_byte_ms = now_ms ? now_ms : 1;
	for (size_t i = 0; i < len; i++) {
		char ch = (char)data[i];

		if (ch == '\n' || ch == '\r') {
			if (p->line_len > 0 && !p->line_overflow && p->line[0] == '{') {
				p->line[p->line_len] = '\0';
				(void)zbuddy_proto_line(p, p->line, p->line_len, now_ms);
			}
			p->line_len = 0;
			p->line_overflow = false;
		} else if (p->line_len < sizeof(p->line) - 1) {
			p->line[p->line_len++] = ch;
		} else {
			p->line_overflow = true;
		}
	}
}

/* --- polling --------------------------------------------------------------- */

bool zbuddy_proto_connected(const struct zbuddy_proto *p, uint32_t now_ms)
{
	return p->last_live_ms != 0 && (now_ms - p->last_live_ms) <= CONNECT_TIMEOUT_MS;
}

bool zbuddy_proto_link_active(const struct zbuddy_proto *p, uint32_t now_ms)
{
	return p->last_byte_ms != 0 && (now_ms - p->last_byte_ms) <= LINK_TIMEOUT_MS;
}

const char *zbuddy_proto_source(const struct zbuddy_proto *p, uint32_t now_ms)
{
	if (p->demo) {
		return demos[p->demo_idx].name;
	}
	return zbuddy_proto_connected(p, now_ms) ? "live" : "none";
}

void zbuddy_proto_set_demo(struct zbuddy_proto *p, bool on, uint32_t now_ms)
{
	p->demo = on;
	if (on) {
		p->demo_idx = 0;
		p->demo_next_ms = now_ms + DEMO_PERIOD_MS;
	}
}

bool zbuddy_proto_demo(const struct zbuddy_proto *p)
{
	return p->demo;
}

void zbuddy_proto_poll(struct zbuddy_proto *p, uint32_t now_ms)
{
	struct zbuddy_snapshot *s = &p->snap;

	if (p->demo) {
		const struct demo_scenario *d;

		if ((int32_t)(now_ms - p->demo_next_ms) >= 0) {
			p->demo_idx = (uint8_t)((p->demo_idx + 1) % (sizeof(demos) / sizeof(demos[0])));
			p->demo_next_ms = now_ms + DEMO_PERIOD_MS;
		}
		d = &demos[p->demo_idx];
		s->sessions_total = d->total;
		s->sessions_running = d->running;
		s->sessions_waiting = d->waiting;
		s->recently_completed = d->completed;
		s->tokens_today = d->tokens;
		s->last_update_ms = now_ms;
		s->connected = true;
		snprintf(s->msg, sizeof(s->msg), "demo: %s", d->name);
		return;
	}
	s->connected = zbuddy_proto_connected(p, now_ms);
	if (!s->connected) {
		s->sessions_total = 0;
		s->sessions_running = 0;
		s->sessions_waiting = 0;
		s->recently_completed = false;
		s->last_update_ms = now_ms;
		strncpy(s->msg, "No Claude connected", sizeof(s->msg) - 1);
		s->msg[sizeof(s->msg) - 1] = '\0';
	}
}

bool zbuddy_proto_xfer_active(const struct zbuddy_proto *p)
{
	return p->xfer_active;
}

uint32_t zbuddy_proto_xfer_done(const struct zbuddy_proto *p)
{
	return p->xfer_total_written;
}

uint32_t zbuddy_proto_xfer_total(const struct zbuddy_proto *p)
{
	return p->xfer_total;
}

bool zbuddy_proto_rtc_valid(const struct zbuddy_proto *p)
{
	return p->rtc_valid;
}
