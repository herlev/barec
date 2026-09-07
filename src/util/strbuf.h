#pragma once

#include <stddef.h>

/// Growable NUL-terminated string builder, data is valid while the StrBuf
/// lives and always NUL-terminated once anything has been appended.
typedef struct {
  char *data;
  size_t len;
  size_t cap;
} StrBuf;

void strbuf_append(StrBuf *sb, const char *s);
[[gnu::format(printf, 2, 3)]]
void strbuf_appendf(StrBuf *sb, const char *fmt, ...);
void strbuf_free(StrBuf *sb);
