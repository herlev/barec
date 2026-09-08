#include "cli/cmd/common.h"

#include "cli/cli.h"
#include "frontend/check.h"
#include "frontend/parser.h"
#include "frontend/schema.h"
#include "util/diag.h"
#include "util/file.h"

#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
  char *text = file_read(path, &len);
  if (text == nullptr) {
    (void)fprintf(stderr, "%s: error: cannot open '%s': %s\n", cli_prog_name(), path,
                  strerror(errno));
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
