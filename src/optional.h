#pragma once

/// Distinct uses of OPTIONAL(T) with the same T are compatible types under
/// C23's structural compatibility rule for untagged structs, so no typedef
/// per T is needed.
#define OPTIONAL(T)                                                                                \
  struct {                                                                                         \
    bool has_value;                                                                                \
    T value;                                                                                       \
  }
