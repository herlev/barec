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
  Packet in = {0};
  in.kind = PacketKind_A;
  in.choice.tag = PacketChoiceTag_U32;
  in.choice.value.u32 = 0xDEADBEEF;
  Packet out = {0};
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

static void fill_full(Packet *p) {
  p->kind = PacketKind_B;
  p->pos.x = 1.5F;
  p->pos.y = -2.5F;
  p->choice.tag = PacketChoiceTag_MEMBER1;
  p->choice.value.member1.a = 7;
  for (uint64_t i = 0; i < 2; i++) {
    for (uint64_t j = 0; j < 3; j++) {
      p->grid.items[i][j] = (uint8_t)((i * 3) + j);
    }
  }
  p->rows.len = 2;
  p->rows.items[0].len = 1;
  p->rows.items[0].items[0] = 0xBEEF;
  p->rows.items[1].len = 3;
  p->rows.items[1].items[0] = 1;
  p->rows.items[1].items[1] = 2;
  p->rows.items[1].items[2] = 3;
  p->tags.has_value = true;
  p->tags.value.len = 2;
  p->tags.value.items[0] = BARE_STR64("alpha");
  p->tags.value.items[1] = BARE_STR64("beta");
  p->fp.has_value = true;
  memset(p->fp.value, 0xAB, sizeof(p->fp.value));
  p->by_mode.len = 2;
  p->by_mode.entries[0].key = Mode_OFF;
  p->by_mode.entries[0].value = 100;
  p->by_mode.entries[1].key = Mode_TURBO;
  p->by_mode.entries[1].value = 300;
  p->by_blob.len = 2;
  memcpy(p->by_blob.entries[0].key.data, "\x01\x02\x03\x04", 4);
  p->by_blob.entries[0].value = BARE_STR64("first");
  memcpy(p->by_blob.entries[1].key.data, "\x09\x09\x09\x09", 4);
  p->by_blob.entries[1].value = BARE_STR64("second");
  p->by_id.len = 1;
  p->by_id.entries[0].key = 42;
  p->by_id.entries[0].value = true;
  for (uint64_t i = 0; i < 2; i++) {
    memset(p->fixed_blobs[i], (int)(0x10 + i), 8);
  }
  p->maybe.has_value = true;
  p->maybe.value = 99;
}

static void test_full_roundtrip(void) {
  Packet in = {0};
  fill_full(&in);
  Packet out = {0};
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
  Packet in = {0};
  in.kind = PacketKind_A;
  in.choice.tag = PacketChoiceTag_DATA;
  in.choice.value.data.len = 3;
  memcpy(in.choice.value.data.data, "\x0a\x0b\x0c", 3);
  Packet out = {0};
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
  Packet in = {0};
  in.kind = PacketKind_A;
  in.choice.tag = PacketChoiceTag_U32;

  in.by_mode.len = 2;
  in.by_mode.entries[0].key = Mode_ON;
  in.by_mode.entries[1].key = Mode_ON;
  uint8_t buf[1024];
  size_t written = 0;
  assert(packet_encode(&in, buf, sizeof(buf), &written) == BareStatus_OK);
  Packet out = {0};
  assert(packet_decode(&out, buf, written) == BareStatus_DUPLICATE_KEY);
  in.by_mode.len = 0;

  in.by_blob.len = 2;
  memcpy(in.by_blob.entries[0].key.data, "same", 4);
  memcpy(in.by_blob.entries[1].key.data, "same", 4);
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

  Wide w = {0};
  w.tag = WideTag_STR;
  w.value.str = BARE_STR64("big");
  assert(wide_encode(&w, buf, sizeof(buf), &written) == BareStatus_OK);
  Wide wout = {0};
  assert(wide_decode(&wout, buf, written) == BareStatus_OK);
  assert(wout.tag == WideTag_STR);
  assert(BARE_STR_EQ(&wout.value.str, "big"));
}

int main(void) {
  test_empty_roundtrip();
  test_full_roundtrip();
  test_union_data_arms();
  test_duplicate_keys();
  test_huge_constants();
  return 0;
}
