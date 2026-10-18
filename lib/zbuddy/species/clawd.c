/*
 * Copyright (c) 2026 Muhammad Waleed Badar
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * clawd: a retro crab, the one species that is not from claude-desktop-buddy.
 * Seven states like the others; the art is 12 columns wide.
 */
#include <zbuddy/art.h>

#include <stdint.h>
#include <string.h>

#define SHELL 0xDBAA /* #DA7756 */

/* --- SLEEP: flat on the sand, breathing, Zs drifting ----------------------- */
static void doSleep(uint32_t t)
{
	static const char *const FLAT[5] = {"            ", "            ", "  _  ___  _ ",
					    " (_)(===)(_)", "  /|/| |\\|\\ "};
	static const char *const BREATHE[5] = {"            ", "            ", "  _ _____ _ ",
					       " (_)(===)(_)", "  /|/| |\\|\\ "};
	static const char *const TWITCH[5] = {"            ", "            ", "  _  ___  _ ",
					      " (_)(===)(_)", "  /|/| |\\|\\~"};
	const char *const *P[3] = {FLAT, BREATHE, TWITCH};
	static const uint8_t SEQ[] = {0, 0, 1, 1, 0, 0, 1, 1, 2, 0, 0, 1, 1, 0, 2, 2};
	uint8_t beat = (t / 5) % sizeof(SEQ);
	int p1 = t % 12, p2 = (t + 5) % 12, p3 = (t + 9) % 12;

	zbuddy_art_sprite(P[SEQ[beat]], 5, 0, SHELL, 0);
	zbuddy_art_color(ZBUDDY_ART_DIM);
	zbuddy_art_cursor(ZBUDDY_ART_X_CENTER + 18 + p1, ZBUDDY_ART_Y_OVERLAY + 18 - p1 * 2);
	zbuddy_art_print("z");
	zbuddy_art_color(ZBUDDY_ART_WHITE);
	zbuddy_art_cursor(ZBUDDY_ART_X_CENTER + 24 + p2, ZBUDDY_ART_Y_OVERLAY + 14 - p2);
	zbuddy_art_print("Z");
	zbuddy_art_color(ZBUDDY_ART_DIM);
	zbuddy_art_cursor(ZBUDDY_ART_X_CENTER + 14 + p3 / 2, ZBUDDY_ART_Y_OVERLAY + 8 - p3 / 2);
	zbuddy_art_print("z");
}

/* --- IDLE: eyes on stalks looking around, a claw wave, a sidestep ---------- */
static void doIdle(uint32_t t)
{
	static const char *const REST[5] = {"            ", "    o   o   ", " \\/ |___| \\/",
					    " (_(=====)_)", "  /|/| |\\|\\ "};
	static const char *const LOOK_L[5] = {"            ", "   o   o    ", " \\/ |___| \\/",
					      " (_(=====)_)", "  /|/| |\\|\\ "};
	static const char *const LOOK_R[5] = {"            ", "     o   o  ", " \\/ |___| \\/",
					      " (_(=====)_)", "  /|/| |\\|\\ "};
	static const char *const BLINK[5] = {"            ", "    -   -   ", " \\/ |___| \\/",
					     " (_(=====)_)", "  /|/| |\\|\\ "};
	static const char *const WAVE[5] = {"          \\/", "    o   o /", " \\/ |___|/  ",
					    " (_(=====)_)", "  /|/| |\\|\\ "};
	static const char *const STEP[5] = {"            ", "    o   o   ", " \\/ |___| \\/",
					    " (_(=====)_)", " /|/|   |\\|\\"};
	const char *const *P[6] = {REST, LOOK_L, LOOK_R, BLINK, WAVE, STEP};
	static const uint8_t SEQ[] = {0, 0, 0, 3, 0, 1, 1, 0, 2, 2, 0, 0, 4, 4, 0, 3,
				      0, 5, 0, 5, 0, 0, 1, 0, 2, 0, 3, 0};
	static const int8_t X_SHIFT[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
					 0, 0, 0, -3, -6, -3, 0, 0, 0, 0, 0, 0, 0, 0};
	uint8_t beat = (t / 5) % sizeof(SEQ);

	zbuddy_art_sprite(P[SEQ[beat]], 5, 0, SHELL, X_SHIFT[beat]);
}

/* --- BUSY: scuttling left and right, clicking the claws -------------------- */
static void doBusy(uint32_t t)
{
	static const char *const RUN_A[5] = {"            ", "    o   o   ", " \\/ |___| \\/",
					     " (_(=====)_)", " /|/|   |\\|\\"};
	static const char *const RUN_B[5] = {"            ", "    o   o   ", " \\/ |___| \\/",
					     " (_(=====)_)", "  /|/| |\\|\\ "};
	static const char *const CLICK[5] = {" \\/      \\/ ", "  \\ o   o / ", "   \\|___|/  ",
					     " (_(=====)_)", "  /|/| |\\|\\ "};
	const char *const *P[3] = {RUN_A, RUN_B, CLICK};
	static const uint8_t SEQ[] = {0, 1, 0, 1, 0, 1, 2, 2, 0, 1, 0, 1, 0, 1, 2, 2};
	static const int8_t X_SHIFT[] = {-8, -6, -4, -2, 0, 2, 4, 4, 4, 2, 0, -2, -4, -6, -8, -8};
	static const char *const DOTS[] = {".  ", ".. ", "...", " ..", "  .", "   "};
	uint8_t beat = (t / 2) % sizeof(SEQ);

	zbuddy_art_sprite(P[SEQ[beat]], 5, 0, SHELL, X_SHIFT[beat]);
	zbuddy_art_color(ZBUDDY_ART_WHITE);
	zbuddy_art_cursor(ZBUDDY_ART_X_CENTER + 22, ZBUDDY_ART_Y_OVERLAY + 14);
	zbuddy_art_print(DOTS[t % 6]);
}

/* --- ATTENTION: claws up, eyes wide, bouncing ------------------------------ */
static void doAttention(uint32_t t)
{
	static const char *const UP[5] = {" \\/      \\/ ", "  \\ O   O / ", "   \\|___|/  ",
					  " (_(=====)_)", "  /|/| |\\|\\ "};
	static const char *const UP_HI[5] = {" \\/      \\/ ", "  | O   O | ", "   \\|___|/  ",
					     " (_(=====)_)", "  /|/| |\\|\\ "};
	static const char *const SCAN_L[5] = {" \\/      \\/ ", "  \\O   O  / ", "   \\|___|/  ",
					      " (_(=====)_)", "  /|/| |\\|\\ "};
	static const char *const SCAN_R[5] = {" \\/      \\/ ", "  \\  O   O/ ", "   \\|___|/  ",
					      " (_(=====)_)", "  /|/| |\\|\\ "};
	const char *const *P[4] = {UP, UP_HI, SCAN_L, SCAN_R};
	static const uint8_t SEQ[] = {0, 1, 0, 1, 2, 2, 0, 3, 3, 0, 1, 0, 1, 1};
	static const int8_t Y_SHIFT[] = {0, -2, 0, -2, 0, 0, 0, 0, 0, 0, -2, 0, -2, -2};
	uint8_t beat = (t / 3) % sizeof(SEQ);

	zbuddy_art_sprite(P[SEQ[beat]], 5, Y_SHIFT[beat], SHELL, 0);
	if ((t / 2) & 1) {
		zbuddy_art_color(ZBUDDY_ART_YEL);
		zbuddy_art_cursor(ZBUDDY_ART_X_CENTER - 4, ZBUDDY_ART_Y_OVERLAY);
		zbuddy_art_print("!");
	}
	if ((t / 3) & 1) {
		zbuddy_art_color(ZBUDDY_ART_YEL);
		zbuddy_art_cursor(ZBUDDY_ART_X_CENTER + 4, ZBUDDY_ART_Y_OVERLAY + 4);
		zbuddy_art_print("!");
	}
}

/* --- CELEBRATE: jumping with the claws up, confetti ------------------------ */
static void doCelebrate(uint32_t t)
{
	static const char *const CROUCH[5] = {"            ", "    ^   ^   ", " \\/ |___| \\/",
					      " (_(=====)_)", " /|/|___|\\|\\"};
	static const char *const JUMP[5] = {" \\/      \\/ ", "  \\ ^   ^ / ", "   \\|___|/  ",
					    " (_(=====)_)", "  /|/| |\\|\\ "};
	static const char *const PEAK[5] = {" \\/      \\/ ", "  \\ *   * / ", "   \\|___|/  ",
					    " (_(=====)_)", "   |/| |\\|  "};
	static const char *const SNAP[5] = {" \\/      \\/ ", "  \\ ^   ^ / ", "   \\|___|/  ",
					    " (_(=====)_)", "  /|/| |\\|\\ "};
	const char *const *P[4] = {CROUCH, JUMP, PEAK, SNAP};
	static const uint8_t SEQ[] = {0, 1, 2, 1, 0, 3, 3, 0, 1, 2, 1, 0, 3, 3};
	static const int8_t Y_SHIFT[] = {0, -3, -6, -3, 0, 0, 0, 0, -3, -6, -3, 0, 0, 0};
	static const uint16_t cols[] = {ZBUDDY_ART_YEL, ZBUDDY_ART_HEART, ZBUDDY_ART_CYAN,
					ZBUDDY_ART_WHITE, ZBUDDY_ART_GREEN};
	uint8_t beat = (t / 3) % sizeof(SEQ);

	zbuddy_art_sprite(P[SEQ[beat]], 5, Y_SHIFT[beat], SHELL, 0);
	for (int i = 0; i < 6; i++) {
		int phase = (t * 2 + i * 11) % 22;
		int x = ZBUDDY_ART_X_CENTER - 36 + i * 14;
		int y = ZBUDDY_ART_Y_OVERLAY - 6 + phase;

		if (y > ZBUDDY_ART_Y_BASE + 20 || y < 0) {
			continue;
		}
		zbuddy_art_color(cols[i % 5]);
		zbuddy_art_cursor(x, y);
		zbuddy_art_print((i + (int)(t / 2)) & 1 ? "*" : ".");
	}
}

/* --- DIZZY: wobbling, spiral eyes, orbiting stars -------------------------- */
static void doDizzy(uint32_t t)
{
	static const char *const TILT_L[5] = {"            ", "   @   @    ", "\\/ |___|  \\/",
					      " (_(=====)_)", "  /|/| |\\|\\ "};
	static const char *const TILT_R[5] = {"            ", "     @   @  ", "\\/  |___| \\/",
					      " (_(=====)_)", "  /|/| |\\|\\ "};
	static const char *const WOOZY[5] = {"            ", "    x   @   ", " \\/ |___| \\/",
					     " (_(=====)_)", "  /|/|~|\\|\\ "};
	static const char *const FLIP[5] = {"            ", "    @   x   ", " \\/ |___| \\/",
					    " (_(=====)_)", " ~/|/| |\\|\\ "};
	const char *const *P[4] = {TILT_L, TILT_R, WOOZY, FLIP};
	static const uint8_t SEQ[] = {0, 1, 0, 1, 2, 3, 0, 1, 0, 1, 2, 2, 3, 3};
	static const int8_t X_SHIFT[] = {-3, 3, -3, 3, 0, 0, -3, 3, -3, 3, 0, 0, 0, 0};
	static const int8_t OX[] = {0, 5, 7, 5, 0, -5, -7, -5};
	static const int8_t OY[] = {-5, -3, 0, 3, 5, 3, 0, -3};
	uint8_t beat = (t / 4) % sizeof(SEQ);
	uint8_t p1 = t % 8, p2 = (t + 4) % 8;

	zbuddy_art_sprite(P[SEQ[beat]], 5, 0, SHELL, X_SHIFT[beat]);
	zbuddy_art_color(ZBUDDY_ART_CYAN);
	zbuddy_art_cursor(ZBUDDY_ART_X_CENTER + OX[p1] - 2, ZBUDDY_ART_Y_OVERLAY + 6 + OY[p1]);
	zbuddy_art_print("*");
	zbuddy_art_color(ZBUDDY_ART_YEL);
	zbuddy_art_cursor(ZBUDDY_ART_X_CENTER + OX[p2] - 2, ZBUDDY_ART_Y_OVERLAY + 6 + OY[p2]);
	zbuddy_art_print("*");
}

/* --- HEART: smitten, a claw on the heart, hearts rising -------------------- */
static void doHeart(uint32_t t)
{
	static const char *const DREAMY[5] = {"            ", "    ^   ^   ", " \\/ |___| \\/",
					      " (_(=====)_)", "  /|/| |\\|\\ "};
	static const char *const HOLD[5] = {"            ", "    ^   ^   ", " \\/ |___|   ",
					    " (_(====<)_)", "  /|/| |\\|\\ "};
	static const char *const BLUSH[5] = {"            ", "   #^   ^#  ", " \\/ |___| \\/",
					     " (_(=====)_)", "  /|/| |\\|\\ "};
	static const char *const EYES[5] = {"            ", "   <3   <3  ", " \\/ |___| \\/",
					    " (_(=====)_)", "  /|/| |\\|\\ "};
	const char *const *P[4] = {DREAMY, HOLD, BLUSH, EYES};
	static const uint8_t SEQ[] = {0, 0, 1, 1, 0, 2, 2, 0, 3, 3, 3, 0, 1, 1, 2, 0};
	static const int8_t Y_BOB[] = {0, -1, 0, -1, 0, -1, 0, 0, -1, 0, -1, 0, 0, -1, 0, -1};
	uint8_t beat = (t / 5) % sizeof(SEQ);

	zbuddy_art_sprite(P[SEQ[beat]], 5, Y_BOB[beat], SHELL, 0);
	zbuddy_art_color(ZBUDDY_ART_HEART);
	for (int i = 0; i < 5; i++) {
		int phase = (t + i * 4) % 16;
		int y = ZBUDDY_ART_Y_OVERLAY + 16 - phase;
		int x;

		if (y < -2 || y > ZBUDDY_ART_Y_BASE) {
			continue;
		}
		x = ZBUDDY_ART_X_CENTER - 20 + i * 8 + ((phase / 3) & 1) * 2 - 1;
		zbuddy_art_cursor(x, y);
		zbuddy_art_print("v");
	}
}

const struct zbuddy_species zbuddy_species_clawd = {
	"clawd", SHELL, {doSleep, doIdle, doBusy, doAttention, doCelebrate, doDizzy, doHeart}};
