#pragma once

#include "frontend/schema.h"
#include "util/diag.h"

#include <stddef.h>

/// Prints a diagnostic as "path:line:col: error: message", or without the
/// location prefix when the diagnostic has none.
void cmd_print_diag(const char *path, const Diag *diag);

/// Reads, parses, and checks a schema, printing errors on failure. On
/// success the caller owns *text_out (which the schema borrows) and frees
/// the schema with schema_free.
[[nodiscard]] bool cmd_load_schema(const char *path, char **text_out, Schema *schema);
