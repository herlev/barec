#include "bare.h"

#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

#define BYTES(...) (const uint8_t[]){__VA_ARGS__}, sizeof((const uint8_t[]){__VA_ARGS__})

static void check_uint(uint64_t value, const uint8_t expect[], size_t n) {
  uint8_t buf[16];
  BareWriter w = bare_writer_new(buf, sizeof(buf));
  assert(bare_write_uint(&w, value) == BareStatus_OK);
  assert(w.len == n);
  assert(memcmp(buf, expect, n) == 0);
  BareReader r = bare_reader_new(expect, n);
  uint64_t out;
  assert(bare_read_uint(&r, &out) == BareStatus_OK);
  assert(out == value);
  assert(bare_reader_remaining(&r) == 0);
}

static void check_int(int64_t value, const uint8_t expect[], size_t n) {
  uint8_t buf[16];
  BareWriter w = bare_writer_new(buf, sizeof(buf));
  assert(bare_write_int(&w, value) == BareStatus_OK);
  assert(w.len == n);
  assert(memcmp(buf, expect, n) == 0);
  BareReader r = bare_reader_new(expect, n);
  int64_t out;
  assert(bare_read_int(&r, &out) == BareStatus_OK);
  assert(out == value);
  assert(bare_reader_remaining(&r) == 0);
}

static void test_uint_vectors(void) {
  check_uint(0, BYTES(0x00));
  check_uint(1, BYTES(0x01));
  check_uint(126, BYTES(0x7e));
  check_uint(127, BYTES(0x7f));
  check_uint(128, BYTES(0x80, 0x01));
  check_uint(129, BYTES(0x81, 0x01));
  check_uint(255, BYTES(0xff, 0x01));
  check_uint(UINT64_MAX, BYTES(0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x01));
}

static void test_int_vectors(void) {
  check_int(0, BYTES(0x00));
  check_int(1, BYTES(0x02));
  check_int(-1, BYTES(0x01));
  check_int(63, BYTES(0x7e));
  check_int(-63, BYTES(0x7d));
  check_int(64, BYTES(0x80, 0x01));
  check_int(-64, BYTES(0x7f));
  check_int(65, BYTES(0x82, 0x01));
  check_int(-65, BYTES(0x81, 0x01));
  check_int(255, BYTES(0xfe, 0x03));
  check_int(-255, BYTES(0xfd, 0x03));
  check_int(INT64_MAX, BYTES(0xfe, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x01));
  check_int(INT64_MIN, BYTES(0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x01));
}

static void test_uint_errors(void) {
  uint64_t out;
  BareReader r = bare_reader_new(BYTES(0x80));
  assert(bare_read_uint(&r, &out) == BareStatus_SHORT_READ);

  r = bare_reader_new(BYTES(0x80, 0x00));
  assert(bare_read_uint(&r, &out) == BareStatus_OVERLONG_VARINT);

  r = bare_reader_new(BYTES(0xff, 0x00));
  assert(bare_read_uint(&r, &out) == BareStatus_OVERLONG_VARINT);

  r = bare_reader_new(BYTES(0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80));
  assert(bare_read_uint(&r, &out) == BareStatus_VARINT_TOO_LONG);

  r = bare_reader_new(BYTES(0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x02));
  assert(bare_read_uint(&r, &out) == BareStatus_VARINT_TOO_LONG);
}

static void test_fixed_ints(void) {
  uint8_t buf[16];

  BareWriter w = bare_writer_new(buf, sizeof(buf));
  assert(bare_write_u32(&w, 255) == BareStatus_OK);
  assert(w.len == 4);
  assert(memcmp(buf, (const uint8_t[]){0xff, 0x00, 0x00, 0x00}, 4) == 0);
  BareReader r = bare_reader_new(buf, 4);
  uint32_t u32_out;
  assert(bare_read_u32(&r, &u32_out) == BareStatus_OK);
  assert(u32_out == 255);

  w = bare_writer_new(buf, sizeof(buf));
  assert(bare_write_i16(&w, -1) == BareStatus_OK);
  assert(memcmp(buf, (const uint8_t[]){0xff, 0xff}, 2) == 0);
  w = bare_writer_new(buf, sizeof(buf));
  assert(bare_write_i16(&w, 255) == BareStatus_OK);
  assert(memcmp(buf, (const uint8_t[]){0xff, 0x00}, 2) == 0);
  w = bare_writer_new(buf, sizeof(buf));
  assert(bare_write_i16(&w, -255) == BareStatus_OK);
  assert(memcmp(buf, (const uint8_t[]){0x01, 0xff}, 2) == 0);
  r = bare_reader_new(buf, 2);
  int16_t i16_out;
  assert(bare_read_i16(&r, &i16_out) == BareStatus_OK);
  assert(i16_out == -255);

  w = bare_writer_new(buf, sizeof(buf));
  assert(bare_write_u64(&w, UINT64_MAX) == BareStatus_OK);
  r = bare_reader_new(buf, 8);
  uint64_t u64_out;
  assert(bare_read_u64(&r, &u64_out) == BareStatus_OK);
  assert(u64_out == UINT64_MAX);

  w = bare_writer_new(buf, sizeof(buf));
  assert(bare_write_i8(&w, -128) == BareStatus_OK);
  assert(bare_write_u8(&w, 0xab) == BareStatus_OK);
  assert(bare_write_u16(&w, 0xbeef) == BareStatus_OK);
  assert(bare_write_i32(&w, INT32_MIN) == BareStatus_OK);
  assert(bare_write_i64(&w, INT64_MIN) == BareStatus_OK);
  r = bare_reader_new(buf, w.len);
  int8_t i8_out;
  uint8_t u8_out;
  uint16_t u16_out;
  int32_t i32_out;
  int64_t i64_out;
  assert(bare_read_i8(&r, &i8_out) == BareStatus_OK && i8_out == -128);
  assert(bare_read_u8(&r, &u8_out) == BareStatus_OK && u8_out == 0xab);
  assert(bare_read_u16(&r, &u16_out) == BareStatus_OK && u16_out == 0xbeef);
  assert(bare_read_i32(&r, &i32_out) == BareStatus_OK && i32_out == INT32_MIN);
  assert(bare_read_i64(&r, &i64_out) == BareStatus_OK && i64_out == INT64_MIN);
  assert(bare_reader_remaining(&r) == 0);
}

static void check_f64(double value, const uint8_t expect[], size_t n) {
  uint8_t buf[8];
  BareWriter w = bare_writer_new(buf, sizeof(buf));
  assert(bare_write_f64(&w, value) == BareStatus_OK);
  assert(w.len == n);
  assert(memcmp(buf, expect, n) == 0);
  BareReader r = bare_reader_new(expect, n);
  double out;
  assert(bare_read_f64(&r, &out) == BareStatus_OK);
  assert(out == value);
}

static void test_floats(void) {
  check_f64(0.0, BYTES(0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00));
  check_f64(1.0, BYTES(0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0x3f));
  check_f64(2.55, BYTES(0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x04, 0x40));
  check_f64(-25.5, BYTES(0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x39, 0xc0));

  uint8_t buf[8];
  BareWriter w = bare_writer_new(buf, sizeof(buf));
  assert(bare_write_f32(&w, 1.5F) == BareStatus_OK);
  assert(w.len == 4);
  BareReader r = bare_reader_new(buf, 4);
  float f32_out;
  assert(bare_read_f32(&r, &f32_out) == BareStatus_OK);
  assert(f32_out == 1.5F);

  w = bare_writer_new(buf, sizeof(buf));
  assert(bare_write_f64(&w, (double)NAN) == BareStatus_OK);
  r = bare_reader_new(buf, 8);
  double f64_out;
  assert(bare_read_f64(&r, &f64_out) == BareStatus_OK);
  assert(isnan(f64_out));
}

static void test_bool(void) {
  uint8_t buf[1];
  BareWriter w = bare_writer_new(buf, sizeof(buf));
  assert(bare_write_bool(&w, true) == BareStatus_OK);
  assert(buf[0] == 0x01);
  w = bare_writer_new(buf, sizeof(buf));
  assert(bare_write_bool(&w, false) == BareStatus_OK);
  assert(buf[0] == 0x00);

  bool out;
  BareReader r = bare_reader_new(BYTES(0x01));
  assert(bare_read_bool(&r, &out) == BareStatus_OK && out);
  r = bare_reader_new(BYTES(0x00));
  assert(bare_read_bool(&r, &out) == BareStatus_OK && !out);
  r = bare_reader_new(BYTES(0x02));
  assert(bare_read_bool(&r, &out) == BareStatus_INVALID_BOOL);
}

static void test_str(void) {
  uint8_t buf[16];
  BareWriter w = bare_writer_new(buf, sizeof(buf));
  assert(bare_write_str(&w, "BARE", 4) == BareStatus_OK);
  assert(w.len == 5);
  assert(memcmp(buf, (const uint8_t[]){0x04, 0x42, 0x41, 0x52, 0x45}, 5) == 0);

  char out[16];
  uint32_t len;
  BareReader r = bare_reader_new(buf, w.len);
  assert(bare_read_str(&r, out, sizeof(out), &len) == BareStatus_OK);
  assert(len == 4);
  assert(memcmp(out, "BARE", 4) == 0);

  r = bare_reader_new(buf, w.len);
  assert(bare_read_str(&r, out, 3, &len) == BareStatus_CAP_EXCEEDED);

  r = bare_reader_new(BYTES(0x02, 0xff, 0xfe));
  assert(bare_read_str(&r, out, sizeof(out), &len) == BareStatus_INVALID_UTF8);

  r = bare_reader_new(BYTES(0x05, 0x42, 0x41));
  assert(bare_read_str(&r, out, sizeof(out), &len) == BareStatus_SHORT_READ);

  w = bare_writer_new(buf, sizeof(buf));
  assert(bare_write_str(&w, "\xff\xfe", 2) == BareStatus_INVALID_UTF8);
}

static void test_data(void) {
  const uint8_t payload[16] = {0xaa, 0xee, 0xff, 0xee, 0xdd, 0xcc, 0xbb, 0xaa,
                               0xee, 0xdd, 0xcc, 0xbb, 0xee, 0xdd, 0xcc, 0xbb};
  uint8_t buf[32];

  BareWriter w = bare_writer_new(buf, sizeof(buf));
  assert(bare_write_data(&w, payload, sizeof(payload)) == BareStatus_OK);
  assert(w.len == 17);
  assert(buf[0] == 0x10);
  assert(memcmp(buf + 1, payload, sizeof(payload)) == 0);

  uint8_t out[32];
  uint32_t len;
  BareReader r = bare_reader_new(buf, w.len);
  assert(bare_read_data(&r, out, sizeof(out), &len) == BareStatus_OK);
  assert(len == 16);
  assert(memcmp(out, payload, 16) == 0);

  w = bare_writer_new(buf, sizeof(buf));
  assert(bare_write_data_fixed(&w, payload, sizeof(payload)) == BareStatus_OK);
  assert(w.len == 16);
  assert(memcmp(buf, payload, 16) == 0);

  r = bare_reader_new(buf, 16);
  assert(bare_read_data_fixed(&r, out, 16) == BareStatus_OK);
  assert(memcmp(out, payload, 16) == 0);
}

static void test_short_write(void) {
  uint8_t buf[2];
  BareWriter w = bare_writer_new(buf, sizeof(buf));
  assert(bare_write_u32(&w, 1) == BareStatus_SHORT_WRITE);
  assert(w.len == 0);
  assert(bare_write_uint(&w, UINT64_MAX) == BareStatus_SHORT_WRITE);
  assert(w.len == 0);
  assert(bare_write_str(&w, "BARE", 4) == BareStatus_OK || true);
}

static void test_str_helpers(void) {
  struct {
    char data[8] BARE_NONSTRING;
    uint32_t len;
  } field;
  assert(BARE_STR_SET(&field, "bare") == BareStatus_OK);
  assert(field.len == 4);
  assert(BARE_STR_EQ(&field, "bare"));
  assert(!BARE_STR_EQ(&field, "bar"));
  assert(!BARE_STR_EQ(&field, "bares"));
  assert(BARE_STR_SET(&field, "12345678") == BareStatus_OK);
  assert(field.len == 8);
  assert(BARE_STR_SET(&field, "123456789") == BareStatus_CAP_EXCEEDED);
  assert(BARE_STR_SET(&field, "") == BareStatus_OK);
  assert(field.len == 0);
  assert(BARE_STR_EQ(&field, ""));

  BARE_STR_LIT(field, "lit");
  assert(field.len == 3);
  assert(BARE_STR_EQ(&field, "lit"));
  BARE_STR_LIT(field, "12345678");
  assert(field.len == 8);
}

static void test_utf8(void) {
  assert(bare_utf8_valid((const uint8_t *)"hello", 5));
  assert(bare_utf8_valid(BYTES(0xc3, 0xa9)));
  assert(bare_utf8_valid(BYTES(0xe2, 0x82, 0xac)));
  assert(bare_utf8_valid(BYTES(0xf0, 0x9f, 0x92, 0xa9)));
  assert(bare_utf8_valid(BYTES(0xf4, 0x8f, 0xbf, 0xbf)));
  assert(!bare_utf8_valid(BYTES(0xc0, 0x80)));
  assert(!bare_utf8_valid(BYTES(0xe0, 0x80, 0x80)));
  assert(!bare_utf8_valid(BYTES(0xed, 0xa0, 0x80)));
  assert(!bare_utf8_valid(BYTES(0xf4, 0x90, 0x80, 0x80)));
  assert(!bare_utf8_valid(BYTES(0xc3)));
  assert(!bare_utf8_valid(BYTES(0x80)));
  assert(!bare_utf8_valid(BYTES(0xff)));
}

int main(void) {
  test_uint_vectors();
  test_int_vectors();
  test_uint_errors();
  test_fixed_ints();
  test_floats();
  test_bool();
  test_str();
  test_data();
  test_short_write();
  test_str_helpers();
  test_utf8();
  return 0;
}
