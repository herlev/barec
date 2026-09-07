#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L
#define BARE_NODISCARD [[nodiscard]]
#define BARE_ENUM_U8 : uint8_t
#else
#define BARE_ENUM_U8
#ifdef __GNUC__
#define BARE_NODISCARD __attribute__((warn_unused_result))
#else
#define BARE_NODISCARD
#endif
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
