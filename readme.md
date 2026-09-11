# barec

A compiler for the [BARE](https://baremessages.org/) schema language
that generates C types and serialization code backed by a small
allocation-free runtime.

Given `reading.bare`:

```zig
type Reading struct {
  device: u32
  celsius: f32
  location: optional<str>
}
```

`barec reading.bare` writes `reading.h` and `reading.c`:

```c
typedef struct {
  uint32_t device;
  float celsius;
  struct {
    bool has_value;
    BareStr64 value;
  } location;
} Reading;

#define READING_MAX_SIZE 74

[[nodiscard]] BareStatus reading_read(BareReader *r, Reading *out);
[[nodiscard]] BareStatus reading_write(BareWriter *w, const Reading *value);
[[nodiscard]] BareStatus reading_decode(Reading *out, const uint8_t buf[], size_t len);
[[nodiscard]] BareStatus reading_encode(const Reading *value, uint8_t buf[], size_t cap, size_t *written);
```

Compile them together with the runtime (`--runtime` also writes it to
the output directory) and use plain values:

```c
Reading in = {
    .device = 7,
    .celsius = 21.5F,
    .location = {.has_value = true, .value = BARE_STR64("greenhouse")},
};
uint8_t wire[READING_MAX_SIZE];
size_t n = 0;
BARE_TRY(reading_encode(&in, wire, sizeof(wire), &n));

Reading out = {};
BARE_TRY(reading_decode(&out, wire, n));
if (out.location.has_value) {
  printf("%.1f C at %.*s\n", (double)out.celsius, BARE_STR_ARG(&out.location.value));
}
```

`barec check` validates a schema without generating, a `barec.conf`
next to the schema is picked up automatically, and flags override it
(`--help` lists them). See `examples/` for a complete client/server
protocol.

## Building

Requires meson, ninja, and a C23 compiler.

```
meson setup build
meson compile -C build
meson install -C build    # installs the barec binary
```

## Memory model

A generated struct is a plain value: it can live on the stack, be
copied freely, and outlive the buffer it was decoded from.

- Decoding variable-sized data like strings, lists, and maps writes
  straight into inline fixed-capacity buffers, sized by the config.
- Anything that doesn't fit fails with `BareStatus_CAP_EXCEEDED`, on
  decode and encode alike. Pick caps for the largest values you expect.
- Every type gets a wire-size constant for sizing buffers: `X_SIZE`
  when the encoding has one exact length, `X_MAX_SIZE` otherwise.

`bare.h` ships string-field helpers: `BARE_STR_SET`, `BARE_STR_LIT`,
`BARE_STR_EQ`, and the `BARE_STR64`/`BARE_STR_ARG` seen above.

## Configuration

`barec config` prints the full annotated default config. Highlights:

```ini
[naming]
type_case = pascal        # snake | camel | pascal | screaming
enum_variant = Type_UPPER # Type_UPPER | UPPER | Type_Pascal | TYPE_UPPER
prefix =                  # acme -> AcmeCustomer, acme_customer_decode
type_suffix =             # _t -> customer_t alongside customer_decode

[codegen]
std = c23                 # c99 | c23

[caps]
str = 64                  # octets, data/list/map likewise

[caps.overrides]
Customer.orders = 16      # Type.field, with .item / .key / .value steps
```
