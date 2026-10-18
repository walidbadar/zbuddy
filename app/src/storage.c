/*
 * Copyright (c) 2026 Muhammad Waleed Badar
 * SPDX-License-Identifier: GPL-3.0-only
 */
#include "storage.h"
#include "character.h"

#include <stdio.h>
#include <string.h>
#include <zephyr/fs/fs.h>
#include <zephyr/fs/littlefs.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/storage/flash_map.h>
#if defined(CONFIG_ESP_SPIRAM)
#include <zephyr/multi_heap/shared_multi_heap.h>
#endif

LOG_MODULE_REGISTER(storage, LOG_LEVEL_INF);

#define MNT CONFIG_ZBUDDY_APP_MOUNT_POINT
#define CHARS MNT "/characters"

FS_LITTLEFS_DECLARE_DEFAULT_CONFIG(lfs_cfg);

static struct fs_mount_t mnt = {
	.type = FS_LITTLEFS,
	.fs_data = &lfs_cfg,
	.storage_dev = (void *)PARTITION_ID(characters_partition),
	.mnt_point = MNT,
};

static bool mounted;
static struct fs_file_t cur_file;
static bool cur_open;
static char cur_dir[80];

void *storage_alloc(size_t n)
{
#if defined(CONFIG_ESP_SPIRAM)
	return shared_multi_heap_alloc(SMH_REG_ATTR_EXTERNAL, n ? n : 1);
#else
	return k_malloc(n ? n : 1);
#endif
}

void storage_free(void *buf)
{
#if defined(CONFIG_ESP_SPIRAM)
	shared_multi_heap_free(buf);
#else
	k_free(buf);
#endif
}

int storage_init(void)
{
	int rc = fs_mount(&mnt);

	if (rc != 0) {
		LOG_ERR("mount %s failed (%d)", MNT, rc);
		return rc;
	}
	mounted = true;
	{
		struct fs_dirent ent;

		if (fs_stat(CHARS, &ent) != 0) {
			fs_mkdir(CHARS);
		}
	}
	LOG_INF("%s mounted", MNT);
	return 0;
}

bool storage_ready(void)
{
	return mounted;
}

void storage_space(uint32_t *free_bytes, uint32_t *total_bytes)
{
	struct fs_statvfs st;

	*free_bytes = 0;
	*total_bytes = 0;
	if (mounted && fs_statvfs(MNT, &st) == 0) {
		*free_bytes = (uint32_t)(st.f_bfree * st.f_frsize);
		*total_bytes = (uint32_t)(st.f_blocks * st.f_frsize);
	}
}

/* Sum of the file sizes in one pack directory, deleting them when @p wipe */
static uint32_t walk_pack(const char *dir, bool wipe)
{
	struct fs_dir_t d;
	struct fs_dirent ent;
	uint32_t bytes = 0;

	fs_dir_t_init(&d);
	if (fs_opendir(&d, dir) != 0) {
		return 0;
	}
	while (fs_readdir(&d, &ent) == 0 && ent.name[0] != '\0') {
		char path[384];

		snprintf(path, sizeof(path), "%s/%s", dir, ent.name);
		if (ent.type == FS_DIR_ENTRY_FILE) {
			bytes += (uint32_t)ent.size;
			if (wipe) {
				fs_unlink(path);
			}
		} else if (wipe) {
			walk_pack(path, true);
			fs_unlink(path);
		}
	}
	fs_closedir(&d);
	return bytes;
}

static uint32_t walk_characters(bool wipe)
{
	struct fs_dir_t d;
	struct fs_dirent ent;
	uint32_t bytes = 0;

	fs_dir_t_init(&d);
	if (fs_opendir(&d, CHARS) != 0) {
		return 0;
	}
	while (fs_readdir(&d, &ent) == 0 && ent.name[0] != '\0') {
		char path[384];

		snprintf(path, sizeof(path), "%s/%s", CHARS, ent.name);
		if (ent.type == FS_DIR_ENTRY_DIR) {
			bytes += walk_pack(path, wipe);
			if (wipe) {
				fs_unlink(path);
			}
		} else {
			bytes += (uint32_t)ent.size;
			if (wipe) {
				fs_unlink(path);
			}
		}
	}
	fs_closedir(&d);
	return bytes;
}

void storage_wipe_characters(void)
{
	if (mounted) {
		walk_characters(true);
	}
}

bool storage_first_character(char *name, size_t len)
{
	struct fs_dir_t d;
	struct fs_dirent ent;
	bool found = false;

	if (!mounted) {
		return false;
	}
	fs_dir_t_init(&d);
	if (fs_opendir(&d, CHARS) != 0) {
		return false;
	}
	while (fs_readdir(&d, &ent) == 0 && ent.name[0] != '\0') {
		if (ent.type == FS_DIR_ENTRY_DIR) {
			strncpy(name, ent.name, len - 1);
			name[len - 1] = '\0';
			found = true;
			break;
		}
	}
	fs_closedir(&d);
	return found;
}

int storage_read_file(const char *character, const char *file, void **buf, size_t *len)
{
	char path[128];
	struct fs_dirent st;
	struct fs_file_t f;
	void *data;
	ssize_t n;
	int rc;

	snprintf(path, sizeof(path), "%s/%s/%s", CHARS, character, file);
	rc = fs_stat(path, &st);
	if (rc != 0) {
		return rc;
	}
	data = storage_alloc(st.size + 1);
	if (data == NULL) {
		return -ENOMEM;
	}
	fs_file_t_init(&f);
	rc = fs_open(&f, path, FS_O_READ);
	if (rc != 0) {
		storage_free(data);
		return rc;
	}
	n = fs_read(&f, data, st.size);
	fs_close(&f);
	if (n < 0 || (size_t)n != st.size) {
		storage_free(data);
		return -EIO;
	}
	((char *)data)[st.size] = '\0';
	*buf = data;
	*len = st.size;
	return 0;
}

/* --- the protocol's store ------------------------------------------------- */

static uint32_t st_reclaimable(void)
{
	return mounted ? walk_characters(false) : 0;
}

static int st_begin(const char *name, uint32_t total)
{
	int rc;

	ARG_UNUSED(total);
	if (!mounted) {
		return -ENODEV;
	}
	character_close();
	walk_characters(true);
	snprintf(cur_dir, sizeof(cur_dir), "%s/%s", CHARS, name);
	rc = fs_mkdir(cur_dir);
	if (rc != 0 && rc != -EEXIST) {
		LOG_ERR("mkdir %s failed (%d)", cur_dir, rc);
		return rc;
	}
	LOG_INF("installing '%s'", name);
	return 0;
}

static int st_open(const char *path, uint32_t size)
{
	char full[128];
	int rc;

	ARG_UNUSED(size);
	if (cur_open) {
		fs_close(&cur_file);
		cur_open = false;
	}
	snprintf(full, sizeof(full), "%s/%s", cur_dir, path);
	fs_file_t_init(&cur_file);
	rc = fs_open(&cur_file, full, FS_O_CREATE | FS_O_WRITE | FS_O_TRUNC);
	if (rc != 0) {
		LOG_ERR("open %s failed (%d)", full, rc);
		return rc;
	}
	cur_open = true;
	return 0;
}

static int st_write(const void *data, size_t len)
{
	ssize_t n;

	if (!cur_open) {
		return -EBADF;
	}
	n = fs_write(&cur_file, data, len);
	return (n == (ssize_t)len) ? 0 : -EIO;
}

static int st_close(void)
{
	if (cur_open) {
		fs_close(&cur_file);
		cur_open = false;
	}
	return 0;
}

static int st_end(const char *name)
{
	st_close();
	return character_init(name) ? 0 : -EINVAL;
}

static const struct zbuddy_store store = {
	.space = storage_space,
	.reclaimable = st_reclaimable,
	.begin = st_begin,
	.open = st_open,
	.write = st_write,
	.close = st_close,
	.end = st_end,
};

const struct zbuddy_store *storage_store(void)
{
	return &store;
}
