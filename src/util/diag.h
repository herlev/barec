#pragma once

#include "util/optional.h"
#include "util/types.h"

typedef struct {
  u32 line;   // 1-based
  u32 column; // 1-based
} SrcLoc;

/// One error with an optional location, formatted into a fixed buffer.
typedef struct {
  OPTIONAL(SrcLoc) loc;
  char message[256];
} Diag;

/// Formats the message with a location, truncating if it does not fit.
[[gnu::format(printf, 3, 4)]]
void diag_set(Diag *diag, SrcLoc loc, const char *fmt, ...);

/// Formats the message without a location.
[[gnu::format(printf, 2, 3)]]
void diag_set_global(Diag *diag, const char *fmt, ...);
