#pragma once

#include "util/types.h"

#include <stddef.h>

/// Growable string builder, data is NUL-terminated once anything has been
/// appended. Aborts on allocation failure. Free with strbuf_free.
typedef struct {
  char *data;
  size_t len;
  size_t cap;
} StrBuf;

void strbuf_append(StrBuf *sb, const char *s);
void strbuf_append_char(StrBuf *sb, char c);
void strbuf_append_str(StrBuf *sb, Str s);
[[gnu::format(printf, 2, 3)]]
void strbuf_appendf(StrBuf *sb, const char *fmt, ...);
void strbuf_free(StrBuf *sb);
