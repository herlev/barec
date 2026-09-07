#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#ifndef __clang__
#define _Nullable
#endif

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;
typedef float f32;
typedef double f64;

/// Fat string view, not NUL-terminated, print with "%.*s". Owned strings
/// backing a Str may append a terminator for convenience, but len never
/// counts it.
typedef struct {
  const char *data;
  size_t len;
} Str;

#define STR(lit) ((Str){.data = (lit), .len = sizeof(lit) - 1})

static inline bool str_eq(Str a, Str b) {
  return (bool)(a.len == b.len && (a.len == 0 || memcmp(a.data, b.data, a.len) == 0));
}
