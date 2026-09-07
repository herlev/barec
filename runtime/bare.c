#include "bare.h"

BareReader bare_reader_new(const uint8_t data[], size_t len) {
  return (BareReader){.data = data, .len = len, .pos = 0};
}

BareWriter bare_writer_new(uint8_t data[], size_t cap) {
  return (BareWriter){.data = data, .cap = cap, .len = 0};
}

size_t bare_reader_remaining(const BareReader *r) { return r->len - r->pos; }
