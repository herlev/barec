#pragma once

#include <stdio.h>
#include <stdlib.h>

#define ARRAY_LEN(a) (sizeof(a) / sizeof((a)[0]))

/// Aborting variant of C23's unreachable(), which is an assume-style marker
/// with undefined behavior when actually reached.
#define UNREACHABLE()                                                                              \
  do {                                                                                             \
    fprintf(stderr, "%s:%d: unreachable\n", __FILE__, __LINE__);                                   \
    abort();                                                                                       \
  } while (0)
