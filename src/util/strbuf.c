#include "util/strbuf.h"

#include "util/types.h"

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void reserve(StrBuf *sb, size_t extra) {
  if (sb->len + extra + 1 <= sb->cap) {
    return;
  }
  size_t new_cap = sb->cap == 0 ? 64 : sb->cap;
  while (new_cap < sb->len + extra + 1) {
    new_cap *= 2;
  }
  char *grown = realloc(sb->data, new_cap);
  if (grown == nullptr) {
    abort();
  }
  sb->data = grown;
  sb->cap = new_cap;
}

void strbuf_append_str(StrBuf *sb, Str s) {
  reserve(sb, s.len);
  memcpy(sb->data + sb->len, s.data, s.len);
  sb->len += s.len;
  sb->data[sb->len] = '\0';
}

void strbuf_append(StrBuf *sb, const char *s) {
  strbuf_append_str(sb, (Str){.data = s, .len = strlen(s)});
}

void strbuf_append_char(StrBuf *sb, char c) { strbuf_append_str(sb, (Str){.data = &c, .len = 1}); }

void strbuf_appendf(StrBuf *sb, const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  va_list measure;
  va_copy(measure, args);
  int n = vsnprintf(nullptr, 0, fmt, measure);
  va_end(measure);
  if (n < 0) {
    abort();
  }
  reserve(sb, (size_t)n);
  (void)vsnprintf(sb->data + sb->len, (size_t)n + 1, fmt, args);
  va_end(args);
  sb->len += (size_t)n;
}

void strbuf_free(StrBuf *sb) {
  free(sb->data);
  *sb = (StrBuf){};
}
