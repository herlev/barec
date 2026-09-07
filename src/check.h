#pragma once

#include "diag.h"
#include "schema.h"

/// Validates the spec's semantic invariants and completes the tree in place:
/// resolves user-type references (defined before use, no recursion), assigns
/// implicit enum values and union tags, and enforces uniqueness, ascending
/// order, void/map-key restrictions, non-empty aggregates, and fixed-length
/// bounds. After a successful check the tree is fully resolved and codegen
/// performs no validation of its own.
[[nodiscard]] bool check_schema(Schema *schema, Diag *diag);
