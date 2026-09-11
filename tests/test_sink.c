#include "bare.h"
#include "sink.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

static_assert(BLOB_SIZE == 4);
static_assert(COUNTS_MAX_SIZE == 100);

static void roundtrip(const Packet *in, Packet *out) {
  uint8_t buf[PACKET_MAX_SIZE];
  size_t written = 0;
  assert(packet_encode(in, buf, sizeof(buf), &written) == BareStatus_OK);
  assert(packet_decode(out, buf, written) == BareStatus_OK);
}

static void test_empty_roundtrip(void) {
  Packet in = {
      .kind = PacketKind_A,
      .choice = {.tag = PacketChoiceTag_U32, .value.u32 = 0xDEADBEEF},
  };
  Packet out = {};
  roundtrip(&in, &out);
  assert(packet_equal(&in, &out));
}

static void test_full_roundtrip(void) {
  Packet in = {
      .kind = PacketKind_B,
      .pos = {.x = 1.5F, .y = -2.5F},
      .choice = {.tag = PacketChoiceTag_MEMBER1, .value.member1.a = 7},
      .grid.items = {{0, 1, 2}, {3, 4, 5}},
      .rows = {.len = 2, .items = {{.len = 1, .items = {0xBEEF}}, {.len = 3, .items = {1, 2, 3}}}},
      .tags = {.has_value = true,
               .value = {.len = 2, .items = {BARE_STR64("alpha"), BARE_STR64("beta")}}},
      .fp = {.has_value = true,
             .value = {0xAB, 0xAB, 0xAB, 0xAB, 0xAB, 0xAB, 0xAB, 0xAB, 0xAB, 0xAB, 0xAB, 0xAB, 0xAB,
                       0xAB, 0xAB, 0xAB}},
      .by_mode = {.len = 2,
                  .entries = {{.key = Mode_OFF, .value = 100}, {.key = Mode_TURBO, .value = 300}}},
      .by_blob = {.len = 2,
                  .entries = {{.key.data = {0x01, 0x02, 0x03, 0x04}, .value = BARE_STR64("first")},
                              {.key.data = {0x09, 0x09, 0x09, 0x09},
                               .value = BARE_STR64("second")}}},
      .by_id = {.len = 1, .entries = {{.key = 42, .value = true}}},
      .fixed_blobs = {{0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10},
                      {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11}},
      .maybe = {.has_value = true, .value = 99},
  };
  Packet out = {};
  roundtrip(&in, &out);
  assert(packet_equal(&in, &out));
}

static void test_union_data_arms(void) {
  Packet in = {
      .kind = PacketKind_A,
      .choice = {.tag = PacketChoiceTag_DATA, .value.data = {.data = {0x0a, 0x0b, 0x0c}, .len = 3}},
  };
  Packet out = {};
  roundtrip(&in, &out);
  assert(packet_equal(&in, &out));

  in.choice.tag = PacketChoiceTag_DATA3;
  memcpy(in.choice.value.data3, "\x77\x88", 2);
  roundtrip(&in, &out);
  assert(packet_equal(&in, &out));
}

static void test_duplicate_keys(void) {
  Packet in = {
      .kind = PacketKind_A,
      .choice.tag = PacketChoiceTag_U32,
      .by_mode = {.len = 2, .entries = {{.key = Mode_ON}, {.key = Mode_ON}}},
  };
  uint8_t buf[PACKET_MAX_SIZE];
  size_t written = 0;
  assert(packet_encode(&in, buf, sizeof(buf), &written) == BareStatus_OK);
  Packet out = {};
  assert(packet_decode(&out, buf, written) == BareStatus_DUPLICATE_KEY);

  Packet in2 = {
      .kind = PacketKind_A,
      .choice.tag = PacketChoiceTag_U32,
      .by_blob = {.len = 2,
                  .entries = {{.key.data = {'s', 'a', 'm', 'e'}},
                              {.key.data = {'s', 'a', 'm', 'e'}}}},
  };
  assert(packet_encode(&in2, buf, sizeof(buf), &written) == BareStatus_OK);
  assert(packet_decode(&out, buf, written) == BareStatus_DUPLICATE_KEY);
}

static void test_varint_fields(void) {
  Packet in = {
      .kind = PacketKind_A,
      .choice.tag = PacketChoiceTag_U32,
      .seq = 300,
      .delta = -255,
  };
  Packet out = {};
  roundtrip(&in, &out);
  assert(packet_equal(&in, &out));
}

static void test_counts_vector(void) {
  Counts in = {.items = {0, 1, 254, 255, 256, 257, 126, 127, 128, 129}};
  uint8_t buf[COUNTS_MAX_SIZE];
  size_t written = 0;
  assert(counts_encode(&in, buf, sizeof(buf), &written) == BareStatus_OK);
  const uint8_t expected[] = {0x00, 0x01, 0xfe, 0x01, 0xff, 0x01, 0x80, 0x02,
                              0x81, 0x02, 0x7e, 0x7f, 0x80, 0x01, 0x81, 0x01};
  assert(written == sizeof(expected));
  assert(memcmp(buf, expected, sizeof(expected)) == 0);
  Counts out = {};
  assert(counts_decode(&out, buf, written) == BareStatus_OK);
  assert(counts_equal(&in, &out));
}

static void test_invalid_optional_marker(void) {
  const uint8_t msg[] = {0x02, 0x07};
  MaybeU8 out = {};
  assert(maybe_u8_decode(&out, msg, sizeof(msg)) == BareStatus_INVALID_OPTIONAL);
}

static void test_decode_cap_exceeded(void) {
  uint8_t list_msg[10] = {0x09};
  Row row = {};
  assert(row_decode(&row, list_msg, sizeof(list_msg)) == BareStatus_CAP_EXCEEDED);

  uint8_t map_msg[19] = {0x09};
  Dict dict = {};
  assert(dict_decode(&dict, map_msg, sizeof(map_msg)) == BareStatus_CAP_EXCEEDED);
}

static void test_anon_data_key_map(void) {
  Packet in = {
      .kind = PacketKind_A,
      .choice.tag = PacketChoiceTag_U32,
      .by_key = {.len = 2,
                 .entries = {{.key = {0x01, 0x01, 0x01, 0x01}, .value = 5},
                             {.key = {0x02, 0x02, 0x02, 0x02}, .value = 6}}},
  };
  Packet out = {};
  roundtrip(&in, &out);
  assert(packet_equal(&in, &out));

  memcpy(in.by_key.entries[1].key, "\x01\x01\x01\x01", 4);
  uint8_t buf[PACKET_MAX_SIZE];
  size_t written = 0;
  assert(packet_encode(&in, buf, sizeof(buf), &written) == BareStatus_OK);
  assert(packet_decode(&out, buf, written) == BareStatus_DUPLICATE_KEY);
}

static void test_equality(void) {
  Packet a = {
      .kind = PacketKind_B,
      .pos = {.x = 1.5F, .y = -2.5F},
      .choice = {.tag = PacketChoiceTag_MEMBER1, .value.member1.a = 7},
      .rows = {.len = 1, .items = {{.len = 2, .items = {1, 2}}}},
      .tags = {.has_value = true, .value = {.len = 1, .items = {BARE_STR64("alpha")}}},
      .by_mode = {.len = 1, .entries = {{.key = Mode_ON, .value = 100}}},
      .maybe = {.has_value = true, .value = 99},
      .seq = 300,
  };
  Packet b = a;
  assert(packet_equal(&a, &b));

  b.seq = 301;
  assert(!packet_equal(&a, &b));
  b = a;
  b.choice.value.member1.a = 8;
  assert(!packet_equal(&a, &b));
  b = a;
  b.choice.tag = PacketChoiceTag_U32;
  assert(!packet_equal(&a, &b));
  b = a;
  b.rows.items[0].items[1] = 3;
  assert(!packet_equal(&a, &b));
  b = a;
  b.tags.value.items[0] = BARE_STR64("alphb");
  assert(!packet_equal(&a, &b));
  b = a;
  b.by_mode.entries[0].value = 101;
  assert(!packet_equal(&a, &b));
  b = a;
  b.maybe.has_value = false;
  assert(!packet_equal(&a, &b));
  b = a;
  b.pos.y = 2.5F;
  assert(!packet_equal(&a, &b));

  Counts c1 = {.items = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9}};
  Counts c2 = c1;
  assert(counts_equal(&c1, &c2));
  c2.items[9] = 10;
  assert(!counts_equal(&c1, &c2));

  Wide w1 = {.tag = WideTag_STR, .value.str = BARE_STR64("big")};
  Wide w2 = w1;
  assert(wide_equal(&w1, &w2));
  w2.value.str = BARE_STR64("bag");
  assert(!wide_equal(&w1, &w2));
}

static void test_skip(void) {
  Packet first = {
      .kind = PacketKind_B,
      .choice = {.tag = PacketChoiceTag_U32, .value.u32 = 1},
      .tags = {.has_value = true, .value = {.len = 1, .items = {BARE_STR64("frame")}}},
      .seq = 7,
  };
  Packet second = first;
  second.seq = 8;
  uint8_t buf[2 * PACKET_MAX_SIZE];
  size_t w1 = 0;
  size_t w2 = 0;
  assert(packet_encode(&first, buf, sizeof(buf), &w1) == BareStatus_OK);
  assert(packet_encode(&second, buf + w1, sizeof(buf) - w1, &w2) == BareStatus_OK);

  BareReader r = bare_reader_new(buf, w1 + w2);
  assert(packet_skip(&r) == BareStatus_OK);
  assert(r.pos == w1);
  Packet out = {};
  assert(packet_read(&r, &out) == BareStatus_OK);
  assert(bare_reader_remaining(&r) == 0);
  assert(packet_equal(&second, &out));

  uint8_t big[21] = {20};
  Row row = {};
  assert(row_decode(&row, big, sizeof(big)) == BareStatus_CAP_EXCEEDED);
  r = bare_reader_new(big, sizeof(big));
  assert(row_skip(&r) == BareStatus_OK);
  assert(bare_reader_remaining(&r) == 0);

  const uint8_t bad_opt[] = {0x02, 0x00};
  r = bare_reader_new(bad_opt, sizeof(bad_opt));
  assert(maybe_u8_skip(&r) == BareStatus_INVALID_OPTIONAL);

  const uint8_t bad_tag[] = {0x05};
  r = bare_reader_new(bad_tag, sizeof(bad_tag));
  assert(wide_skip(&r) == BareStatus_INVALID_TAG);

  const uint8_t trunc[] = {0x03, 0x01};
  r = bare_reader_new(trunc, sizeof(trunc));
  assert(row_skip(&r) == BareStatus_SHORT_READ);
}

static void test_enum_names(void) {
  assert(strcmp(mode_name(Mode_OFF), "OFF") == 0);
  assert(strcmp(mode_name(Mode_TURBO), "TURBO") == 0);
  Mode bogus;
  const uint16_t raw = 7;
  memcpy(&bogus, &raw, sizeof(bogus));
  assert(mode_name(bogus) == NULL);
  assert(strcmp(huge_name(Huge_BIG), "BIG") == 0);
}

static void test_huge_constants(void) {
  uint8_t buf[WIDE_MAX_SIZE];
  size_t written = 0;
  Huge h = Huge_BIG;
  assert(huge_encode(&h, buf, sizeof(buf), &written) == BareStatus_OK);
  Huge hout = Huge_TINY;
  assert(huge_decode(&hout, buf, written) == BareStatus_OK);
  assert(hout == Huge_BIG);

  Wide w = {.tag = WideTag_STR, .value.str = BARE_STR64("big")};
  assert(wide_encode(&w, buf, sizeof(buf), &written) == BareStatus_OK);
  Wide wout = {};
  assert(wide_decode(&wout, buf, written) == BareStatus_OK);
  assert(wout.tag == WideTag_STR);
  assert(BARE_STR_EQ(&wout.value.str, "big"));
}

int main(void) {
  test_empty_roundtrip();
  test_full_roundtrip();
  test_union_data_arms();
  test_duplicate_keys();
  test_varint_fields();
  test_counts_vector();
  test_invalid_optional_marker();
  test_decode_cap_exceeded();
  test_anon_data_key_map();
  test_equality();
  test_skip();
  test_enum_names();
  test_huge_constants();
  return 0;
}
