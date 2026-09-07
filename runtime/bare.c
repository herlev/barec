#include "bare.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
static_assert(sizeof(float) == 4, "float must be IEEE 754 binary32");
static_assert(sizeof(double) == 8, "double must be IEEE 754 binary64");
#endif

enum { UINT_MAX_OCTETS = 10 };

BareReader bare_reader_new(const uint8_t data[], size_t len) {
  return (BareReader){.data = data, .len = len, .pos = 0};
}

BareWriter bare_writer_new(uint8_t data[], size_t cap) {
  return (BareWriter){.data = data, .cap = cap, .len = 0};
}

size_t bare_reader_remaining(const BareReader *r) { return r->len - r->pos; }

BARE_NODISCARD static BareStatus read_bytes(BareReader *r, uint8_t out[], size_t n) {
  if (bare_reader_remaining(r) < n) {
    return BareStatus_SHORT_READ;
  }
  memcpy(out, r->data + r->pos, n);
  r->pos += n;
  return BareStatus_OK;
}

BARE_NODISCARD static BareStatus write_bytes(BareWriter *w, const uint8_t data[], size_t n) {
  if (w->cap - w->len < n) {
    return BareStatus_SHORT_WRITE;
  }
  memcpy(w->data + w->len, data, n);
  w->len += n;
  return BareStatus_OK;
}

BareStatus bare_read_uint(BareReader *r, uint64_t *out) {
  uint64_t value = 0;
  for (size_t i = 0; i < UINT_MAX_OCTETS; i++) {
    if (r->pos == r->len) {
      return BareStatus_SHORT_READ;
    }
    uint8_t octet = r->data[r->pos];
    r->pos += 1;
    if (i == UINT_MAX_OCTETS - 1 && (octet & 0xfe) != 0) {
      return BareStatus_VARINT_TOO_LONG;
    }
    value |= (uint64_t)(octet & 0x7f) << (7 * i);
    if ((octet & 0x80) == 0) {
      if (i > 0 && octet == 0) {
        return BareStatus_OVERLONG_VARINT;
      }
      *out = value;
      return BareStatus_OK;
    }
  }
  return BareStatus_VARINT_TOO_LONG;
}

BareStatus bare_write_uint(BareWriter *w, uint64_t value) {
  uint8_t tmp[UINT_MAX_OCTETS];
  size_t n = 0;
  do {
    uint8_t octet = value & 0x7f;
    value >>= 7;
    if (value != 0) {
      octet |= 0x80;
    }
    tmp[n] = octet;
    n += 1;
  } while (value != 0);
  return write_bytes(w, tmp, n);
}

BareStatus bare_read_int(BareReader *r, int64_t *out) {
  uint64_t raw;
  BareStatus status = bare_read_uint(r, &raw);
  if (status != BareStatus_OK) {
    return status;
  }
  if ((raw & 1) != 0) {
    *out = -(int64_t)(raw >> 1) - 1;
  } else {
    *out = (int64_t)(raw >> 1);
  }
  return BareStatus_OK;
}

BareStatus bare_write_int(BareWriter *w, int64_t value) {
  uint64_t zigzag;
  if (value < 0) {
    zigzag = ((uint64_t)(-(value + 1)) * 2) + 1;
  } else {
    zigzag = (uint64_t)value * 2;
  }
  return bare_write_uint(w, zigzag);
}

BareStatus bare_read_u8(BareReader *r, uint8_t *out) { return read_bytes(r, out, 1); }

BareStatus bare_read_u16(BareReader *r, uint16_t *out) {
  uint8_t b[2];
  BareStatus status = read_bytes(r, b, sizeof(b));
  if (status != BareStatus_OK) {
    return status;
  }
  *out = (uint16_t)((uint16_t)b[0] | ((uint16_t)b[1] << 8));
  return BareStatus_OK;
}

BareStatus bare_read_u32(BareReader *r, uint32_t *out) {
  uint8_t b[4];
  BareStatus status = read_bytes(r, b, sizeof(b));
  if (status != BareStatus_OK) {
    return status;
  }
  *out = (uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
  return BareStatus_OK;
}

BareStatus bare_read_u64(BareReader *r, uint64_t *out) {
  uint8_t b[8];
  BareStatus status = read_bytes(r, b, sizeof(b));
  if (status != BareStatus_OK) {
    return status;
  }
  uint64_t value = 0;
  for (size_t i = 0; i < sizeof(b); i++) {
    value |= (uint64_t)b[i] << (8 * i);
  }
  *out = value;
  return BareStatus_OK;
}

BareStatus bare_write_u8(BareWriter *w, uint8_t value) { return write_bytes(w, &value, 1); }

BareStatus bare_write_u16(BareWriter *w, uint16_t value) {
  uint8_t b[2] = {value & 0xff, (value >> 8) & 0xff};
  return write_bytes(w, b, sizeof(b));
}

BareStatus bare_write_u32(BareWriter *w, uint32_t value) {
  uint8_t b[4] = {value & 0xff, (value >> 8) & 0xff, (value >> 16) & 0xff, (value >> 24) & 0xff};
  return write_bytes(w, b, sizeof(b));
}

BareStatus bare_write_u64(BareWriter *w, uint64_t value) {
  uint8_t b[8];
  for (size_t i = 0; i < sizeof(b); i++) {
    b[i] = (value >> (8 * i)) & 0xff;
  }
  return write_bytes(w, b, sizeof(b));
}

BareStatus bare_read_i8(BareReader *r, int8_t *out) {
  uint8_t u;
  BareStatus status = bare_read_u8(r, &u);
  if (status != BareStatus_OK) {
    return status;
  }
  memcpy(out, &u, sizeof(u));
  return BareStatus_OK;
}

BareStatus bare_read_i16(BareReader *r, int16_t *out) {
  uint16_t u;
  BareStatus status = bare_read_u16(r, &u);
  if (status != BareStatus_OK) {
    return status;
  }
  memcpy(out, &u, sizeof(u));
  return BareStatus_OK;
}

BareStatus bare_read_i32(BareReader *r, int32_t *out) {
  uint32_t u;
  BareStatus status = bare_read_u32(r, &u);
  if (status != BareStatus_OK) {
    return status;
  }
  memcpy(out, &u, sizeof(u));
  return BareStatus_OK;
}

BareStatus bare_read_i64(BareReader *r, int64_t *out) {
  uint64_t u;
  BareStatus status = bare_read_u64(r, &u);
  if (status != BareStatus_OK) {
    return status;
  }
  memcpy(out, &u, sizeof(u));
  return BareStatus_OK;
}

BareStatus bare_write_i8(BareWriter *w, int8_t value) {
  uint8_t u;
  memcpy(&u, &value, sizeof(u));
  return bare_write_u8(w, u);
}

BareStatus bare_write_i16(BareWriter *w, int16_t value) {
  uint16_t u;
  memcpy(&u, &value, sizeof(u));
  return bare_write_u16(w, u);
}

BareStatus bare_write_i32(BareWriter *w, int32_t value) {
  uint32_t u;
  memcpy(&u, &value, sizeof(u));
  return bare_write_u32(w, u);
}

BareStatus bare_write_i64(BareWriter *w, int64_t value) {
  uint64_t u;
  memcpy(&u, &value, sizeof(u));
  return bare_write_u64(w, u);
}

BareStatus bare_read_f32(BareReader *r, float *out) {
  uint32_t bits;
  BareStatus status = bare_read_u32(r, &bits);
  if (status != BareStatus_OK) {
    return status;
  }
  memcpy(out, &bits, sizeof(bits));
  return BareStatus_OK;
}

BareStatus bare_read_f64(BareReader *r, double *out) {
  uint64_t bits;
  BareStatus status = bare_read_u64(r, &bits);
  if (status != BareStatus_OK) {
    return status;
  }
  memcpy(out, &bits, sizeof(bits));
  return BareStatus_OK;
}

BareStatus bare_write_f32(BareWriter *w, float value) {
  uint32_t bits;
  memcpy(&bits, &value, sizeof(bits));
  return bare_write_u32(w, bits);
}

BareStatus bare_write_f64(BareWriter *w, double value) {
  uint64_t bits;
  memcpy(&bits, &value, sizeof(bits));
  return bare_write_u64(w, bits);
}

BareStatus bare_read_bool(BareReader *r, bool *out) {
  uint8_t u;
  BareStatus status = bare_read_u8(r, &u);
  if (status != BareStatus_OK) {
    return status;
  }
  if (u > 1) {
    return BareStatus_INVALID_BOOL;
  }
  *out = u == 1;
  return BareStatus_OK;
}

BareStatus bare_write_bool(BareWriter *w, bool value) { return bare_write_u8(w, (uint8_t)value); }

BARE_NODISCARD static BareStatus read_prefixed(BareReader *r, uint8_t buf[], uint32_t cap,
                                               uint32_t *len, bool validate_utf8) {
  uint64_t n;
  BareStatus status = bare_read_uint(r, &n);
  if (status != BareStatus_OK) {
    return status;
  }
  if (n > cap) {
    return BareStatus_CAP_EXCEEDED;
  }
  if (bare_reader_remaining(r) < n) {
    return BareStatus_SHORT_READ;
  }
  if (validate_utf8 && !bare_utf8_valid(r->data + r->pos, (size_t)n)) {
    return BareStatus_INVALID_UTF8;
  }
  memcpy(buf, r->data + r->pos, (size_t)n);
  r->pos += (size_t)n;
  *len = (uint32_t)n;
  return BareStatus_OK;
}

BareStatus bare_read_str(BareReader *r, char buf[], uint32_t cap, uint32_t *len) {
  return read_prefixed(r, (uint8_t *)buf, cap, len, true);
}

BareStatus bare_read_data(BareReader *r, uint8_t buf[], uint32_t cap, uint32_t *len) {
  return read_prefixed(r, buf, cap, len, false);
}

BareStatus bare_read_data_fixed(BareReader *r, uint8_t buf[], size_t len) {
  return read_bytes(r, buf, len);
}

BareStatus bare_write_str(BareWriter *w, const char buf[], uint32_t len) {
  if (!bare_utf8_valid((const uint8_t *)buf, len)) {
    return BareStatus_INVALID_UTF8;
  }
  BareStatus status = bare_write_uint(w, len);
  if (status != BareStatus_OK) {
    return status;
  }
  return write_bytes(w, (const uint8_t *)buf, len);
}

BareStatus bare_write_data(BareWriter *w, const uint8_t buf[], uint32_t len) {
  BareStatus status = bare_write_uint(w, len);
  if (status != BareStatus_OK) {
    return status;
  }
  return write_bytes(w, buf, len);
}

BareStatus bare_write_data_fixed(BareWriter *w, const uint8_t buf[], size_t len) {
  return write_bytes(w, buf, len);
}

bool bare_utf8_valid(const uint8_t data[], size_t len) {
  size_t i = 0;
  while (i < len) {
    uint8_t lead = data[i];
    if (lead < 0x80) {
      i += 1;
      continue;
    }
    size_t continuations;
    uint32_t codepoint;
    uint32_t min;
    if ((lead & 0xe0) == 0xc0) {
      continuations = 1;
      codepoint = lead & 0x1f;
      min = 0x80;
    } else if ((lead & 0xf0) == 0xe0) {
      continuations = 2;
      codepoint = lead & 0x0f;
      min = 0x800;
    } else if ((lead & 0xf8) == 0xf0) {
      continuations = 3;
      codepoint = lead & 0x07;
      min = 0x10000;
    } else {
      return false;
    }
    if (len - i - 1 < continuations) {
      return false;
    }
    for (size_t j = 1; j <= continuations; j++) {
      uint8_t byte = data[i + j];
      if ((byte & 0xc0) != 0x80) {
        return false;
      }
      codepoint = (codepoint << 6) | (byte & 0x3f);
    }
    if (codepoint < min || codepoint > 0x10FFFF || (codepoint >= 0xD800 && codepoint <= 0xDFFF)) {
      return false;
    }
    i += continuations + 1;
  }
  return true;
}
