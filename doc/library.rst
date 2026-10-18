..
   Copyright (c) 2026 Muhammad Waleed Badar
   SPDX-License-Identifier: GPL-3.0-only

The library
###########

``lib/zbuddy`` is the protocol, the stats, the character manifests and the
ASCII species as portable C, with a Zephyr module wrapper. Enable
``CONFIG_ZBUDDY=y`` with the repository in the module list.

.. code-block:: c

   #include <zbuddy/proto.h>
   #include <zbuddy/stats.h>

   static const struct zbuddy_proto_ops ops = {
       .send = my_send,          /* a line to the desktop */
       .set_name = ..., .get_name = ..., .set_owner = ..., .get_owner = ...,
       .set_species = ..., .unpair = ..., .time_sync = ..., .sys_info = ...,
       .secure = ..., .store = &my_store, /* folder push, may be NULL */
   };
   static struct zbuddy_proto proto;

   zbuddy_stats_init(loaded_record_or_NULL, save_callback, now_ms);
   zbuddy_proto_init(&proto, &ops);
   /* bytes from the transport, in any size */
   zbuddy_proto_feed(&proto, data, len, now_ms);
   /* every loop: timeouts, demo scenarios */
   zbuddy_proto_poll(&proto, now_ms);
   enum zbuddy_persona mood = zbuddy_derive(&proto.snap);
   /* the user decided */
   zbuddy_proto_permission(&proto, proto.snap.prompt.id, true);

``struct zbuddy_snapshot`` is the desktop's state: session counters, the
message, the transcript lines (with a generation counter that bumps when
they change) and the pending prompt.

Stats
*****

``zbuddy_stats_*`` keep the persisted record (``struct zbuddy_stats_data``)
and derive the mood tier, the fed progress and the energy from it. The
save callback is called on the events that change the record.

Species
*******

The ASCII species draw through three callbacks the application provides:

.. code-block:: c

   #include <zbuddy/art.h>

   static const struct zbuddy_art_ops canvas = {
       .set_color = ..., /* RGB565 */
       .set_cursor = ..., /* pixels on a 160x88 canvas */
       .print = ...,     /* 8x8 glyphs, advancing the cursor */
   };

   zbuddy_art_bind(&canvas);
   zbuddy_art_render(zbuddy_species_get(idx), ZBUDDY_IDLE, tick); /* 5 ticks/s */

``tools/port_species.py`` converts the species files of
claude-desktop-buddy to this form; the art is unchanged.

Character packs
***************

``zbuddy_character_parse()`` reads a ``manifest.json`` into
``struct zbuddy_character``: the palette, the GIF file of each state (one
or a rotation) or the text frames.

Kconfig
*******

=========================================  ===================================
Option                                     Purpose
=========================================  ===================================
``ZBUDDY_LINE_MAX``                        longest JSON line accepted (4608)
``ZBUDDY_LINES``, ``ZBUDDY_LINE_LEN``      transcript lines kept (8 x 96)
``ZBUDDY_CHUNK_MAX``                       largest decoded push chunk (1024)
``ZBUDDY_CHARACTER_MAX_BYTES``             largest pack accepted (1.8 MB)
``ZBUDDY_SPECIES``                         the ASCII species
``ZBUDDY_TOKENS_PER_LEVEL``                50000
=========================================  ===================================

The JSON is scanned by a small tokenizer of its own (``zbuddy_json.c``):
the protocol uses the same key with different value types (a state is a
file name or a list of them, ``name`` is a pet or a pack), which a
descriptor based parser cannot express.
