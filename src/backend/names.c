#include "backend/names.h"

#include "backend/config.h"
#include "util/ascii.h"
#include "util/strbuf.h"
#include "util/types.h"

#include <stddef.h>

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
  if (ascii_is_upper(data[start]) && it->pos < len && ascii_is_upper(data[it->pos])) {
    while (it->pos < len && (ascii_is_upper(data[it->pos]) || ascii_is_digit(data[it->pos]))) {
      it->pos += 1;
    }
    if (it->pos < len && ascii_is_lower(data[it->pos])) {
      it->pos -= 1;
    }
  } else {
    while (it->pos < len && (ascii_is_lower(data[it->pos]) || ascii_is_digit(data[it->pos]))) {
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
        c = ascii_to_lower(c);
        break;
      case CaseStyle_SCREAMING:
        c = ascii_to_upper(c);
        break;
      case CaseStyle_PASCAL:
        if (i == 0) {
          c = ascii_to_upper(c);
        } else {
          c = ascii_to_lower(c);
        }
        break;
      case CaseStyle_CAMEL:
        if (i == 0 && !first) {
          c = ascii_to_upper(c);
        } else {
          c = ascii_to_lower(c);
        }
        break;
      }
      strbuf_append_char(out, c);
    }
    first = false;
  }
}
