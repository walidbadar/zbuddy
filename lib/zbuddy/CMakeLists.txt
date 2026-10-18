# Copyright (c) 2026 Muhammad Waleed Badar
# SPDX-License-Identifier: GPL-3.0-only

zephyr_library()

zephyr_include_directories(${ZEPHYR_ZBUDDY_MODULE_DIR}/include)

zephyr_library_sources(
  zbuddy_json.c
  zbuddy_proto.c
  zbuddy_stats.c
  zbuddy_character.c
)

zephyr_library_sources_ifdef(CONFIG_ZBUDDY_SPECIES
  zbuddy_art.c
  species/axolotl.c
  species/blob.c
  species/cactus.c
  species/capybara.c
  species/cat.c
  species/chonk.c
  species/clawd.c
  species/dragon.c
  species/duck.c
  species/ghost.c
  species/goose.c
  species/mushroom.c
  species/octopus.c
  species/owl.c
  species/penguin.c
  species/rabbit.c
  species/robot.c
  species/snail.c
  species/turtle.c
)
