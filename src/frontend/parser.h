#pragma once

#include "frontend/schema.h"
#include "util/diag.h"

#include <stddef.h>

/// Parses schema source into a Type tree, syntax only. Explicit enum values
/// and union tags are recorded verbatim, auto-assignment and all semantic
/// invariants are left to check_schema. On success the caller owns *out and
/// releases it with schema_free. All names in *out view src, which must
/// outlive the schema.
[[nodiscard]] bool parser_parse(const char *src, size_t len, Schema *out, Diag *diag);
