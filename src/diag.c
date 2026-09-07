#include "diag.h"

#include <stdarg.h>
#include <stdio.h>

void diag_set(Diag *diag, SrcLoc loc, const char *fmt, ...) {
  diag->loc = loc;
  va_list args;
  va_start(args, fmt);
  (void)vsnprintf(diag->message, sizeof(diag->message), fmt, args);
  va_end(args);
}
