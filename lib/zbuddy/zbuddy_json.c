/*
 * Copyright (c) 2026 Muhammad Waleed Badar
 * SPDX-License-Identifier: GPL-3.0-only
 */
#include "zbuddy_json.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

static const char *skip_ws(const char *p, const char *end)
{
	while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) {
		p++;
	}
	return p;
}

/* Scan a string starting at the opening quote; the token excludes the quotes. */
static const char *scan_string(const char *p, const char *end, struct zbuddy_jtok *t)
{
	if (p >= end || *p != '"') {
		return NULL;
	}
	p++;
	t->type = ZBUDDY_J_STRING;
	t->start = p;
	while (p < end) {
		if (*p == '\\') {
			p += 2;
			continue;
		}
		if (*p == '"') {
			t->end = p;
			return p + 1;
		}
		p++;
	}
	return NULL;
}

static const char *scan_value(const char *p, const char *end, struct zbuddy_jtok *t);

static const char *scan_container(const char *p, const char *end, struct zbuddy_jtok *t)
{
	char open = *p, close = (open == '{') ? '}' : ']';
	int depth = 0;

	t->type = (open == '{') ? ZBUDDY_J_OBJECT : ZBUDDY_J_ARRAY;
	t->start = p;
	while (p < end) {
		char c = *p;

		if (c == '"') {
			struct zbuddy_jtok s;

			p = scan_string(p, end, &s);
			if (p == NULL) {
				return NULL;
			}
			continue;
		}
		if (c == '{' || c == '[') {
			depth++;
		} else if (c == '}' || c == ']') {
			depth--;
			if (depth == 0) {
				if (c != close) {
					return NULL;
				}
				t->end = p + 1;
				return p + 1;
			}
		}
		p++;
	}
	return NULL;
}

static const char *scan_literal(const char *p, const char *end, struct zbuddy_jtok *t)
{
	static const struct {
		const char *text;
		enum zbuddy_jtype type;
	} lits[] = {
		{"true", ZBUDDY_J_TRUE},
		{"false", ZBUDDY_J_FALSE},
		{"null", ZBUDDY_J_NULL},
	};

	for (size_t i = 0; i < 3; i++) {
		size_t n = strlen(lits[i].text);

		if ((size_t)(end - p) >= n && memcmp(p, lits[i].text, n) == 0) {
			t->type = lits[i].type;
			t->start = p;
			t->end = p + n;
			return p + n;
		}
	}
	return NULL;
}

static const char *scan_number(const char *p, const char *end, struct zbuddy_jtok *t)
{
	const char *q = p;

	if (q < end && (*q == '-' || *q == '+')) {
		q++;
	}
	if (q >= end || !(*q >= '0' && *q <= '9')) {
		return NULL;
	}
	while (q < end && ((*q >= '0' && *q <= '9') || *q == '.' || *q == 'e' || *q == 'E' ||
			   *q == '-' || *q == '+')) {
		q++;
	}
	t->type = ZBUDDY_J_NUMBER;
	t->start = p;
	t->end = q;
	return q;
}

static const char *scan_value(const char *p, const char *end, struct zbuddy_jtok *t)
{
	p = skip_ws(p, end);
	if (p >= end) {
		return NULL;
	}
	switch (*p) {
	case '"':
		return scan_string(p, end, t);
	case '{':
	case '[':
		return scan_container(p, end, t);
	case 't':
	case 'f':
	case 'n':
		return scan_literal(p, end, t);
	default:
		return scan_number(p, end, t);
	}
}

int zbuddy_json_obj_each(const char *s, size_t len, zbuddy_json_member_fn fn, void *ctx)
{
	const char *end = s + len;
	const char *p = skip_ws(s, end);
	int count = 0;

	if (p >= end || *p != '{') {
		return -EINVAL;
	}
	p = skip_ws(p + 1, end);
	if (p < end && *p == '}') {
		return 0;
	}
	while (p < end) {
		struct zbuddy_jkv kv;

		p = skip_ws(p, end);
		p = scan_string(p, end, &kv.key);
		if (p == NULL) {
			return -EINVAL;
		}
		p = skip_ws(p, end);
		if (p >= end || *p != ':') {
			return -EINVAL;
		}
		p = scan_value(p + 1, end, &kv.value);
		if (p == NULL) {
			return -EINVAL;
		}
		count++;
		if (fn != NULL && !fn(&kv, ctx)) {
			return count;
		}
		p = skip_ws(p, end);
		if (p >= end) {
			return -EINVAL;
		}
		if (*p == '}') {
			return count;
		}
		if (*p != ',') {
			return -EINVAL;
		}
		p++;
	}
	return -EINVAL;
}

int zbuddy_json_arr_each(const char *s, size_t len, zbuddy_json_elem_fn fn, void *ctx)
{
	const char *end = s + len;
	const char *p = skip_ws(s, end);
	int count = 0;

	if (p >= end || *p != '[') {
		return -EINVAL;
	}
	p = skip_ws(p + 1, end);
	if (p < end && *p == ']') {
		return 0;
	}
	while (p < end) {
		struct zbuddy_jtok t;

		p = scan_value(p, end, &t);
		if (p == NULL) {
			return -EINVAL;
		}
		if (fn != NULL && !fn(&t, (size_t)count, ctx)) {
			return count + 1;
		}
		count++;
		p = skip_ws(p, end);
		if (p >= end) {
			return -EINVAL;
		}
		if (*p == ']') {
			return count;
		}
		if (*p != ',') {
			return -EINVAL;
		}
		p++;
	}
	return -EINVAL;
}

int zbuddy_json_obj_each_tok(const struct zbuddy_jtok *obj, zbuddy_json_member_fn fn, void *ctx)
{
	if (obj->type != ZBUDDY_J_OBJECT) {
		return -EINVAL;
	}
	return zbuddy_json_obj_each(obj->start, (size_t)(obj->end - obj->start), fn, ctx);
}

int zbuddy_json_arr_each_tok(const struct zbuddy_jtok *arr, zbuddy_json_elem_fn fn, void *ctx)
{
	if (arr->type != ZBUDDY_J_ARRAY) {
		return -EINVAL;
	}
	return zbuddy_json_arr_each(arr->start, (size_t)(arr->end - arr->start), fn, ctx);
}

bool zbuddy_json_key_is(const struct zbuddy_jkv *kv, const char *name)
{
	size_t n = strlen(name);

	return (size_t)(kv->key.end - kv->key.start) == n && memcmp(kv->key.start, name, n) == 0;
}

static uint32_t hex4(const char *p)
{
	uint32_t v = 0;

	for (int i = 0; i < 4; i++) {
		char c = p[i];

		v <<= 4;
		if (c >= '0' && c <= '9') {
			v |= (uint32_t)(c - '0');
		} else if (c >= 'a' && c <= 'f') {
			v |= (uint32_t)(c - 'a' + 10);
		} else if (c >= 'A' && c <= 'F') {
			v |= (uint32_t)(c - 'A' + 10);
		}
	}
	return v;
}

static size_t put(char *dst, size_t dst_len, size_t pos, char c)
{
	if (dst != NULL && pos + 1 < dst_len) {
		dst[pos] = c;
	}
	return pos + 1;
}

static size_t put_utf8(char *dst, size_t dst_len, size_t pos, uint32_t cp)
{
	if (cp < 0x80) {
		return put(dst, dst_len, pos, (char)cp);
	}
	if (cp < 0x800) {
		pos = put(dst, dst_len, pos, (char)(0xC0 | (cp >> 6)));
		return put(dst, dst_len, pos, (char)(0x80 | (cp & 0x3F)));
	}
	if (cp < 0x10000) {
		pos = put(dst, dst_len, pos, (char)(0xE0 | (cp >> 12)));
		pos = put(dst, dst_len, pos, (char)(0x80 | ((cp >> 6) & 0x3F)));
		return put(dst, dst_len, pos, (char)(0x80 | (cp & 0x3F)));
	}
	pos = put(dst, dst_len, pos, (char)(0xF0 | (cp >> 18)));
	pos = put(dst, dst_len, pos, (char)(0x80 | ((cp >> 12) & 0x3F)));
	pos = put(dst, dst_len, pos, (char)(0x80 | ((cp >> 6) & 0x3F)));
	return put(dst, dst_len, pos, (char)(0x80 | (cp & 0x3F)));
}

size_t zbuddy_json_str(const struct zbuddy_jtok *v, char *dst, size_t dst_len)
{
	size_t pos = 0;
	const char *p, *end;

	if (dst != NULL && dst_len > 0) {
		dst[0] = '\0';
	}
	if (v == NULL || v->type != ZBUDDY_J_STRING) {
		return 0;
	}
	p = v->start;
	end = v->end;
	while (p < end) {
		char c = *p++;

		if (c != '\\' || p >= end) {
			pos = put(dst, dst_len, pos, c);
			continue;
		}
		c = *p++;
		switch (c) {
		case 'n':
			pos = put(dst, dst_len, pos, '\n');
			break;
		case 't':
			pos = put(dst, dst_len, pos, '\t');
			break;
		case 'r':
			pos = put(dst, dst_len, pos, '\r');
			break;
		case 'b':
			pos = put(dst, dst_len, pos, '\b');
			break;
		case 'f':
			pos = put(dst, dst_len, pos, '\f');
			break;
		case 'u':
			if (end - p >= 4) {
				uint32_t cp = hex4(p);

				p += 4;
				/* surrogate pair */
				if (cp >= 0xD800 && cp <= 0xDBFF && end - p >= 6 && p[0] == '\\' &&
				    p[1] == 'u') {
					uint32_t lo = hex4(p + 2);

					if (lo >= 0xDC00 && lo <= 0xDFFF) {
						cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
						p += 6;
					}
				}
				pos = put_utf8(dst, dst_len, pos, cp);
			}
			break;
		default:
			pos = put(dst, dst_len, pos, c);
			break;
		}
	}
	if (dst != NULL && dst_len > 0) {
		dst[pos < dst_len ? pos : dst_len - 1] = '\0';
	}
	return pos;
}

bool zbuddy_json_str_is(const struct zbuddy_jtok *v, const char *s)
{
	char buf[64];

	if (v == NULL || v->type != ZBUDDY_J_STRING) {
		return false;
	}
	zbuddy_json_str(v, buf, sizeof(buf));
	return strcmp(buf, s) == 0;
}

int64_t zbuddy_json_int(const struct zbuddy_jtok *v, int64_t def)
{
	char buf[32];
	size_t n;

	if (v == NULL) {
		return def;
	}
	if (v->type == ZBUDDY_J_TRUE) {
		return 1;
	}
	if (v->type == ZBUDDY_J_FALSE) {
		return 0;
	}
	if (v->type != ZBUDDY_J_NUMBER) {
		return def;
	}
	n = (size_t)(v->end - v->start);
	if (n >= sizeof(buf)) {
		n = sizeof(buf) - 1;
	}
	memcpy(buf, v->start, n);
	buf[n] = '\0';
	return strtoll(buf, NULL, 10);
}

bool zbuddy_json_bool(const struct zbuddy_jtok *v, bool def)
{
	if (v == NULL) {
		return def;
	}
	if (v->type == ZBUDDY_J_TRUE) {
		return true;
	}
	if (v->type == ZBUDDY_J_FALSE) {
		return false;
	}
	if (v->type == ZBUDDY_J_NUMBER) {
		return zbuddy_json_int(v, 0) != 0;
	}
	return def;
}

size_t zbuddy_json_escape(const char *s, char *dst, size_t dst_len)
{
	size_t pos = 0;

	for (; *s != '\0'; s++) {
		unsigned char c = (unsigned char)*s;

		if (c == '"' || c == '\\') {
			pos = put(dst, dst_len, pos, '\\');
			pos = put(dst, dst_len, pos, (char)c);
		} else if (c == '\n') {
			pos = put(dst, dst_len, pos, '\\');
			pos = put(dst, dst_len, pos, 'n');
		} else if (c < 0x20) {
			pos = put(dst, dst_len, pos, ' ');
		} else {
			pos = put(dst, dst_len, pos, (char)c);
		}
	}
	if (dst != NULL && dst_len > 0) {
		dst[pos < dst_len ? pos : dst_len - 1] = '\0';
	}
	return pos;
}
