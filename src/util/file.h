#pragma once

#include "util/types.h"

#include <stddef.h>

/// Reads a whole file into an owned NUL-free buffer, returning nullptr
/// with errno set on failure. The caller frees the result.
[[nodiscard]] char *_Nullable file_read(const char *path, size_t *out_len);
