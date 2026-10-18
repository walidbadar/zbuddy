/*
 * Copyright (c) 2026 Muhammad Waleed Badar
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * A small JSON scanner: walks the members of an object or the elements of
 * an array as raw tokens, so a handler can route each value by key and by
 * type. The protocol and the manifests use the same key with different
 * value types (a state is a file name or a list of them, "name" is a pet
 * name or a pack name), which a descriptor-based parser cannot express.
 */
#ifndef ZBUDDY_JSON_H
#define ZBUDDY_JSON_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum zbuddy_jtype {
	ZBUDDY_J_NONE,
	ZBUDDY_J_STRING,
	ZBUDDY_J_NUMBER,
	ZBUDDY_J_TRUE,
	ZBUDDY_J_FALSE,
	ZBUDDY_J_NULL,
	ZBUDDY_J_OBJECT,
	ZBUDDY_J_ARRAY,
};

/** A value: for a string, start/end delimit the escaped text without the
 * quotes; for an object or array they span the balanced braces; for the
 * others the literal text.
 */
struct zbuddy_jtok {
	enum zbuddy_jtype type;
	const char *start;
	const char *end;
};

struct zbuddy_jkv {
	struct zbuddy_jtok key; /* always a string */
	struct zbuddy_jtok value;
};

/** Called for each member; return false to stop early. */
typedef bool (*zbuddy_json_member_fn)(const struct zbuddy_jkv *kv, void *ctx);
/** Called for each element; return false to stop early. */
typedef bool (*zbuddy_json_elem_fn)(const struct zbuddy_jtok *elem, size_t idx, void *ctx);

/** Walk the members of the object in [s, s+len). Returns the number of
 * members visited, or -EINVAL when the text is not a well formed object.
 */
int zbuddy_json_obj_each(const char *s, size_t len, zbuddy_json_member_fn fn, void *ctx);

/** Walk the elements of the array in [s, s+len). */
int zbuddy_json_arr_each(const char *s, size_t len, zbuddy_json_elem_fn fn, void *ctx);

/** Convenience forms for a token that is an object/array. */
int zbuddy_json_obj_each_tok(const struct zbuddy_jtok *obj, zbuddy_json_member_fn fn, void *ctx);
int zbuddy_json_arr_each_tok(const struct zbuddy_jtok *arr, zbuddy_json_elem_fn fn, void *ctx);

bool zbuddy_json_key_is(const struct zbuddy_jkv *kv, const char *name);
/** Compare a string token with a C string (after unescaping). */
bool zbuddy_json_str_is(const struct zbuddy_jtok *v, const char *s);

/** Copy a string token out, unescaped and NUL terminated, truncating to
 * @p dst_len. Returns the full unescaped length.
 */
size_t zbuddy_json_str(const struct zbuddy_jtok *v, char *dst, size_t dst_len);
int64_t zbuddy_json_int(const struct zbuddy_jtok *v, int64_t def);
bool zbuddy_json_bool(const struct zbuddy_jtok *v, bool def);

/** Escape a C string into @p dst for embedding in a JSON string. */
size_t zbuddy_json_escape(const char *s, char *dst, size_t dst_len);

#endif /* ZBUDDY_JSON_H */
