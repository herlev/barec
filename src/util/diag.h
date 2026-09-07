#pragma once

#include "util/types.h"

typedef struct {
  u32 line;
  u32 column;
} SrcLoc;

typedef struct {
  SrcLoc loc;
  char message[256];
} Diag;

[[gnu::format(printf, 3, 4)]]
void diag_set(Diag *diag, SrcLoc loc, const char *fmt, ...);
