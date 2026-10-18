/*
 * Copyright (c) 2026 Muhammad Waleed Badar
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * The axolotl species. Art and animation sequences from claude-desktop-buddy
 * (Copyright 2026 Anthropic, PBC, MIT licence, see LICENSE.claude-desktop-buddy),
 * converted to C by tools/port_species.py.
 */
#include <zbuddy/art.h>

#include <stdint.h>
#include <string.h>

// Base silhouette: }~ ... ~{ are the feathery gills, ( .--. ) is the smile,
// (_/  \_) are the little feet. Always smiling, always bubbling.

// ─── SLEEP ───  ~12s cycle, 6 poses + bubble stream
static void doSleep(uint32_t t) {
  static const char *const FLOAT[5]   = { "            ", "}~(______)~{", "}~( -  - )~{", "  ( .__. )  ", "  (_/  \\_)  " };
  static const char *const BREATHE[5] = { "            ", "}~(______)~{", "}~( _  _ )~{", "  ( .__. )  ", "  (_/  \\_)  " };
  static const char *const DEEP[5]    = { "            ", "~}(______){~", "~}( _  _ ){~", "  ( ____ )  ", "  ~_/  \\_~  " };
  static const char *const CURL[5]    = { "            ", "            ", "}~(.____.)~{", " }~(- __ -)~{", "  (__//__)  " };
  static const char *const SIDE[5]    = { "            ", "            ", "  }~~~~~_   ", " }~( -- -)= ", "  (__----)  " };
  static const char *const SNORE[5]   = { "            ", "}~(______)~{", "}~( o  o )~{", "  ( oOOo )  ", "  (_/  \\_)  " };

  const char *const * P[6] = { FLOAT, BREATHE, DEEP, CURL, SIDE, SNORE };
  static const uint8_t SEQ[] = {
    0,1,0,1,0,1,2,1,
    0,1,0,1,
    3,3,4,4,3,4,
    3,3,
    1,5,1,1
  };
  uint8_t beat = (t / 5) % sizeof(SEQ);
  zbuddy_art_sprite(P[SEQ[beat]], 5, 0, 0xFB1E, 0);

  // Bubbles drift up
  int p1 = (t)     % 12;
  int p2 = (t + 5) % 12;
  int p3 = (t + 9) % 12;
  zbuddy_art_color(ZBUDDY_ART_CYAN);
  zbuddy_art_cursor(ZBUDDY_ART_X_CENTER + 18 + (p1 % 3), ZBUDDY_ART_Y_OVERLAY + 20 - p1 * 2);
  zbuddy_art_print("o");
  zbuddy_art_color(ZBUDDY_ART_WHITE);
  zbuddy_art_cursor(ZBUDDY_ART_X_CENTER + 26 - (p2 % 4), ZBUDDY_ART_Y_OVERLAY + 18 - p2);
  zbuddy_art_print("O");
  zbuddy_art_color(ZBUDDY_ART_DIM);
  zbuddy_art_cursor(ZBUDDY_ART_X_CENTER + 14 + (p3 % 5), ZBUDDY_ART_Y_OVERLAY + 12 - p3 / 2);
  zbuddy_art_print("z");
}

// ─── IDLE ───  ~14s cycle, 10 poses + lazy bubbles
static void doIdle(uint32_t t) {
  static const char *const REST[5]    = { "            ", "}~(______)~{", "}~( o  o )~{", "  ( .--. )  ", "  (_/  \\_)  " };
  static const char *const LOOK_L[5]  = { "            ", "}~(______)~{", "}~(o   o )~{", "  ( .--. )  ", "  (_/  \\_)  " };
  static const char *const LOOK_R[5]  = { "            ", "}~(______)~{", "}~( o   o)~{", "  ( .--. )  ", "  (_/  \\_)  " };
  static const char *const LOOK_U[5]  = { "            ", "}~(______)~{", "}~( ^  ^ )~{", "  ( .--. )  ", "  (_/  \\_)  " };
  static const char *const BLINK[5]   = { "            ", "}~(______)~{", "}~( -  - )~{", "  ( .--. )  ", "  (_/  \\_)  " };
  static const char *const GILL_L[5]  = { "            ", "}}~(_____)~{", "}}~(o  o )~{", "  ( .--. )  ", "  (_/  \\_)  " };
  static const char *const GILL_R[5]  = { "            ", "}~(_____)~{{", "}~(o  o )~{{", "  ( .--. )  ", "  (_/  \\_)  " };
  static const char *const CHEW_A[5]  = { "            ", "}~(______)~{", "}~( o  o )~{", "  ( wwww )  ", "  (_/  \\_)  " };
  static const char *const CHEW_B[5]  = { "            ", "}~(______)~{", "}~( o  o )~{", "  ( WWWW )  ", "  (_/  \\_)  " };
  static const char *const WIGGLE[5]  = { "            ", "~}(______){~", "~}( o  o ){~", "  ( .--. )  ", "  ~_/  \\_~  " };

  const char *const * P[10] = { REST, LOOK_L, LOOK_R, LOOK_U, BLINK, GILL_L, GILL_R, CHEW_A, CHEW_B, WIGGLE };
  static const uint8_t SEQ[] = {
    0,0,0,1,0,2,0,4,
    0,5,0,6,0,
    7,8,7,8,
    0,0,3,3,0,4,
    9,9,0,0,
    1,2,1,2,0
  };
  uint8_t beat = (t / 5) % sizeof(SEQ);
  zbuddy_art_sprite(P[SEQ[beat]], 5, 0, 0xFB1E, 0);

  // single lazy bubble
  int p = (t / 2) % 14;
  zbuddy_art_color(ZBUDDY_ART_CYAN);
  zbuddy_art_cursor(ZBUDDY_ART_X_CENTER + 24, ZBUDDY_ART_Y_OVERLAY + 16 - p);
  zbuddy_art_print(p & 1 ? "o" : "O");
}

// ─── BUSY ───  ~10s cycle, 6 poses + dot ticker
static void doBusy(uint32_t t) {
  static const char *const TYPE_A[5]  = { "            ", "}~(______)~{", "}~( v  v )~{", "  (  --  )  ", " /(_/  \\_)\\ " };
  static const char *const TYPE_B[5]  = { "            ", "}~(______)~{", "}~( v  v )~{", "  (  __  )  ", " \\(_/  \\_)/ " };
  static const char *const THINK[5]   = { "      ?     ", "}~(______)~{", "}~( ^  ^ )~{", "  (  ..  )  ", "  (_/  \\_)  " };
  static const char *const SCRIBBLE[5]= { "      /     ", "}~(_____)~{ ", "}~( o  o )~{", "  ( .--. ) /", "  (_/  \\_)  " };
  static const char *const EUREKA[5]  = { "      *     ", "}~(______)~{", "}~( O  O )~{", "  (  ^^  )  ", " /(_/  \\_)\\ " };
  static const char *const RELIEF[5]  = { "    ~~~     ", "}~(______)~{", "}~( -  - )~{", "  (  __  )  ", "  (_/  \\_)  " };

  const char *const * P[6] = { TYPE_A, TYPE_B, THINK, SCRIBBLE, EUREKA, RELIEF };
  static const uint8_t SEQ[] = {
    0,1,0,1,0,1, 2,2, 0,1,0,1, 3,3, 2,4, 0,1,0,1,5
  };
  uint8_t beat = (t / 5) % sizeof(SEQ);
  zbuddy_art_sprite(P[SEQ[beat]], 5, 0, 0xFB1E, 0);

  static const char *const DOTS[] = { ".  ", ".. ", "...", " ..", "  .", "   " };
  zbuddy_art_color(ZBUDDY_ART_WHITE);
  zbuddy_art_cursor(ZBUDDY_ART_X_CENTER + 22, ZBUDDY_ART_Y_OVERLAY + 14);
  zbuddy_art_print(DOTS[t % 6]);

  // tiny bubbles
  int b = (t * 2) % 10;
  zbuddy_art_color(ZBUDDY_ART_CYAN);
  zbuddy_art_cursor(ZBUDDY_ART_X_CENTER - 28, ZBUDDY_ART_Y_OVERLAY + 18 - b);
  zbuddy_art_print("o");
}

// ─── ATTENTION ───  ~8s cycle, 6 poses + ! pulse
static void doAttention(uint32_t t) {
  static const char *const ALERT[5]   = { "    ^  ^    ", "}}~(______)~{{", "}}~( O  O )~{{", "  (  O   )  ", "  (_/  \\_)  " };
  static const char *const SCAN_L[5]  = { "    ^  ^    ", "}}~(______)~{{", "}}~(O    O)~{{", "  (  O   )  ", "  (_/  \\_)  " };
  static const char *const SCAN_R[5]  = { "    ^  ^    ", "}}~(______)~{{", "}}~(O    O)~{{", "  (   O  )  ", "  (_/  \\_)  " };
  static const char *const SCAN_U[5]  = { "    ^  ^    ", "}}~(______)~{{", "}}~( ^  ^ )~{{", "  (  O   )  ", "  (_/  \\_)  " };
  static const char *const TENSE[5]   = { "    ^  ^    ", "}}}~(____)~{{{", "}}}~( O  O)~{{{", "  (  O   )  ", " /(_/  \\_)\\ " };
  static const char *const HUSH[5]    = { "    ^  ^    ", "}~(______)~{", "}~( o  o )~{", "  (  .   )  ", "  (_/  \\_)  " };

  const char *const * P[6] = { ALERT, SCAN_L, SCAN_R, SCAN_U, TENSE, HUSH };
  static const uint8_t SEQ[] = {
    0,4,0,1,0,2,0,3, 4,4,0,1,2,0, 5,0
  };
  uint8_t beat = (t / 5) % sizeof(SEQ);
  uint8_t pose = SEQ[beat];
  int xOff = (pose == 4) ? ((t & 1) ? 1 : -1) : 0;
  zbuddy_art_sprite(P[pose], 5, 0, 0xFB1E, xOff);

  if ((t / 2) & 1) {
    zbuddy_art_color(ZBUDDY_ART_YEL);
    zbuddy_art_cursor(ZBUDDY_ART_X_CENTER - 4, ZBUDDY_ART_Y_OVERLAY);
    zbuddy_art_print("!");
  }
  if ((t / 3) & 1) {
    zbuddy_art_color(ZBUDDY_ART_RED);
    zbuddy_art_cursor(ZBUDDY_ART_X_CENTER + 4, ZBUDDY_ART_Y_OVERLAY + 4);
    zbuddy_art_print("!");
  }
}

// ─── CELEBRATE ───  ~5.6s cycle, 6 poses + confetti rain
static void doCelebrate(uint32_t t) {
  static const char *const CROUCH[5]  = { "            ", "}~(______)~{", "}~( ^  ^ )~{", "  (  ww  )  ", " /(_/  \\_)\\ " };
  static const char *const JUMP[5]    = { "  \\(    )/  ", "}~(______)~{", "}~( ^  ^ )~{", "  (  ww  )  ", "  (_/  \\_)  " };
  static const char *const PEAK[5]    = { "  \\^    ^/  ", "}~(______)~{", "}~( ^  ^ )~{", "  (  WW  )  ", "  (_/  \\_)  " };
  static const char *const SPIN_L[5]  = { "            ", "}~(______)~{", "}~(<    <)~{", "  (  ww  ) /", "  (_/  \\_)  " };
  static const char *const SPIN_R[5]  = { "            ", "}~(______)~{", "}~(>    >)~{", "\\ (  ww  )  ", "  (_/  \\_)  " };
  static const char *const POSE[5]    = { "    \\__/    ", "}~(______)~{", "}~( ^  ^ )~{", "/ (  WW  ) \\", "  (_/  \\_)  " };

  const char *const * P[6] = { CROUCH, JUMP, PEAK, SPIN_L, SPIN_R, POSE };
  static const uint8_t SEQ[] = { 0,1,2,1,0, 3,4,3,4, 0,1,2,1,0, 5,5 };
  static const int8_t Y_SHIFT[] = { 0,-3,-6,-3,0, 0,0,0,0, 0,-3,-6,-3,0, 0,0 };
  uint8_t beat = (t / 3) % sizeof(SEQ);
  zbuddy_art_sprite(P[SEQ[beat]], 5, Y_SHIFT[beat], 0xFB1E, 0);

  static const uint16_t cols[] = { ZBUDDY_ART_YEL, ZBUDDY_ART_HEART, ZBUDDY_ART_CYAN, ZBUDDY_ART_WHITE, ZBUDDY_ART_GREEN, ZBUDDY_ART_PURPLE };
  for (int i = 0; i < 7; i++) {
    int phase = (t * 2 + i * 9) % 24;
    int x = ZBUDDY_ART_X_CENTER - 36 + i * 12;
    int y = ZBUDDY_ART_Y_OVERLAY - 6 + phase;
    if (y > ZBUDDY_ART_Y_BASE + 20 || y < 0) continue;
    zbuddy_art_color(cols[i % 6]);
    zbuddy_art_cursor(x, y);
    zbuddy_art_print((i + (int)(t/2)) & 1 ? "*" : "o");
  }
}

// ─── DIZZY ───  ~6s cycle, 5 poses + orbiting stars
static void doDizzy(uint32_t t) {
  static const char *const TILT_L[5]  = { "            ", "}~(______)~{ ", "}~( @  @ )~{ ", "  ( ~~~~ )  ", "  (_/  \\_)  " };
  static const char *const TILT_R[5]  = { "            ", " }~(______)~{", " }~( @  @ )~{", "  ( ~~~~ )  ", "  (_/  \\_)  " };
  static const char *const WOOZY[5]   = { "            ", "}~(______)~{", "}~( x  @ )~{", "  ( ~vv~ )  ", "  (_/  \\_)  " };
  static const char *const WOOZY2[5]  = { "            ", "}~(______)~{", "}~( @  x )~{", "  ( vv~~ )  ", "  (_/  \\_)  " };
  static const char *const STUMBLE[5] = { "            ", "~}(______){~", "~}( @  @ ){~", "  (  --  )  ", " /~_/  \\_~\\ " };

  const char *const * P[5] = { TILT_L, TILT_R, WOOZY, WOOZY2, STUMBLE };
  static const uint8_t SEQ[] = { 0,1,0,1, 2,3, 0,1,0,1, 4,4, 2,3 };
  static const int8_t X_SHIFT[] = { -3,3,-3,3, 0,0, -3,3,-3,3, 0,0, 0,0 };
  uint8_t beat = (t / 4) % sizeof(SEQ);
  zbuddy_art_sprite(P[SEQ[beat]], 5, 0, 0xFB1E, X_SHIFT[beat]);

  static const int8_t OX[] = { 0, 5, 7, 5, 0, -5, -7, -5 };
  static const int8_t OY[] = { -5, -3, 0, 3, 5, 3, 0, -3 };
  uint8_t p1 = t % 8;
  uint8_t p2 = (t + 4) % 8;
  uint8_t p3 = (t + 2) % 8;
  zbuddy_art_color(ZBUDDY_ART_CYAN);
  zbuddy_art_cursor(ZBUDDY_ART_X_CENTER + OX[p1] - 2, ZBUDDY_ART_Y_OVERLAY + 6 + OY[p1]);
  zbuddy_art_print("*");
  zbuddy_art_color(ZBUDDY_ART_YEL);
  zbuddy_art_cursor(ZBUDDY_ART_X_CENTER + OX[p2] - 2, ZBUDDY_ART_Y_OVERLAY + 6 + OY[p2]);
  zbuddy_art_print("*");
  zbuddy_art_color(ZBUDDY_ART_HEART);
  zbuddy_art_cursor(ZBUDDY_ART_X_CENTER + OX[p3] - 2, ZBUDDY_ART_Y_OVERLAY + 6 + OY[p3]);
  zbuddy_art_print("o");
}

// ─── HEART ───  ~10s cycle, 6 poses + rising heart stream
static void doHeart(uint32_t t) {
  static const char *const DREAMY[5]  = { "            ", "}~(______)~{", "}~( ^  ^ )~{", "  ( .vv. )  ", "  (_/  \\_)  " };
  static const char *const BLUSH[5]   = { "            ", "}~(______)~{", "}~(#^  ^#)~{", "  ( .vv. )  ", "  (_/  \\_)  " };
  static const char *const EYES_C[5]  = { "            ", "}~(______)~{", "}~(<3  <3)~{", "  ( .vv. )  ", "  (_/  \\_)  " };
  static const char *const TWIRL[5]   = { "            ", "~}(______){~", "~}( @  @ ){~", "  ( .vv. )  ", " /(_/  \\_)\\ " };
  static const char *const SIGH[5]    = { "            ", "}~(______)~{", "}~( -  - )~{", "  ( ^^^^ )  ", "  (_/  \\_)  " };
  static const char *const WINK[5]    = { "            ", "}~(______)~{", "}~( ^  - )~{", "  ( .vv. )  ", "  (_/  \\_)  " };

  const char *const * P[6] = { DREAMY, BLUSH, EYES_C, TWIRL, SIGH, WINK };
  static const uint8_t SEQ[] = {
    0,0,1,0, 2,2,0, 1,0,4, 0,0,3,3, 0,1,0,2, 5,0
  };
  static const int8_t Y_BOB[] = { 0,-1,0,-1, 0,-1,0, -1,0,0, -1,0,0,0, -1,0,-1,0, -1,0 };
  uint8_t beat = (t / 5) % sizeof(SEQ);
  zbuddy_art_sprite(P[SEQ[beat]], 5, Y_BOB[beat], 0xFB1E, 0);

  zbuddy_art_color(ZBUDDY_ART_HEART);
  for (int i = 0; i < 5; i++) {
    int phase = (t + i * 4) % 16;
    int y = ZBUDDY_ART_Y_OVERLAY + 16 - phase;
    if (y < -2 || y > ZBUDDY_ART_Y_BASE) continue;
    int x = ZBUDDY_ART_X_CENTER - 20 + i * 8 + ((phase / 3) & 1) * 2 - 1;
    zbuddy_art_cursor(x, y);
    zbuddy_art_print("v");
  }

  // a couple of pink bubbles too
  int b = (t + 3) % 14;
  zbuddy_art_color(ZBUDDY_ART_HEART);
  zbuddy_art_cursor(ZBUDDY_ART_X_CENTER + 26, ZBUDDY_ART_Y_OVERLAY + 16 - b);
  zbuddy_art_print(b & 1 ? "o" : "O");
}


const struct zbuddy_species zbuddy_species_axolotl = {
  "axolotl",
  0xFB1E,
  { doSleep, doIdle, doBusy, doAttention,
    doCelebrate, doDizzy, doHeart }
};
