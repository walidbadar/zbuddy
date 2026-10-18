/*
 * Copyright (c) 2026 Muhammad Waleed Badar
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * Character packs on a LittleFS partition: <mount>/characters/<name>/...
 */
#ifndef ZBUDDY_APP_STORAGE_H
#define ZBUDDY_APP_STORAGE_H

#include <zbuddy/proto.h>

int storage_init(void);
bool storage_ready(void);
/** The folder-push receiver for the protocol. */
const struct zbuddy_store *storage_store(void);
/** Name of the first installed pack, or false. */
bool storage_first_character(char *name, size_t len);
/** Read a file of the installed pack into a new buffer (storage_free). */
int storage_read_file(const char *character, const char *file, void **buf, size_t *len);
void storage_free(void *buf);
/** Delete every installed pack. */
void storage_wipe_characters(void);
void storage_space(uint32_t *free_bytes, uint32_t *total_bytes);

/** Big allocations: PSRAM on the ESP32-S3, the kernel heap elsewhere. */
void *storage_alloc(size_t n);

#endif
