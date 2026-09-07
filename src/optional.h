#pragma once

/// Uses of OPTIONAL(T) with the same T are compatible types everywhere: the
/// pasted tag makes each expansion a C23 redeclaration of the same struct.
/// Untagged structs would remain distinct types within one translation unit.
/// T must be a single identifier.
#define OPTIONAL(T)                                                                                \
  struct Optional_##T {                                                                            \
    bool has_value;                                                                                \
    T value;                                                                                       \
  }
