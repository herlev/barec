#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L
#define BARE_NODISCARD [[nodiscard]]
#define BARE_ENUM_U8 : uint8_t
#define BARE_TYPEOF(x) typeof(x)
#else
#define BARE_ENUM_U8
#ifdef __GNUC__
#define BARE_NODISCARD __attribute__((warn_unused_result))
#define BARE_TYPEOF(x) __typeof__(x)
#else
#define BARE_NODISCARD
#endif
#endif

/// Evaluates a BareStatus expression and returns it from the enclosing
/// function unless it is BareStatus_OK.
#define BARE_TRY(expr)                                                                             \
  do {                                                                                             \
    BareStatus bare_try_status_ = (expr);                                                          \
    if (bare_try_status_ != BareStatus_OK) {                                                       \
      return bare_try_status_;                                                                     \
    }                                                                                              \
  } while (0)

#ifdef __has_attribute
#if __has_attribute(nonstring)
#define BARE_NONSTRING __attribute__((nonstring))
#endif
#endif
#ifndef BARE_NONSTRING
#define BARE_NONSTRING
#endif

typedef enum BARE_ENUM_U8 {
  BareStatus_OK,
  BareStatus_SHORT_READ,
  BareStatus_SHORT_WRITE,
  BareStatus_OVERLONG_VARINT,
  BareStatus_VARINT_TOO_LONG,
  BareStatus_INVALID_BOOL,
  BareStatus_INVALID_UTF8,
  BareStatus_INVALID_ENUM,
  BareStatus_INVALID_TAG,
  BareStatus_INVALID_OPTIONAL,
  BareStatus_DUPLICATE_KEY,
  BareStatus_CAP_EXCEEDED,
  BareStatus_TRAILING_DATA,
} BareStatus;

typedef struct {
  const uint8_t *data;
  size_t len;
  size_t pos;
} BareReader;

typedef struct {
  uint8_t *data;
  size_t cap;
  size_t len;
} BareWriter;

BareReader bare_reader_new(const uint8_t data[], size_t len);
BareWriter bare_writer_new(uint8_t data[], size_t cap);
size_t bare_reader_remaining(const BareReader *r);

/// Advances past n octets without reading them, SHORT_READ if fewer remain.
BARE_NODISCARD BareStatus bare_reader_skip(BareReader *r, uint64_t n);

/// Reads and discards one uint, enforcing the same encoding rules as
/// bare_read_uint.
BARE_NODISCARD BareStatus bare_skip_uint(BareReader *r);

/// uint/int are ULEB128 with zig-zag for int, at most 10 octets and 64 bits.
/// Reads reject non-minimal encodings with OVERLONG_VARINT and encodings
/// exceeding 64 bits or 10 octets with VARINT_TOO_LONG.
BARE_NODISCARD BareStatus bare_read_uint(BareReader *r, uint64_t *out);
BARE_NODISCARD BareStatus bare_read_int(BareReader *r, int64_t *out);
BARE_NODISCARD BareStatus bare_read_u8(BareReader *r, uint8_t *out);
BARE_NODISCARD BareStatus bare_read_u16(BareReader *r, uint16_t *out);
BARE_NODISCARD BareStatus bare_read_u32(BareReader *r, uint32_t *out);
BARE_NODISCARD BareStatus bare_read_u64(BareReader *r, uint64_t *out);
BARE_NODISCARD BareStatus bare_read_i8(BareReader *r, int8_t *out);
BARE_NODISCARD BareStatus bare_read_i16(BareReader *r, int16_t *out);
BARE_NODISCARD BareStatus bare_read_i32(BareReader *r, int32_t *out);
BARE_NODISCARD BareStatus bare_read_i64(BareReader *r, int64_t *out);
BARE_NODISCARD BareStatus bare_read_f32(BareReader *r, float *out);
BARE_NODISCARD BareStatus bare_read_f64(BareReader *r, double *out);
BARE_NODISCARD BareStatus bare_read_bool(BareReader *r, bool *out);

/// str/data read the uint length prefix, fail with CAP_EXCEEDED if it does
/// not fit cap, and copy the payload into buf. str additionally validates
/// UTF-8. The fixed variant reads exactly len octets with no prefix.
BARE_NODISCARD BareStatus bare_read_str(BareReader *r, char buf[], uint32_t cap, uint32_t *len);
BARE_NODISCARD BareStatus bare_read_data(BareReader *r, uint8_t buf[], uint32_t cap, uint32_t *len);
BARE_NODISCARD BareStatus bare_read_data_fixed(BareReader *r, uint8_t buf[], size_t len);

/// Encoded size in octets of a uint or int, without writing it.
BARE_NODISCARD uint64_t bare_uint_size(uint64_t value);
BARE_NODISCARD uint64_t bare_int_size(int64_t value);

BARE_NODISCARD BareStatus bare_write_uint(BareWriter *w, uint64_t value);
BARE_NODISCARD BareStatus bare_write_int(BareWriter *w, int64_t value);
BARE_NODISCARD BareStatus bare_write_u8(BareWriter *w, uint8_t value);
BARE_NODISCARD BareStatus bare_write_u16(BareWriter *w, uint16_t value);
BARE_NODISCARD BareStatus bare_write_u32(BareWriter *w, uint32_t value);
BARE_NODISCARD BareStatus bare_write_u64(BareWriter *w, uint64_t value);
BARE_NODISCARD BareStatus bare_write_i8(BareWriter *w, int8_t value);
BARE_NODISCARD BareStatus bare_write_i16(BareWriter *w, int16_t value);
BARE_NODISCARD BareStatus bare_write_i32(BareWriter *w, int32_t value);
BARE_NODISCARD BareStatus bare_write_i64(BareWriter *w, int64_t value);
BARE_NODISCARD BareStatus bare_write_f32(BareWriter *w, float value);
BARE_NODISCARD BareStatus bare_write_f64(BareWriter *w, double value);
BARE_NODISCARD BareStatus bare_write_bool(BareWriter *w, bool value);

/// str/data write the uint length prefix followed by the payload. str fails
/// with INVALID_UTF8 on invalid input. The fixed variant writes exactly len
/// octets with no prefix.
BARE_NODISCARD BareStatus bare_write_str(BareWriter *w, const char buf[], uint32_t len);
BARE_NODISCARD BareStatus bare_write_data(BareWriter *w, const uint8_t buf[], uint32_t len);
BARE_NODISCARD BareStatus bare_write_data_fixed(BareWriter *w, const uint8_t buf[], size_t len);

/// RFC 3629 validation, rejects overlong forms, surrogates, and values
/// beyond U+10FFFF.
BARE_NODISCARD bool bare_utf8_valid(const uint8_t data[], size_t len);

/// Convenience helpers for generated fixed-capacity string fields, which
/// all share the shape { char data[N]; uint32_t len; }:
///   BARE_TRY(BARE_STR_SET(&msg.label, "greenhouse"));
///   if (BARE_STR_EQ(&msg.label, "greenhouse")) { ... }
///   printf("%.*s", BARE_STR_ARG(&msg.label));
#define BARE_STR_SET(field, text)                                                                  \
  bare_str_set((field)->data, sizeof((field)->data), &(field)->len, (text))
#define BARE_STR_EQ(field, text) bare_str_eq((field)->data, (field)->len, (text))
#define BARE_STR_ARG(field) (int)(field)->len, (field)->data

/// Assigns a string literal to a fixed-capacity string field through a
/// pointer, like BARE_STR_SET. A literal longer than the field's capacity
/// is rejected at compile time, so there is no status to check.
#ifdef BARE_TYPEOF
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#define BARE_STR_LIT(field, lit)                                                                   \
  do {                                                                                             \
    _Static_assert(sizeof(lit) - 1 <= sizeof((field)->data),                                       \
                   "string literal exceeds the field's capacity");                                 \
    *(field) = (BARE_TYPEOF(*(field))){.data = "" lit, .len = sizeof(lit) - 1};                    \
  } while (0)
#else
#define BARE_STR_LIT(field, lit)                                                                   \
  ((void)sizeof(char[sizeof(lit) - 1 <= sizeof((field)->data) ? 1 : -1]),                          \
   (void)(*(field) = (BARE_TYPEOF(*(field))){.data = "" lit, .len = sizeof(lit) - 1}))
#endif
#endif

/// Copies NUL-terminated text into a cap-bounded field, CAP_EXCEEDED when
/// it does not fit. The terminator is not stored and len does not count it.
BARE_NODISCARD BareStatus bare_str_set(char data[], size_t cap, uint32_t *len, const char *text);
BARE_NODISCARD bool bare_str_eq(const char data[], uint32_t len, const char *text);
