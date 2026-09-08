#include "util/file.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

char *file_read(const char *path, size_t *out_len) {
  FILE *file = fopen(path, "rb");
  if (file == nullptr) {
    return nullptr;
  }
  char *text = nullptr;
  size_t len = 0;
  size_t cap = 0;
  bool ok = true;
  for (;;) {
    if (len == cap) {
      cap = cap == 0 ? 4096 : cap * 2;
      char *grown = realloc(text, cap);
      if (grown == nullptr) {
        abort();
      }
      text = grown;
    }
    size_t want = cap - len;
    size_t n = fread(text + len, 1, want, file);
    len += n;
    if (n < want) {
      ok = ferror(file) == 0;
      break;
    }
  }
  int read_errno = errno;
  fclose(file);
  if (!ok) {
    free(text);
    errno = read_errno;
    return nullptr;
  }
  *out_len = len;
  return text;
}
