/*
 * Copyright (c) 2026 Muhammad Waleed Badar
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * native_sim only: command line options for screenshots and scripted
 * bridge traffic.
 *
 *   --shot=<file.ppm>     write the screen after --shot-delay=<ms> (1500)
 *   --species=<n>         show ASCII species n (0..17)
 *   --state=<name>        force a persona: sleep idle busy attention
 *                         celebrate dizzy heart
 *   --screen=<name>       home pet info menu settings reset passkey
 *   --page=<n>            page of the pet/info screen
 *   --demo                demo mode (fake scenarios)
 *   --script=<file>       lines "<ms> <json>": fed as if from the desktop
 *                         when the uptime reaches <ms>
 */
#include "host.h"
#include "bridge.h"
#include "buddy.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <lvgl.h>
#include <posix_native_task.h>
#include <cmdline.h>

static char *shot_path;
static unsigned int shot_delay_ms;
static unsigned int species_no;
static char *state_name;
static char *screen_name;
static unsigned int page_no;
static bool demo;
static char *script_path;

static FILE *script;
static char script_line[CONFIG_ZBUDDY_LINE_MAX + 32];
static long script_at = -1;
static bool configured, shot_done;

static void add_options(void)
{
	static struct args_struct_t options[] = {
		{.option = "shot", .name = "file.ppm", .type = 's', .dest = (void *)&shot_path,
		 .descript = "Write a screenshot (binary PPM) and keep running"},
		{.option = "shot-delay", .name = "ms", .type = 'u', .dest = (void *)&shot_delay_ms,
		 .descript = "Delay before the screenshot (default 1500 ms)"},
		{.option = "species", .name = "n", .type = 'u', .dest = (void *)&species_no,
		 .descript = "Show ASCII species n"},
		{.option = "state", .name = "name", .type = 's', .dest = (void *)&state_name,
		 .descript = "Force a persona state (sleep, idle, busy, attention, celebrate, dizzy, heart)"},
		{.option = "screen", .name = "name", .type = 's', .dest = (void *)&screen_name,
		 .descript = "Open a screen (home, pet, info, menu, settings, reset, passkey)"},
		{.option = "page", .name = "n", .type = 'u', .dest = (void *)&page_no,
		 .descript = "Page of the pet or info screen"},
		{.is_switch = true, .option = "demo", .type = 'b', .dest = (void *)&demo,
		 .descript = "Demo mode: cycle fake scenarios"},
		{.option = "script", .name = "file", .type = 's', .dest = (void *)&script_path,
		 .descript = "Feed '<ms> <json>' lines as if sent by the desktop"},
		ARG_TABLE_ENDMARKER,
	};

	native_add_command_line_opts(options);
}

NATIVE_TASK(add_options, PRE_BOOT_1, 10);

static void configure(uint32_t now_ms)
{
	/* numeric options that were not given are left at UINT32_MAX */
	if (shot_delay_ms == UINT32_MAX) {
		shot_delay_ms = 1500;
	}
	if (species_no != UINT32_MAX) {
		buddy_set_species((uint8_t)species_no);
	}
	if (state_name != NULL) {
		for (int s = 0; s < ZBUDDY_PERSONA_COUNT; s++) {
			if (strcmp(state_name, zbuddy_persona_name(s)) == 0) {
				buddy_force_persona(s);
			}
		}
	}
	if (screen_name != NULL) {
		uint8_t page = page_no == UINT32_MAX ? 0 : (uint8_t)(page_no > 0 ? page_no - 1 : 0);

		if (strcmp(screen_name, "pet") == 0) {
			buddy_debug_show(SCREEN_PET, OVERLAY_NONE, page);
		} else if (strcmp(screen_name, "info") == 0) {
			buddy_debug_show(SCREEN_INFO, OVERLAY_NONE, page);
		} else if (strcmp(screen_name, "menu") == 0) {
			buddy_debug_show(SCREEN_HOME, OVERLAY_MENU, 0);
		} else if (strcmp(screen_name, "settings") == 0) {
			buddy_debug_show(SCREEN_HOME, OVERLAY_SETTINGS, 0);
		} else if (strcmp(screen_name, "reset") == 0) {
			buddy_debug_show(SCREEN_HOME, OVERLAY_RESET, 0);
		} else if (strcmp(screen_name, "passkey") == 0) {
			bridge_debug_passkey(428731);
		}
	}
	if (demo) {
		zbuddy_proto_set_demo(buddy_proto(), true, now_ms);
	}
	if (script_path != NULL) {
		script = fopen(script_path, "r");
		if (script == NULL) {
			printk("host: cannot open %s\n", script_path);
		}
	}
}

static void feed_script(uint32_t now_ms)
{
	while (script != NULL) {
		char *json;

		if (script_at < 0) {
			if (fgets(script_line, sizeof(script_line), script) == NULL) {
				fclose(script);
				script = NULL;
				return;
			}
			if (script_line[0] == '#' || script_line[0] == '\n') {
				continue;
			}
			script_at = strtol(script_line, &json, 10);
			if (json == script_line) {
				script_at = -1;
				continue;
			}
		}
		if ((long)now_ms < script_at) {
			return;
		}
		json = script_line;
		(void)strtol(json, &json, 10);
		while (*json == ' ' || *json == '\t') {
			json++;
		}
		bridge_rx_put((const uint8_t *)json, strlen(json));
		if (json[strlen(json) - 1] != '\n') {
			bridge_rx_put((const uint8_t *)"\n", 1);
		}
		script_at = -1;
	}
}

static void screenshot(void)
{
	lv_draw_buf_t *buf = lv_snapshot_take(lv_screen_active(), LV_COLOR_FORMAT_RGB888);
	FILE *fp;
	uint32_t w, h, stride;

	if (buf == NULL) {
		printk("screenshot: snapshot failed\n");
		return;
	}
	fp = fopen(shot_path, "wb");
	if (fp == NULL) {
		printk("screenshot: cannot write %s\n", shot_path);
		lv_draw_buf_destroy(buf);
		return;
	}
	w = buf->header.w;
	h = buf->header.h;
	stride = buf->header.stride;
	fprintf(fp, "P6\n%u %u\n255\n", w, h);
	for (uint32_t y = 0; y < h; y++) {
		const uint8_t *row = (const uint8_t *)buf->data + y * stride;

		for (uint32_t x = 0; x < w; x++) {
			/* LVGL RGB888 is stored as B, G, R */
			uint8_t px[3] = {row[x * 3 + 2], row[x * 3 + 1], row[x * 3]};

			fwrite(px, 1, 3, fp);
		}
	}
	fclose(fp);
	lv_draw_buf_destroy(buf);
	printk("screenshot: wrote %s (%ux%u)\n", shot_path, w, h);
}

void host_init(void)
{
}

void host_poll(uint32_t now_ms)
{
	if (!configured) {
		configured = true;
		configure(now_ms);
	}
	feed_script(now_ms);
	if (shot_path != NULL && !shot_done && now_ms >= shot_delay_ms) {
		shot_done = true;
		screenshot();
	}
}
