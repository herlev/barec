#include "bare.h"
#include "macros.h"

#include <assert.h>
#include <stdint.h>

int main(void) {
  uint8_t buf[4] = {};
  BareReader reader = bare_reader_new(buf, ARRAY_LEN(buf));
  assert(bare_reader_remaining(&reader) == 4);
  BareWriter writer = bare_writer_new(buf, ARRAY_LEN(buf));
  assert(writer.len == 0);
  assert(writer.cap == 4);
  return 0;
}
