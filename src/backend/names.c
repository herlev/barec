#include "backend/names.h"

#include "backend/config.h"
#include "util/strbuf.h"
#include "util/types.h"

#include <stddef.h>

static bool is_upper(char c) { return (bool)(c >= 'A' && c <= 'Z'); }

static bool is_lower(char c) { return (bool)(c >= 'a' && c <= 'z'); }

static bool is_digit(char c) { return (bool)(c >= '0' && c <= '9'); }

static char to_lower(char c) {
  if (is_upper(c)) {
    return (char)(c + ('a' - 'A'));
  }
  return c;
}

static char to_upper(char c) {
  if (is_lower(c)) {
    return (char)(c - ('a' - 'A'));
  }
  return c;
}

typedef struct {
  Str src;
  size_t pos;
} WordIter;

static bool next_word(WordIter *it, Str *out) {
  const char *data = it->src.data;
  size_t len = it->src.len;
  while (it->pos < len && data[it->pos] == '_') {
    it->pos += 1;
  }
  if (it->pos == len) {
    return false;
  }
  size_t start = it->pos;
  it->pos += 1;
  if (is_upper(data[start]) && it->pos < len && is_upper(data[it->pos])) {
    while (it->pos < len && (is_upper(data[it->pos]) || is_digit(data[it->pos]))) {
      it->pos += 1;
    }
    if (it->pos < len && is_lower(data[it->pos])) {
      it->pos -= 1;
    }
  } else {
    while (it->pos < len && (is_lower(data[it->pos]) || is_digit(data[it->pos]))) {
      it->pos += 1;
    }
  }
  *out = (Str){.data = data + start, .len = it->pos - start};
  return true;
}

void name_render(Str name, CaseStyle style, StrBuf *out) {
  WordIter it = {.src = name};
  Str word;
  bool first = true;
  while (next_word(&it, &word)) {
    if (!first && (style == CaseStyle_SNAKE || style == CaseStyle_SCREAMING)) {
      strbuf_append_char(out, '_');
    }
    for (size_t i = 0; i < word.len; i++) {
      char c = word.data[i];
      switch (style) {
      case CaseStyle_SNAKE:
        c = to_lower(c);
        break;
      case CaseStyle_SCREAMING:
        c = to_upper(c);
        break;
      case CaseStyle_PASCAL:
        if (i == 0) {
          c = to_upper(c);
        } else {
          c = to_lower(c);
        }
        break;
      case CaseStyle_CAMEL:
        if (i == 0 && !first) {
          c = to_upper(c);
        } else {
          c = to_lower(c);
        }
        break;
      }
      strbuf_append_char(out, c);
    }
    first = false;
  }
}
