#include "cli/cmd/common.h"

#include "cli/cli.h"
#include "frontend/check.h"
#include "frontend/parser.h"
#include "frontend/schema.h"
#include "util/diag.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

char *cmd_read_file(const char *path, size_t *out_len) {
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
  (void)fclose(file);
  if (!ok) {
    free(text);
    return nullptr;
  }
  *out_len = len;
  return text;
}

void cmd_print_diag(const char *path, const Diag *diag) {
  if (diag->loc.line > 0) {
    (void)fprintf(stderr, "%s:%u:%u: error: %s\n", path, diag->loc.line, diag->loc.column,
                  diag->message);
  } else {
    (void)fprintf(stderr, "%s: error: %s\n", cli_prog_name(), diag->message);
  }
}

bool cmd_load_schema(const char *path, char **text_out, Schema *schema) {
  size_t len = 0;
  char *text = cmd_read_file(path, &len);
  if (text == nullptr) {
    (void)fprintf(stderr, "%s: error: cannot open '%s'\n", cli_prog_name(), path);
    return false;
  }
  Diag diag = {};
  if (!parser_parse(text, len, schema, &diag)) {
    cmd_print_diag(path, &diag);
    free(text);
    return false;
  }
  if (!check_schema(schema, &diag)) {
    cmd_print_diag(path, &diag);
    schema_free(schema);
    free(text);
    return false;
  }
  *text_out = text;
  return true;
}
