/*
 * Copyright (c) 2026 Muhammad Waleed Badar
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * Unit tests of the zbuddy library: JSON scanner, protocol, folder push,
 * stats, character manifests and the ASCII species.
 */
#include <zbuddy/art.h>
#include <zbuddy/character.h>
#include <zbuddy/proto.h>
#include <zbuddy/stats.h>
#include "zbuddy_json.h"

#include <string.h>
#include <zephyr/ztest.h>

/* --- a fake bridge, storage and device ----------------------------------- */

static char sent[16][512];
static int n_sent;
static char pet_name[ZBUDDY_NAME_LEN] = "Buddy";
static char owner[ZBUDDY_OWNER_LEN];
static int species_set = -1;
static int unpaired;
static int64_t synced_epoch;
static int32_t synced_tz;

static void fake_send(const char *line, size_t len)
{
	if (n_sent < 16) {
		size_t n = len < sizeof(sent[0]) - 1 ? len : sizeof(sent[0]) - 1;

		memcpy(sent[n_sent], line, n);
		sent[n_sent][n] = '\0';
		n_sent++;
	}
}

static void fake_set_name(const char *n)
{
	strncpy(pet_name, n, sizeof(pet_name) - 1);
}
static const char *fake_get_name(void)
{
	return pet_name;
}
static void fake_set_owner(const char *n)
{
	strncpy(owner, n, sizeof(owner) - 1);
}
static const char *fake_get_owner(void)
{
	return owner;
}
static void fake_set_species(uint8_t idx)
{
	species_set = idx;
}
static void fake_unpair(void)
{
	unpaired++;
}
static void fake_time_sync(int64_t epoch, int32_t tz)
{
	synced_epoch = epoch;
	synced_tz = tz;
}
static void fake_sys_info(struct zbuddy_sys_info *info)
{
	info->uptime_s = 42;
	info->heap_free = 1000;
	info->fs_free = 2000;
	info->fs_total = 4000;
}
static bool fake_secure(void)
{
	return true;
}

/* RAM storage: one file at a time, all bytes concatenated with markers */
static char store_name[32];
static uint32_t store_total;
static char store_file[64];
static uint8_t store_data[4096];
static size_t store_len;
static int store_ended;
static uint32_t store_free = 3 * 1024 * 1024;

static void st_space(uint32_t *f, uint32_t *t)
{
	*f = store_free;
	*t = 4 * 1024 * 1024;
}
static uint32_t st_reclaim(void)
{
	return 0;
}
static int st_begin(const char *name, uint32_t total)
{
	strncpy(store_name, name, sizeof(store_name) - 1);
	store_total = total;
	store_len = 0;
	return 0;
}
static int st_open(const char *path, uint32_t size)
{
	ARG_UNUSED(size);
	strncpy(store_file, path, sizeof(store_file) - 1);
	return 0;
}
static int st_write(const void *data, size_t len)
{
	if (store_len + len > sizeof(store_data)) {
		return -1;
	}
	memcpy(store_data + store_len, data, len);
	store_len += len;
	return 0;
}
static int st_close(void)
{
	return 0;
}
static int st_end(const char *name)
{
	store_ended++;
	return strcmp(name, store_name) == 0 ? 0 : -1;
}

static const struct zbuddy_store store = {
	.space = st_space,
	.reclaimable = st_reclaim,
	.begin = st_begin,
	.open = st_open,
	.write = st_write,
	.close = st_close,
	.end = st_end,
};

static const struct zbuddy_proto_ops ops = {
	.send = fake_send,
	.set_name = fake_set_name,
	.get_name = fake_get_name,
	.set_owner = fake_set_owner,
	.get_owner = fake_get_owner,
	.set_species = fake_set_species,
	.unpair = fake_unpair,
	.time_sync = fake_time_sync,
	.sys_info = fake_sys_info,
	.secure = fake_secure,
	.store = &store,
};

static struct zbuddy_proto proto;
static struct zbuddy_stats_data saved;
static int n_saved;

static void stats_save(const struct zbuddy_stats_data *d)
{
	saved = *d;
	n_saved++;
}

static void feed(const char *s, uint32_t now)
{
	zbuddy_proto_feed(&proto, (const uint8_t *)s, strlen(s), now);
}

static void reset(void *f)
{
	ARG_UNUSED(f);
	n_sent = 0;
	memset(sent, 0, sizeof(sent));
	strcpy(pet_name, "Buddy");
	owner[0] = '\0';
	species_set = -1;
	unpaired = 0;
	store_len = 0;
	store_ended = 0;
	store_free = 3 * 1024 * 1024;
	n_saved = 0;
	zbuddy_stats_init(NULL, stats_save, 1000);
	zbuddy_proto_init(&proto, &ops);
}

ZTEST_SUITE(zbuddy, NULL, NULL, reset, NULL, NULL);

/* --- JSON scanner ---------------------------------------------------------- */

struct kv_rec {
	char keys[8][16];
	enum zbuddy_jtype types[8];
	int n;
};

static bool rec_member(const struct zbuddy_jkv *kv, void *ctx)
{
	struct kv_rec *r = ctx;

	zbuddy_json_str(&kv->key, r->keys[r->n], 16);
	r->types[r->n] = kv->value.type;
	r->n++;
	return true;
}

ZTEST(zbuddy, test_json_scanner)
{
	const char *s = "{\"a\":1, \"b\":\"x{y}\", \"c\":[1,{\"d\":[2]}], \"e\":{\"f\":true},"
			" \"g\":null,\"h\":-3.5e2 }";
	struct kv_rec r = {.n = 0};

	zassert_equal(zbuddy_json_obj_each(s, strlen(s), rec_member, &r), 6);
	zassert_str_equal(r.keys[1], "b");
	zassert_equal(r.types[0], ZBUDDY_J_NUMBER);
	zassert_equal(r.types[1], ZBUDDY_J_STRING);
	zassert_equal(r.types[2], ZBUDDY_J_ARRAY);
	zassert_equal(r.types[3], ZBUDDY_J_OBJECT);
	zassert_equal(r.types[4], ZBUDDY_J_NULL);
	zassert_equal(r.types[5], ZBUDDY_J_NUMBER);
	zassert_true(zbuddy_json_obj_each("{", 1, rec_member, &r) < 0);
	zassert_true(zbuddy_json_obj_each("[1]", 3, rec_member, &r) < 0);
	zassert_equal(zbuddy_json_obj_each("{}", 2, rec_member, &r), 0);
}

ZTEST(zbuddy, test_json_strings)
{
	const char *s = "{\"t\":\"a\\\"b\\\\c\\n\\u00e9\\ud83d\\ude00\"}";
	struct zbuddy_jkv got;
	char out[32];
	struct kv_rec r = {.n = 0};

	zassert_equal(zbuddy_json_obj_each(s, strlen(s), rec_member, &r), 1);
	/* grab the value again to unescape it */
	got.value.type = ZBUDDY_J_STRING;
	got.value.start = strchr(s, ':') + 2;
	got.value.end = s + strlen(s) - 2;
	zassert_equal(zbuddy_json_str(&got.value, out, sizeof(out)), 12);
	zassert_mem_equal(out, "a\"b\\c\n\xc3\xa9\xf0\x9f\x98\x80", 13);
	/* truncation keeps the terminator */
	zbuddy_json_str(&got.value, out, 4);
	zassert_equal(strlen(out), 3);
	zassert_equal(zbuddy_json_escape("x\"y\\z\n", out, sizeof(out)), 9);
	zassert_str_equal(out, "x\\\"y\\\\z\\n");
}

/* --- heartbeat snapshots --------------------------------------------------- */

ZTEST(zbuddy, test_heartbeat)
{
	feed("{\"total\":3,\"running\":2,\"waiting\":1,\"msg\":\"Editing main.c\","
	     "\"entries\":[\"user: fix it\",\"assistant: on it\"],\"tokens\":500,"
	     "\"tokens_today\":1234,\"prompt\":{\"id\":\"p1\",\"tool\":\"Bash\","
	     "\"hint\":\"rm -rf build\"}}\n",
	     1000);
	zbuddy_proto_poll(&proto, 1001);
	zassert_true(proto.snap.connected);
	zassert_equal(proto.snap.sessions_total, 3);
	zassert_equal(proto.snap.sessions_running, 2);
	zassert_equal(proto.snap.sessions_waiting, 1);
	zassert_equal(proto.snap.tokens_today, 1234);
	zassert_str_equal(proto.snap.msg, "Editing main.c");
	zassert_equal(proto.snap.n_lines, 2);
	zassert_str_equal(proto.snap.lines[1], "assistant: on it");
	zassert_equal(proto.snap.line_gen, 1);
	zassert_str_equal(proto.snap.prompt.id, "p1");
	zassert_str_equal(proto.snap.prompt.tool, "Bash");
	zassert_str_equal(proto.snap.prompt.hint, "rm -rf build");
	zassert_equal(zbuddy_derive(&proto.snap), ZBUDDY_ATTENTION);

	/* same entries again: no new generation; prompt gone when absent */
	feed("{\"total\":3,\"running\":0,\"waiting\":0,\"entries\":[\"user: fix it\","
	     "\"assistant: on it\"]}\n",
	     2000);
	zassert_equal(proto.snap.line_gen, 1);
	zassert_equal(proto.snap.prompt.id[0], '\0');
	zassert_equal(zbuddy_derive(&proto.snap), ZBUDDY_IDLE);

	feed("{\"running\":3}\n", 2500);
	zassert_equal(zbuddy_derive(&proto.snap), ZBUDDY_BUSY);
	feed("{\"running\":0,\"completed\":true}\n", 2600);
	zassert_equal(zbuddy_derive(&proto.snap), ZBUDDY_CELEBRATE);

	/* silence for more than 30 s: disconnected and zeroed */
	zbuddy_proto_poll(&proto, 2600 + 31000);
	zassert_false(proto.snap.connected);
	zassert_equal(proto.snap.sessions_total, 0);
	zassert_str_equal(proto.snap.msg, "No Claude connected");
	zassert_equal(zbuddy_derive(&proto.snap), ZBUDDY_IDLE);
	zassert_equal(n_sent, 0, "heartbeats are not acked");
}

ZTEST(zbuddy, test_feed_fragments)
{
	/* Notifications split a line at the MTU: reassembled across feeds */
	feed("{\"tot", 10);
	feed("al\":5,\"run", 20);
	feed("ning\":1}\r\n{\"waiting\":2}\n", 30);
	zassert_equal(proto.snap.sessions_total, 5);
	zassert_equal(proto.snap.sessions_waiting, 2);
	/* garbage and blank lines are ignored */
	feed("\n\nhello world\n{bad json\n", 40);
	zassert_equal(proto.snap.sessions_total, 5);
	/* an overlong line is dropped, the next one parsed */
	for (int i = 0; i < CONFIG_ZBUDDY_LINE_MAX / 8 + 2; i++) {
		feed("{\"a\":1}", 50);
	}
	feed("\n{\"total\":7}\n", 60);
	zassert_equal(proto.snap.sessions_total, 7);
}

ZTEST(zbuddy, test_turn_event_and_time)
{
	feed("{\"evt\":\"turn\",\"role\":\"assistant\",\"content\":[{\"type\":\"text\","
	     "\"text\":\"Done,\\nall tests pass\"}]}\n",
	     100);
	zassert_equal(proto.snap.n_lines, 1);
	zassert_str_equal(proto.snap.lines[0], "assistant: Done, all tests pass");
	for (int i = 0; i < CONFIG_ZBUDDY_LINES + 2; i++) {
		feed("{\"evt\":\"turn\",\"role\":\"user\",\"content\":\"hi\"}\n", 200);
	}
	zassert_equal(proto.snap.n_lines, CONFIG_ZBUDDY_LINES);
	zassert_str_equal(proto.snap.lines[CONFIG_ZBUDDY_LINES - 1], "user: hi");

	zassert_false(zbuddy_proto_rtc_valid(&proto));
	feed("{\"time\":[1760000000,3600]}\n", 300);
	zassert_true(zbuddy_proto_rtc_valid(&proto));
	zassert_equal(synced_epoch, 1760000000);
	zassert_equal(synced_tz, 3600);
}

/* --- commands -------------------------------------------------------------- */

ZTEST(zbuddy, test_commands)
{
	feed("{\"cmd\":\"name\",\"name\":\"Pixel\"}\n", 1);
	zassert_str_equal(pet_name, "Pixel");
	zassert_str_equal(sent[0], "{\"ack\":\"name\",\"ok\":true,\"n\":0}\n");
	feed("{\"cmd\":\"name\"}\n", 1);
	zassert_str_equal(sent[1], "{\"ack\":\"name\",\"ok\":false,\"n\":0}\n");
	feed("{\"cmd\":\"owner\",\"name\":\"Walid\"}\n", 1);
	zassert_str_equal(owner, "Walid");
	zassert_str_equal(sent[2], "{\"ack\":\"owner\",\"ok\":true,\"n\":0}\n");
	feed("{\"cmd\":\"species\",\"idx\":4}\n", 1);
	zassert_equal(species_set, 4);
	feed("{\"cmd\":\"unpair\"}\n", 1);
	zassert_equal(unpaired, 1);
	zassert_str_equal(sent[4], "{\"ack\":\"unpair\",\"ok\":true,\"n\":0}\n");

	zbuddy_stats_on_approval(3);
	zbuddy_stats_on_denial();
	feed("{\"cmd\":\"status\"}\n", 1);
	zassert_equal(n_sent, 6);
	zassert_not_null(strstr(sent[5], "{\"ack\":\"status\",\"ok\":true,\"n\":0,\"data\":{"));
	zassert_not_null(strstr(sent[5], "\"name\":\"Pixel\",\"owner\":\"Walid\",\"sec\":true,"));
	zassert_not_null(strstr(sent[5], "\"sys\":{\"up\":42,\"heap\":1000,\"fsFree\":2000,\"fsTotal\":4000}"));
	zassert_not_null(strstr(sent[5], "\"stats\":{\"appr\":1,\"deny\":1,\"vel\":3,\"nap\":0,\"lvl\":0}}}\n"));
	zassert_is_null(strstr(sent[5], "\"bat\""), "no battery, no bat object");

	/* a permission cmd from the desktop is not ours; nothing sent */
	feed("{\"cmd\":\"permission\",\"id\":\"x\",\"decision\":\"once\"}\n", 1);
	zassert_equal(n_sent, 6);

	zbuddy_proto_permission(&proto, "p\"1", true);
	zassert_str_equal(sent[6], "{\"cmd\":\"permission\",\"id\":\"p\\\"1\",\"decision\":\"once\"}\n");
	zbuddy_proto_permission(&proto, "p2", false);
	zassert_str_equal(sent[7], "{\"cmd\":\"permission\",\"id\":\"p2\",\"decision\":\"deny\"}\n");
}

/* --- folder push ----------------------------------------------------------- */

ZTEST(zbuddy, test_folder_push)
{
	/* "hello buddy!" = aGVsbG8gYnVkZHkh */
	feed("{\"cmd\":\"char_begin\",\"name\":\"bufo\",\"total\":12}\n", 1);
	zassert_true(zbuddy_proto_xfer_active(&proto));
	zassert_str_equal(store_name, "bufo");
	zassert_str_equal(sent[0], "{\"ack\":\"char_begin\",\"ok\":true,\"n\":0}\n");
	feed("{\"cmd\":\"file\",\"path\":\"idle.gif\",\"size\":12}\n", 1);
	zassert_str_equal(store_file, "idle.gif");
	zassert_str_equal(sent[1], "{\"ack\":\"file\",\"ok\":true,\"n\":0}\n");
	feed("{\"cmd\":\"chunk\",\"d\":\"aGVsbG8g\"}\n", 1);
	zassert_str_equal(sent[2], "{\"ack\":\"chunk\",\"ok\":true,\"n\":6}\n");
	feed("{\"cmd\":\"chunk\",\"d\":\"YnVkZHkh\"}\n", 1);
	zassert_str_equal(sent[3], "{\"ack\":\"chunk\",\"ok\":true,\"n\":12}\n");
	zassert_equal(zbuddy_proto_xfer_done(&proto), 12);
	zassert_equal(zbuddy_proto_xfer_total(&proto), 12);
	feed("{\"cmd\":\"file_end\"}\n", 1);
	zassert_str_equal(sent[4], "{\"ack\":\"file_end\",\"ok\":true,\"n\":12}\n");
	zassert_mem_equal(store_data, "hello buddy!", 12);
	/* a path escaping the pack directory is refused */
	feed("{\"cmd\":\"file\",\"path\":\"../x\",\"size\":1}\n", 1);
	zassert_not_null(strstr(sent[5], "\"ok\":false"));
	feed("{\"cmd\":\"file\",\"path\":\"/etc/passwd\",\"size\":1}\n", 1);
	zassert_not_null(strstr(sent[6], "\"ok\":false"));
	feed("{\"cmd\":\"char_end\"}\n", 1);
	zassert_str_equal(sent[7], "{\"ack\":\"char_end\",\"ok\":true,\"n\":0}\n");
	zassert_false(zbuddy_proto_xfer_active(&proto));
	zassert_equal(store_ended, 1);
	zassert_equal(species_set, 0xFF, "the installed pack is selected");

	/* without a transfer, file commands are ignored */
	n_sent = 0;
	feed("{\"cmd\":\"chunk\",\"d\":\"aGVsbG8g\"}\n", 1);
	zassert_equal(n_sent, 0);

	/* too big for the storage */
	store_free = 1024;
	feed("{\"cmd\":\"char_begin\",\"name\":\"big\",\"total\":500000}\n", 1);
	zassert_not_null(strstr(sent[0], "\"ack\":\"char_begin\",\"ok\":false"));
	zassert_not_null(strstr(sent[0], "need 488K, have 1K"));
	zassert_false(zbuddy_proto_xfer_active(&proto));
	/* and bigger than the protocol allows */
	feed("{\"cmd\":\"char_begin\",\"name\":\"huge\",\"total\":99999999}\n", 1);
	zassert_not_null(strstr(sent[1], "too large"));
}

ZTEST(zbuddy, test_demo)
{
	zbuddy_proto_set_demo(&proto, true, 0);
	zbuddy_proto_poll(&proto, 0);
	zassert_true(proto.snap.connected);
	zassert_str_equal(proto.snap.msg, "demo: asleep");
	zbuddy_proto_poll(&proto, 8001);
	zassert_str_equal(proto.snap.msg, "demo: one idle");
	zassert_equal(proto.snap.sessions_total, 1);
	zbuddy_proto_poll(&proto, 16002);
	zassert_equal(zbuddy_derive(&proto.snap), ZBUDDY_BUSY);
	zassert_str_equal(zbuddy_proto_source(&proto, 16002), "busy");
	zbuddy_proto_set_demo(&proto, false, 16002);
	zbuddy_proto_poll(&proto, 16003);
	zassert_false(proto.snap.connected);
	zassert_str_equal(zbuddy_proto_source(&proto, 16003), "none");
}

/* --- stats ----------------------------------------------------------------- */

ZTEST(zbuddy, test_stats)
{
	const struct zbuddy_stats_data *s = zbuddy_stats_get();

	zassert_equal(zbuddy_stats_median_velocity(), 0);
	zassert_equal(zbuddy_stats_mood_tier(), 2, "neutral without data");
	zbuddy_stats_on_approval(10);
	zbuddy_stats_on_approval(40);
	zbuddy_stats_on_approval(20);
	zassert_equal(zbuddy_stats_median_velocity(), 20);
	zassert_equal(zbuddy_stats_mood_tier(), 3);
	zassert_equal(n_saved, 3);
	zbuddy_stats_on_denial();
	zbuddy_stats_on_denial();
	zassert_equal(s->denials, 2);
	zassert_equal(zbuddy_stats_mood_tier(), 2, "deny rate above a third");
	zbuddy_stats_on_denial();
	zbuddy_stats_on_denial();
	zassert_equal(zbuddy_stats_mood_tier(), 1, "more denials than approvals");

	/* tokens: first sight latches, then deltas; a drop resyncs */
	zbuddy_stats_on_bridge_tokens(30000);
	zassert_equal(s->tokens, 0);
	zbuddy_stats_on_bridge_tokens(30000 + 49999);
	zassert_equal(s->tokens, 49999);
	zassert_equal(s->level, 0);
	zassert_false(zbuddy_stats_poll_level_up());
	n_saved = 0;
	zbuddy_stats_on_bridge_tokens(30000 + 50001);
	zassert_equal(s->level, 1);
	zassert_true(zbuddy_stats_poll_level_up());
	zassert_false(zbuddy_stats_poll_level_up());
	zassert_equal(n_saved, 1, "saved on the milestone only");
	zbuddy_stats_on_bridge_tokens(100);
	zassert_equal(s->tokens, 50001, "a restart does not credit");
	zbuddy_stats_on_bridge_tokens(5100);
	zassert_equal(s->tokens, 55001);
	zassert_equal(zbuddy_stats_fed_progress(), 1);

	zassert_equal(zbuddy_stats_energy_tier(1000), 3);
	zassert_equal(zbuddy_stats_energy_tier(1000 + 2 * 3600000U), 2);
	zbuddy_stats_on_nap_end(600, 50000);
	zassert_equal(s->nap_seconds, 600);
	zassert_equal(zbuddy_stats_energy_tier(50000), 5);
	zassert_equal(zbuddy_stats_energy_tier(50000 + 5 * 3600000U), 3);

	/* an old record with a level but no tokens is backfilled */
	{
		struct zbuddy_stats_data old = {.level = 2, .vel_idx = 200};

		zbuddy_stats_init(&old, stats_save, 0);
		zassert_equal(zbuddy_stats_get()->tokens, 100000);
		zassert_equal(zbuddy_stats_get()->vel_idx, 0);
	}
	zbuddy_stats_reset(0);
	zassert_equal(zbuddy_stats_get()->level, 0);
}

/* --- character manifests --------------------------------------------------- */

ZTEST(zbuddy, test_character_manifest)
{
	char gif[] = "{\"name\":\"bufo\",\"colors\":{\"body\":\"#6B8E23\",\"bg\":\"#000000\","
		     "\"text\":\"#FFFFFF\",\"textDim\":\"#808080\",\"ink\":\"#000000\"},"
		     "\"states\":{\"sleep\":\"sleep.gif\",\"idle\":[\"idle_0.gif\",\"idle_1.gif\"],"
		     "\"busy\":\"busy.gif\",\"attention\":\"attention.gif\",\"celebrate\":\"c.gif\","
		     "\"dizzy\":\"dizzy.gif\",\"heart\":\"heart.gif\"}}";
	char text[] = "{\"name\":\"txt\",\"states\":{\"idle\":{\"frames\":[\"(o_o)\",\"(-_-)\"],"
		      "\"delay\":300}},\"mode\":\"text\"}";
	char bad[] = "{\"name\":\"empty\"}";
	struct zbuddy_character ch;

	zassert_equal(zbuddy_character_parse(gif, strlen(gif), &ch), 0);
	zassert_str_equal(ch.name, "bufo");
	zassert_false(ch.text_mode);
	zassert_equal(ch.palette.body, 0x6C64, "#6B8E23 in RGB565");
	zassert_equal(ch.palette.text, 0xFFFF);
	zassert_equal(ch.palette.text_dim, 0x8410);
	zassert_equal(ch.n_gifs, 8);
	zassert_equal(ch.state_count[ZBUDDY_SLEEP], 1);
	zassert_equal(ch.state_count[ZBUDDY_IDLE], 2);
	zassert_str_equal(ch.gifs[ch.state_start[ZBUDDY_IDLE] + 1], "idle_1.gif");
	zassert_str_equal(ch.gifs[ch.state_start[ZBUDDY_HEART]], "heart.gif");

	zassert_equal(zbuddy_character_parse(text, strlen(text), &ch), 0);
	zassert_true(ch.text_mode);
	zassert_equal(ch.text[ZBUDDY_IDLE].n_frames, 2);
	zassert_equal(ch.text[ZBUDDY_IDLE].delay_ms, 300);
	zassert_str_equal(ch.text[ZBUDDY_IDLE].frames[1], "(-_-)");
	zassert_equal(ch.text[ZBUDDY_SLEEP].n_frames, 0);
	zassert_equal(ch.palette.body, zbuddy_default_palette.body);

	zassert_equal(zbuddy_character_parse(bad, strlen(bad), &ch), -EINVAL);
	zassert_equal(zbuddy_parse_hex_color("#FF0000", 1), 0xF800);
	zassert_equal(zbuddy_parse_hex_color("zz", 7), 7);
}

/* --- species --------------------------------------------------------------- */

static int prints, cur_x, cur_y, min_x, max_x, min_y, max_y;
static uint16_t cur_color;

static void cv_color(uint16_t c)
{
	cur_color = c;
}
static void cv_cursor(int x, int y)
{
	cur_x = x;
	cur_y = y;
}
static void cv_print(const char *s)
{
	int n = (int)strlen(s);

	/* track the ink extent: leading/trailing spaces do not count */
	for (int i = 0; i < n; i++) {
		if (s[i] != ' ') {
			int x = cur_x + i * ZBUDDY_ART_CHAR_W;

			if (x < min_x) {
				min_x = x;
			}
			if (x + ZBUDDY_ART_CHAR_W > max_x) {
				max_x = x + ZBUDDY_ART_CHAR_W;
			}
			if (cur_y < min_y) {
				min_y = cur_y;
			}
			if (cur_y + ZBUDDY_ART_CHAR_H > max_y) {
				max_y = cur_y + ZBUDDY_ART_CHAR_H;
			}
		}
	}
	cur_x += n * ZBUDDY_ART_CHAR_W;
	prints++;
}

static const struct zbuddy_art_ops canvas = {
	.set_color = cv_color,
	.set_cursor = cv_cursor,
	.print = cv_print,
};

ZTEST(zbuddy, test_species)
{
	zassert_equal(zbuddy_species_count(), 19);
	zassert_equal(zbuddy_species_find("clawd"), 0);
	zassert_equal(zbuddy_species_find("cat"), 5);
	zassert_equal(zbuddy_species_find("chonk"), 18);
	zassert_equal(zbuddy_species_find("unicorn"), -1);
	zbuddy_art_bind(&canvas);
	for (size_t i = 0; i < zbuddy_species_count(); i++) {
		const struct zbuddy_species *sp = zbuddy_species_get(i);

		zassert_not_null(sp->name);
		for (int s = 0; s < ZBUDDY_PERSONA_COUNT; s++) {
			zassert_not_null(sp->states[s], "%s has no state %d", sp->name, s);
			prints = 0;
			min_x = min_y = 1000;
			max_x = max_y = -1000;
			for (uint32_t t = 0; t < 400; t++) {
				zbuddy_art_render(sp, (enum zbuddy_persona)s, t);
			}
			zassert_true(prints >= 400 * 5, "%s/%d drew nothing", sp->name, s);
			/* the body stays on the canvas; particles may overshoot a little */
			zassert_true(min_x >= -ZBUDDY_ART_CHAR_W * 2 &&
					     max_x <= ZBUDDY_ART_CANVAS_W + ZBUDDY_ART_CHAR_W * 2,
				     "%s/%d x range %d..%d", sp->name, s, min_x, max_x);
			zassert_true(max_y <= ZBUDDY_ART_CANVAS_H + ZBUDDY_ART_CHAR_H,
				     "%s/%d y range %d..%d", sp->name, s, min_y, max_y);
		}
	}
	zassert_equal(zbuddy_persona_name(ZBUDDY_HEART), "heart");
}
