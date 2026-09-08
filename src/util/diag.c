#include "util/diag.h"

#include "util/optional.h"

#include <stdarg.h>
#include <stdio.h>

static void set_message(Diag *diag, const char *fmt, va_list args) {
  vsnprintf(diag->message, sizeof(diag->message), fmt, args);
}

void diag_set(Diag *diag, SrcLoc loc, const char *fmt, ...) {
  diag->loc = (OPTIONAL(SrcLoc)){.has_value = true, .value = loc};
  va_list args;
  va_start(args, fmt);
  set_message(diag, fmt, args);
  va_end(args);
}

void diag_set_global(Diag *diag, const char *fmt, ...) {
  diag->loc = (OPTIONAL(SrcLoc)){};
  va_list args;
  va_start(args, fmt);
  set_message(diag, fmt, args);
  va_end(args);
}
