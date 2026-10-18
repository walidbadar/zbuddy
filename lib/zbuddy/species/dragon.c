/*
 * Copyright (c) 2026 Muhammad Waleed Badar
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * The dragon species. Art and animation sequences from claude-desktop-buddy
 * (Copyright 2026 Anthropic, PBC, MIT licence, see LICENSE.claude-desktop-buddy),
 * converted to C by tools/port_species.py.
 */
#include <zbuddy/art.h>

#include <stdint.h>
#include <string.h>

// ─── SLEEP ───  ~12s cycle, 6 poses
static void doSleep(uint32_t t) {
  static const char *const CURL[5]    = { "            ", "            ", "   _____    ", "  (--   )~  ", "  `vvvvv'   " };
  static const char *const BREATH[5]  = { "            ", "            ", "   _____    ", "  (--   )~~ ", "  `vvvvv'   " };
  static const char *const PUFF[5]    = { "            ", "       o    ", "   _____    ", "  (--   )~~ ", "  `vvvvv'   " };
  static const char *const TWITCH[5]  = { "            ", "  /v\\  /v\\  ", " <  --  -- >", " (        ) ", "  `-vvvv-'  " };
  static const char *const SNORE[5]   = { "            ", "  /^\\  /^\\  ", " <  oo  oo >", " (   __   ) ", "  `-vvvv-'  " };
  static const char *const HOARD[5]   = { "            ", "            ", "   _____    ", "  (--   )$  ", "  `vvvvv'$$ " };

  const char *const * P[6] = { CURL, BREATH, PUFF, TWITCH, SNORE, HOARD };
  static const uint8_t SEQ[] = {
    0,1,0,1,0,1,2,1,
    0,1,0,1,
    3,4,3,4,
    1,2,1,
    5,5,0,0,
    1,2,1,2
  };
  uint8_t beat = (t / 5) % sizeof(SEQ);
  zbuddy_art_sprite(P[SEQ[beat]], 5, 0, 0xF800, 0);

  // Z's drift up-right, smoke puff drifts up
  int p1 = (t)     % 10;
  int p2 = (t + 4) % 10;
  int p3 = (t + 7) % 12;
  zbuddy_art_color(ZBUDDY_ART_DIM);
  zbuddy_art_cursor(ZBUDDY_ART_X_CENTER + 22 + p1, ZBUDDY_ART_Y_OVERLAY + 18 - p1 * 2);
  zbuddy_art_print("z");
  zbuddy_art_color(ZBUDDY_ART_WHITE);
  zbuddy_art_cursor(ZBUDDY_ART_X_CENTER + 28 + p2, ZBUDDY_ART_Y_OVERLAY + 12 - p2);
  zbuddy_art_print("Z");
  // Smoke ring rising
  zbuddy_art_color(ZBUDDY_ART_DIM);
  zbuddy_art_cursor(ZBUDDY_ART_X_CENTER + 18, ZBUDDY_ART_Y_OVERLAY + 16 - p3);
  zbuddy_art_print("o");
}

// ─── IDLE ───  ~14s cycle, 10 poses
static void doIdle(uint32_t t) {
  static const char *const PROUD[5]   = { "            ", "  /^\\  /^\\  ", " <  o    o >", " (   ww   ) ", "  `-vvvv-'  " };
  static const char *const LOOK_L[5]  = { "            ", "  /^\\  /^\\  ", " <o     o  >", " (   ww   ) ", "  `-vvvv-'  " };
  static const char *const LOOK_R[5]  = { "            ", "  /^\\  /^\\  ", " <  o     o>", " (   ww   ) ", "  `-vvvv-'  " };
  static const char *const BLINK[5]   = { "            ", "  /^\\  /^\\  ", " <  -    - >", " (   ww   ) ", "  `-vvvv-'  " };
  static const char *const WING_UP[5] = { "  /^\\  /^\\  ", "  \\_/  \\_/  ", " <  o    o >", " (   ww   ) ", "  `-vvvv-'  " };
  static const char *const WING_DN[5] = { "            ", "  \\v/  \\v/  ", " <  o    o >", " (   ww   ) ", "  `-vvvv-'  " };
  static const char *const SNIFF[5]   = { "      ~     ", "  /^\\  /^\\  ", " <  o    o >", " (   nn   ) ", "  `-vvvv-'  " };
  static const char *const PUFF_R[5]  = { "         ~  ", "  /^\\  /^\\  ", " <  o    o >", " (   ww   )~", "  `-vvvv-'  " };
  static const char *const SMUG[5]    = { "            ", "  /^\\  /^\\  ", " <  ^    ^ >", " (   --   ) ", "  `-vvvv-'  " };
  static const char *const STRETCH[5] = { "  /^\\  /^\\  ", " //^\\  /^\\\\ ", "< <  o   o> >", "  (   ww  )  ", "   `-vvvv-'  " };

  const char *const * P[10] = { PROUD, LOOK_L, LOOK_R, BLINK, WING_UP, WING_DN, SNIFF, PUFF_R, SMUG, STRETCH };
  static const uint8_t SEQ[] = {
    0,0,1,0,2,0,3,
    4,5,4,5,4,5,
    0,0,6,7,
    0,3,0,8,8,0,
    1,2,0,
    9,9,0,0,
    7,0,8,0
  };
  uint8_t beat = (t / 5) % sizeof(SEQ);
  zbuddy_art_sprite(P[SEQ[beat]], 5, 0, 0xF800, 0);

  // Tiny smoke trail from nostrils when sniffing/puffing
  uint8_t pose = SEQ[beat];
  if (pose == 6 || pose == 7) {
    int s = (t & 7);
    zbuddy_art_color(ZBUDDY_ART_DIM);
    zbuddy_art_cursor(ZBUDDY_ART_X_CENTER + (pose == 7 ? 14 : 0), ZBUDDY_ART_Y_OVERLAY - 4 - s);
    zbuddy_art_print(s & 1 ? "o" : ".");
  }
}

// ─── BUSY ───  ~10s cycle, 6 poses + gold-coin ticker
static void doBusy(uint32_t t) {
  static const char *const COUNT_A[5] = { "    $$$$    ", "  /^\\  /^\\  ", " <  v    v >", " (   --   ) ", " /`-vvvv-'\\ " };
  static const char *const COUNT_B[5] = { "    $$$$    ", "  /^\\  /^\\  ", " <  v    v >", " (   __   ) ", " \\`-vvvv-'/ " };
  static const char *const PONDER[5]  = { "      ?     ", "  /^\\  /^\\  ", " <  ^    ^ >", " (   ..   ) ", "  `-vvvv-'  " };
  static const char *const STACK[5]   = { "    [$]     ", "  /^|  /^\\  ", " <  v|   v >", " (   --   ) ", "  `-vvvv-'  " };
  static const char *const EUREKA[5]  = { "      *     ", "  /^\\  /^\\  ", " <  O    O >", " (   ^^   )~", "  `-vvvv-'  " };
  static const char *const PUFF_OUT[5]= { "    ~~~~    ", "  /^\\  /^\\  ", " <  -    - >", " (   __   ) ", "  `-vvvv-'  " };

  const char *const * P[6] = { COUNT_A, COUNT_B, PONDER, STACK, EUREKA, PUFF_OUT };
  static const uint8_t SEQ[] = {
    0,1,0,1,0,1, 2,2, 0,1,0,1, 3,3, 2,4, 0,1,0,1,5
  };
  uint8_t beat = (t / 5) % sizeof(SEQ);
  zbuddy_art_sprite(P[SEQ[beat]], 5, 0, 0xF800, 0);

  // Gold coins clinking — '$' and '.' alternate
  static const char *const COINS[] = { "$  ", "$$ ", "$$$", " $$", "  $", "   " };
  zbuddy_art_color(ZBUDDY_ART_YEL);
  zbuddy_art_cursor(ZBUDDY_ART_X_CENTER + 22, ZBUDDY_ART_Y_OVERLAY + 14);
  zbuddy_art_print(COINS[t % 6]);
  // Sparkle on top of pile
  if ((t / 2) & 1) {
    zbuddy_art_color(ZBUDDY_ART_WHITE);
    zbuddy_art_cursor(ZBUDDY_ART_X_CENTER + 24, ZBUDDY_ART_Y_OVERLAY + 10);
    zbuddy_art_print("*");
  }
}

// ─── ATTENTION ───  ~8s cycle, 6 poses + ! pulse
static void doAttention(uint32_t t) {
  static const char *const ROAR[5]    = { "    ^  ^    ", " /^^\\  /^^\\ ", "<  O    O  >", " (   <>   ) ", "  `-vvvv-'  " };
  static const char *const SCAN_L[5]  = { "    ^  ^    ", " /^^\\  /^^\\ ", "< O      O >", " (   O    ) ", "  `-vvvv-'  " };
  static const char *const SCAN_R[5]  = { "    ^  ^    ", " /^^\\  /^^\\ ", "<  O      O>", " (    O   ) ", "  `-vvvv-'  " };
  static const char *const FLAME[5]   = { "  ~~~  ~~~  ", " /^^\\  /^^\\ ", "<  O    O  >", " (   <>   )~", "  `-vvvv-'  " };
  static const char *const PUFF_UP[5] = { "    ^  ^    ", "/^^^\\  /^^^\\", "<  O    O  >", "((  <>   ))~", " /`-vvvv-'\\ " };
  static const char *const HISS[5]    = { "    ^  ^    ", " /^^\\  /^^\\ ", "<  o    o  >", " (   ss   ) ", "  `-vvvv-'  " };

  const char *const * P[6] = { ROAR, SCAN_L, SCAN_R, FLAME, PUFF_UP, HISS };
  static const uint8_t SEQ[] = {
    0,4,0,1,0,2,0,3, 4,4,0,1,2,3, 5,0
  };
  uint8_t beat = (t / 5) % sizeof(SEQ);
  uint8_t pose = SEQ[beat];
  int xOff = (pose == 4) ? ((t & 1) ? 1 : -1) : 0;
  zbuddy_art_sprite(P[pose], 5, 0, 0xF800, xOff);

  // Pulsing exclamations
  if ((t / 2) & 1) {
    zbuddy_art_color(ZBUDDY_ART_YEL);
    zbuddy_art_cursor(ZBUDDY_ART_X_CENTER - 6, ZBUDDY_ART_Y_OVERLAY);
    zbuddy_art_print("!");
  }
  if ((t / 3) & 1) {
    zbuddy_art_color(ZBUDDY_ART_RED);
    zbuddy_art_cursor(ZBUDDY_ART_X_CENTER + 6, ZBUDDY_ART_Y_OVERLAY + 4);
    zbuddy_art_print("!");
  }
  // Flame puff after a flame frame
  if (pose == 3 || pose == 4) {
    zbuddy_art_color(ZBUDDY_ART_YEL);
    zbuddy_art_cursor(ZBUDDY_ART_X_CENTER + 18, ZBUDDY_ART_Y_OVERLAY + 12 - ((t * 2) % 8));
    zbuddy_art_print("^");
  }
}

// ─── CELEBRATE ───  ~5.6s cycle, 6 poses + multicolor confetti
static void doCelebrate(uint32_t t) {
  static const char *const CROUCH[5]  = { "            ", "  /^\\  /^\\  ", " <  ^    ^ >", " (   WW   ) ", " /`-vvvv-'\\ " };
  static const char *const JUMP[5]    = { "  \\(    )/  ", "   /^\\/^\\   ", " <  ^    ^ >", " (   WW   ) ", "  `-vvvv-'  " };
  static const char *const PEAK[5]    = { "  \\^    ^/  ", "  //^\\/^\\\\  ", " <  *    * >", " (   OO   ) ", "  `-vvvv-'  " };
  static const char *const SPIN_L[5]  = { "            ", "  /^\\  /^\\  ", "< <    <  > ", " (   ww   ) ", "  `-vvvv-'  " };
  static const char *const SPIN_R[5]  = { "            ", "  /^\\  /^\\  ", " <  >    > >", " (   ww   ) ", "  `-vvvv-'  " };
  static const char *const POSE[5]    = { "    \\$$/    ", "  /^\\  /^\\  ", " <  ^    ^ >", "/(   WW   )\\", "  `-vvvv-'  " };

  const char *const * P[6] = { CROUCH, JUMP, PEAK, SPIN_L, SPIN_R, POSE };
  static const uint8_t SEQ[] = { 0,1,2,1,0, 3,4,3,4, 0,1,2,1,0, 5,5 };
  static const int8_t Y_SHIFT[] = { 0,-3,-6,-3,0, 0,0,0,0, 0,-3,-6,-3,0, 0,0 };
  uint8_t beat = (t / 3) % sizeof(SEQ);
  zbuddy_art_sprite(P[SEQ[beat]], 5, Y_SHIFT[beat], 0xF800, 0);

  // Multicolor confetti raining
  static const uint16_t cols[] = { ZBUDDY_ART_YEL, ZBUDDY_ART_HEART, ZBUDDY_ART_CYAN, ZBUDDY_ART_WHITE, ZBUDDY_ART_GREEN };
  for (int i = 0; i < 6; i++) {
    int phase = (t * 2 + i * 11) % 22;
    int x = ZBUDDY_ART_X_CENTER - 36 + i * 14;
    int y = ZBUDDY_ART_Y_OVERLAY - 6 + phase;
    if (y > ZBUDDY_ART_Y_BASE + 20 || y < 0) continue;
    zbuddy_art_color(cols[i % 5]);
    zbuddy_art_cursor(x, y);
    zbuddy_art_print((i + (int)(t / 2)) & 1 ? "$" : "*");
  }
}

// ─── DIZZY ───  ~5.6s cycle, 5 poses + orbiting stars
static void doDizzy(uint32_t t) {
  static const char *const TILT_L[5]  = { "            ", " /^\\  /^\\   ", "< @    @ )  ", " (   ~~   ) ", "  `-vvvv-'  " };
  static const char *const TILT_R[5]  = { "            ", "   /^\\  /^\\ ", "  ( @    @ >", " (   ~~   ) ", "  `-vvvv-'  " };
  static const char *const WOOZY[5]   = { "            ", "  /v\\  /^\\  ", " <  x    @ >", " (   ~v   ) ", "  `-vvvv-'  " };
  static const char *const WOOZY2[5]  = { "            ", "  /^\\  /v\\  ", " <  @    x >", " (   v~   ) ", "  `-vvvv-'  " };
  static const char *const STUMBLE[5] = { "            ", "  /v\\  /v\\  ", " <  @    @ >", " (   --   ) ", " /`_-vv-_'\\ " };

  const char *const * P[5] = { TILT_L, TILT_R, WOOZY, WOOZY2, STUMBLE };
  static const uint8_t SEQ[] = { 0,1,0,1, 2,3, 0,1,0,1, 4,4, 2,3 };
  static const int8_t X_SHIFT[] = { -3,3,-3,3, 0,0, -3,3,-3,3, 0,0, 0,0 };
  uint8_t beat = (t / 4) % sizeof(SEQ);
  zbuddy_art_sprite(P[SEQ[beat]], 5, 0, 0xF800, X_SHIFT[beat]);

  // Orbiting stars
  static const int8_t OX[] = { 0, 5, 7, 5, 0, -5, -7, -5 };
  static const int8_t OY[] = { -5, -3, 0, 3, 5, 3, 0, -3 };
  uint8_t p1 = t % 8;
  uint8_t p2 = (t + 4) % 8;
  zbuddy_art_color(ZBUDDY_ART_CYAN);
  zbuddy_art_cursor(ZBUDDY_ART_X_CENTER + OX[p1] - 2, ZBUDDY_ART_Y_OVERLAY + 6 + OY[p1]);
  zbuddy_art_print("*");
  zbuddy_art_color(ZBUDDY_ART_YEL);
  zbuddy_art_cursor(ZBUDDY_ART_X_CENTER + OX[p2] - 2, ZBUDDY_ART_Y_OVERLAY + 6 + OY[p2]);
  zbuddy_art_print("*");
  // Spinning '$' coin to remind us this is a dragon
  uint8_t p3 = (t + 2) % 8;
  zbuddy_art_color(ZBUDDY_ART_WHITE);
  zbuddy_art_cursor(ZBUDDY_ART_X_CENTER + OX[p3] - 2, ZBUDDY_ART_Y_OVERLAY + 6 + OY[p3]);
  zbuddy_art_print("$");
}

// ─── HEART ───  ~10s cycle, 5 poses + rising heart stream
static void doHeart(uint32_t t) {
  static const char *const DREAMY[5]  = { "            ", "  /^\\  /^\\  ", " <  ^    ^ >", " (   ww   ) ", "  `-vvvv-'  " };
  static const char *const BLUSH[5]   = { "            ", "  /^\\  /^\\  ", " <#^    ^# >", " (   ww   ) ", "  `-vvvv-'  " };
  static const char *const EYES_C[5]  = { "            ", "  /^\\  /^\\  ", " < <3  <3  >", " (   ww   ) ", "  `-vvvv-'  " };
  static const char *const TWIRL[5]   = { "            ", "  /^\\  /^\\  ", " <  @    @ >", " (   ww   ) ", " /`-vvvv-'\\ " };
  static const char *const SIGH[5]    = { "      v     ", "  /^\\  /^\\  ", " <  -    - >", " (   ^^   )~", "  `-vvvv-'  " };

  const char *const * P[5] = { DREAMY, BLUSH, EYES_C, TWIRL, SIGH };
  static const uint8_t SEQ[] = {
    0,0,1,0, 2,2,0, 1,0,4, 0,0,3,3, 0,1,0,2, 1,0
  };
  static const int8_t Y_BOB[] = { 0,-1,0,-1, 0,-1,0, -1,0,0, -1,0,0,0, -1,0,-1,0, -1,0 };
  uint8_t beat = (t / 5) % sizeof(SEQ);
  zbuddy_art_sprite(P[SEQ[beat]], 5, Y_BOB[beat], 0xF800, 0);

  // Rising heart stream
  zbuddy_art_color(ZBUDDY_ART_HEART);
  for (int i = 0; i < 5; i++) {
    int phase = (t + i * 4) % 16;
    int y = ZBUDDY_ART_Y_OVERLAY + 16 - phase;
    if (y < -2 || y > ZBUDDY_ART_Y_BASE) continue;
    int x = ZBUDDY_ART_X_CENTER - 20 + i * 8 + ((phase / 3) & 1) * 2 - 1;
    zbuddy_art_cursor(x, y);
    zbuddy_art_print("v");
  }
  // Lovesick smoke ring drifts up
  int sp = (t * 2) % 18;
  zbuddy_art_color(ZBUDDY_ART_DIM);
  zbuddy_art_cursor(ZBUDDY_ART_X_CENTER + 14, ZBUDDY_ART_Y_OVERLAY + 14 - sp);
  zbuddy_art_print("o");
}


const struct zbuddy_species zbuddy_species_dragon = {
  "dragon",
  0xF800,
  { doSleep, doIdle, doBusy, doAttention,
    doCelebrate, doDizzy, doHeart }
};
