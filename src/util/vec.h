#pragma once

#include <stddef.h>
#include <stdlib.h>

/// Growable array. Uses of VEC(T) with the same T are compatible types via
/// the pasted tag, like OPTIONAL(T), so T must be a single identifier.
/// VEC_PUSH aborts on allocation failure. Free with free(vec.ptr).
#define VEC(T)                                                                                     \
  struct Vec_##T {                                                                                 \
    T *ptr;                                                                                        \
    size_t len;                                                                                    \
    size_t cap;                                                                                    \
  }

/// A frozen counted array, typically the result of a VEC whose growth is
/// done. Compatible across identical instantiations like VEC and OPTIONAL.
#define SLICE(T)                                                                                   \
  struct Slice_##T {                                                                               \
    T *ptr;                                                                                        \
    size_t len;                                                                                    \
  }

#define VEC_PUSH(vec, item)                                                                        \
  do {                                                                                             \
    if ((vec)->len == (vec)->cap) {                                                                \
      (vec)->cap = (vec)->cap == 0 ? 8 : (vec)->cap * 2;                                           \
      void *vec_grown_ = realloc((void *)(vec)->ptr, (vec)->cap * sizeof(*(vec)->ptr));            \
      if (vec_grown_ == nullptr) {                                                                 \
        abort();                                                                                   \
      }                                                                                            \
      (vec)->ptr = (typeof((vec)->ptr))vec_grown_;                                                 \
    }                                                                                              \
    (vec)->ptr[(vec)->len] = (item);                                                               \
    (vec)->len += 1;                                                                               \
  } while (0)
