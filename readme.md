# barec

A compiler for the [BARE](https://baremessages.org/) schema language
that generates C types and serialization code depending only on a
small allocation-free runtime.

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
#define READING_SCHEMA_HASH UINT64_C(0xa89d53711dc3dca9)

[[nodiscard]] BareStatus reading_read(BareReader *r, Reading *out);
[[nodiscard]] BareStatus reading_write(BareWriter *w, const Reading *value);
[[nodiscard]] BareStatus reading_decode(Reading *out, const uint8_t buf[], size_t len);
[[nodiscard]] BareStatus reading_encode(const Reading *value, uint8_t buf[], size_t cap, size_t *written);
[[nodiscard]] bool reading_equal(const Reading *a, const Reading *b);
[[nodiscard]] BareStatus reading_skip(BareReader *r);
[[nodiscard]] uint64_t reading_size(const Reading *value);
```

Compile both files with the runtime (`--runtime` writes `bare.h` and
`bare.c` next to them) and work with plain values:

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

`reading_equal` compares decoded values structurally, `reading_skip`
walks past one message without touching the caps (for framing
concatenated streams or forwarding), `reading_size` computes the exact
encoded size of a value without encoding it, and enums also get a
`*_name` function returning the variant's name for logging.

`READING_SCHEMA_HASH` is a hash of the declared schema, covering its
names, values, and structure. Peers can exchange it in a handshake to
check that both sides use the same schema version. `barec hash` prints
it for every type in a schema.

`barec check` validates a schema without writing anything. A
`barec.conf` next to the schema is picked up automatically, and flags
(`--help` lists them) override it. See `examples/` for a complete
client/server protocol.

## Memory model

A generated struct is a plain value: it can live on the stack, be
copied freely, and outlive the buffer it was decoded from.

- Decoding variable-sized data like strings, lists, and maps writes
  straight into inline fixed-capacity buffers, sized by the config.
- Wire data that doesn't fit fails decoding with
  `BareStatus_CAP_EXCEEDED`. Pick caps for the largest values you
  expect.
- Every type gets a wire-size constant for sizing buffers: `X_SIZE`
  when the encoding has one exact length, `X_MAX_SIZE` otherwise.

Schema types map to C like this:

| BARE type                | C representation                                                  |
| ------------------------ | ----------------------------------------------------------------- |
| `u8`..`u64`, `i8`..`i64` | `uint8_t`..`uint64_t`, `int8_t`..`int64_t`                        |
| `uint` / `int`           | `uint64_t` / `int64_t`                                            |
| `f32` / `f64`, `bool`    | `float` / `double`, `bool`                                        |
| `str`                    | `struct { char data[N]; uint32_t len; }` (`BareStrN`)             |
| `data`                   | `struct { uint8_t data[N]; uint32_t len; }` (`BareDataN`)         |
| `data[n]`                | `uint8_t data[n]`                                                 |
| `list<T>`                | `struct { T items[N]; uint32_t len; }`                            |
| `list<T>[n]`             | `T items[n]`                                                      |
| `map<K><V>`              | `struct { struct { K key; V value; } entries[N]; uint32_t len; }` |
| `optional<T>`            | `struct { bool has_value; T value; }`                             |
| `enum`                   | `typedef enum : uint8_t { Type_VARIANT, ... }`                    |
| `union`                  | `struct { TypeTag tag; union { ... } value; }`                    |

N is the configured cap. Enums use the smallest underlying type that
fits their values, and C99 output, which has no fixed-type enums,
renders them as a typedef plus constants.

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

## Building

Requires meson, ninja, and a C23 compiler.

```
meson setup build
meson compile -C build
meson install -C build    # installs the barec binary
```
