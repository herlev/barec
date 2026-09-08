#include "bare.h"
#include "sink.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

static void roundtrip(const Packet *in, Packet *out) {
  uint8_t buf[1024];
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
  assert(out.kind == PacketKind_A);
  assert(out.choice.tag == PacketChoiceTag_U32);
  assert(out.choice.value.u32 == 0xDEADBEEF);
  assert(out.rows.len == 0);
  assert(!out.tags.has_value);
  assert(!out.fp.has_value);
  assert(out.by_mode.len == 0);
  assert(!out.maybe.has_value);
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

  assert(out.kind == PacketKind_B);
  assert(out.pos.x == 1.5F);
  assert(out.pos.y == -2.5F);
  assert(out.choice.tag == PacketChoiceTag_MEMBER1);
  assert(out.choice.value.member1.a == 7);
  assert(out.grid.items[1][2] == 5);
  assert(out.rows.len == 2);
  assert(out.rows.items[0].items[0] == 0xBEEF);
  assert(out.rows.items[1].len == 3);
  assert(out.tags.has_value);
  assert(BARE_STR_EQ(&out.tags.value.items[1], "beta"));
  assert(out.fp.has_value);
  assert(out.fp.value[15] == 0xAB);
  assert(out.by_mode.len == 2);
  assert(out.by_mode.entries[1].key == Mode_TURBO);
  assert(out.by_mode.entries[1].value == 300);
  assert(out.by_blob.len == 2);
  assert(memcmp(out.by_blob.entries[1].key.data, "\x09\x09\x09\x09", 4) == 0);
  assert(BARE_STR_EQ(&out.by_blob.entries[1].value, "second"));
  assert(out.by_id.entries[0].key == 42);
  assert(out.by_id.entries[0].value);
  assert(out.fixed_blobs[1][7] == 0x11);
  assert(out.maybe.has_value);
  assert(out.maybe.value == 99);
}

static void test_union_data_arms(void) {
  Packet in = {
      .kind = PacketKind_A,
      .choice = {.tag = PacketChoiceTag_DATA, .value.data = {.data = {0x0a, 0x0b, 0x0c}, .len = 3}},
  };
  Packet out = {};
  roundtrip(&in, &out);
  assert(out.choice.tag == PacketChoiceTag_DATA);
  assert(out.choice.value.data.len == 3);
  assert(out.choice.value.data.data[2] == 0x0c);

  in.choice.tag = PacketChoiceTag_DATA3;
  memcpy(in.choice.value.data3, "\x77\x88", 2);
  roundtrip(&in, &out);
  assert(out.choice.tag == PacketChoiceTag_DATA3);
  assert(out.choice.value.data3[1] == 0x88);
}

static void test_duplicate_keys(void) {
  Packet in = {
      .kind = PacketKind_A,
      .choice.tag = PacketChoiceTag_U32,
      .by_mode = {.len = 2, .entries = {{.key = Mode_ON}, {.key = Mode_ON}}},
  };
  uint8_t buf[1024];
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
  assert(out.seq == 300);
  assert(out.delta == -255);
}

static void test_counts_vector(void) {
  Counts in = {.items = {0, 1, 254, 255, 256, 257, 126, 127, 128, 129}};
  uint8_t buf[32];
  size_t written = 0;
  assert(counts_encode(&in, buf, sizeof(buf), &written) == BareStatus_OK);
  const uint8_t expected[] = {0x00, 0x01, 0xfe, 0x01, 0xff, 0x01, 0x80, 0x02,
                              0x81, 0x02, 0x7e, 0x7f, 0x80, 0x01, 0x81, 0x01};
  assert(written == sizeof(expected));
  assert(memcmp(buf, expected, sizeof(expected)) == 0);
  Counts out = {};
  assert(counts_decode(&out, buf, written) == BareStatus_OK);
  assert(out.items[4] == 256);
  assert(out.items[9] == 129);
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
  assert(out.by_key.len == 2);
  assert(memcmp(out.by_key.entries[1].key, "\x02\x02\x02\x02", 4) == 0);
  assert(out.by_key.entries[1].value == 6);

  memcpy(in.by_key.entries[1].key, "\x01\x01\x01\x01", 4);
  uint8_t buf[1024];
  size_t written = 0;
  assert(packet_encode(&in, buf, sizeof(buf), &written) == BareStatus_OK);
  assert(packet_decode(&out, buf, written) == BareStatus_DUPLICATE_KEY);
}

static void test_huge_constants(void) {
  uint8_t buf[32];
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
  test_huge_constants();
  return 0;
}
