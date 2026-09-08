#include "backend/codegen.h"
#include "backend/config.h"
#include "frontend/check.h"
#include "frontend/parser.h"
#include "frontend/schema.h"
#include "util/diag.h"
#include "util/strbuf.h"
#include "util/types.h"

#include <assert.h>
#include <string.h>

static const char EXAMPLE_SCHEMA[] = {
#embed "example.bare"
    , '\0'};

static const char GOLDEN_HEADER[] = {
#embed "golden/example_default.h"
    , '\0'};

static const char GOLDEN_SOURCE[] = {
#embed "golden/example_default.c"
    , '\0'};

typedef struct {
  StrBuf header;
  StrBuf source;
} Generated;

static Generated generate_ok(const char *src, const Config *cfg) {
  Schema schema = {};
  Diag diag = {};
  assert(parser_parse(src, strlen(src), &schema, &diag));
  assert(check_schema(&schema, &diag));
  Generated gen = {};
  assert(codegen_generate(&schema, cfg, "example", &gen.header, &gen.source, &diag));
  schema_free(&schema);
  return gen;
}

static Diag generate_fail(const char *src, const Config *cfg) {
  Schema schema = {};
  Diag diag = {};
  assert(parser_parse(src, strlen(src), &schema, &diag));
  assert(check_schema(&schema, &diag));
  Generated gen = {};
  assert(!codegen_generate(&schema, cfg, "example", &gen.header, &gen.source, &diag));
  strbuf_free(&gen.header);
  strbuf_free(&gen.source);
  schema_free(&schema);
  return diag;
}

static void free_generated(Generated *gen) {
  strbuf_free(&gen->header);
  strbuf_free(&gen->source);
}

static void test_golden_default(void) {
  Config cfg = config_default();
  Generated gen = generate_ok(EXAMPLE_SCHEMA, &cfg);
  assert(gen.header.len == strlen(GOLDEN_HEADER));
  assert(memcmp(gen.header.data, GOLDEN_HEADER, gen.header.len) == 0);
  assert(gen.source.len == strlen(GOLDEN_SOURCE));
  assert(memcmp(gen.source.data, GOLDEN_SOURCE, gen.source.len) == 0);
  free_generated(&gen);
}

static void test_function_bodies(void) {
  Config cfg = config_default();
  Generated gen = generate_ok("type E enum {A B = 9}\n"
                              "type O optional<E>\n"
                              "type M map<str><u8>\n"
                              "type U union {E | void}",
                              &cfg);
  assert(strstr(gen.source.data, "case 9:\n    break;\n  default:\n"
                                 "    return BareStatus_INVALID_ENUM;") != nullptr);
  assert(strstr(gen.source.data, "*out = (E)raw;") != nullptr);
  assert(strstr(gen.source.data, "return BareStatus_INVALID_OPTIONAL;") != nullptr);
  assert(strstr(gen.source.data, "return BareStatus_DUPLICATE_KEY;") != nullptr);
  assert(strstr(gen.source.data, "memcmp(out->entries[j0].key.data, out->entries[i0].key.data") !=
         nullptr);
  assert(strstr(gen.source.data, "case UTag_VOID:\n    BARE_TRY(bare_write_uint(w, 1));\n"
                                 "    break;") != nullptr);
  assert(strstr(gen.source.data, "return BareStatus_INVALID_TAG;") != nullptr);
  free_generated(&gen);
}

static void test_c99_mode(void) {
  Config cfg = config_default();
  cfg.std = CStd_C99;
  Generated gen = generate_ok("type Department enum {ACCOUNTING JSMITH = 99}\n"
                              "type Flag bool",
                              &cfg);
  assert(strstr(gen.header.data, "#include <stdbool.h>") != nullptr);
  assert(strstr(gen.header.data, "typedef uint8_t Department;") != nullptr);
  assert(strstr(gen.header.data, "enum {\n  Department_ACCOUNTING = 0,\n"
                                 "  Department_JSMITH = 99,\n};") != nullptr);
  assert(strstr(gen.header.data, "[[nodiscard]]") == nullptr);
  assert(strstr(gen.header.data, "BARE_NODISCARD BareStatus department_read") != nullptr);
  free_generated(&gen);
}

static void test_c99_large_enum(void) {
  Config cfg = config_default();
  cfg.std = CStd_C99;
  Generated gen = generate_ok("type Big enum {A B = 4294967296}", &cfg);
  assert(strstr(gen.header.data, "typedef uint64_t Big;") != nullptr);
  assert(strstr(gen.header.data, "#define Big_A UINT64_C(0)") != nullptr);
  assert(strstr(gen.header.data, "#define Big_B UINT64_C(4294967296)") != nullptr);
  free_generated(&gen);
}

static void test_cap_overrides(void) {
  Config cfg = config_default();
  Diag diag = {};
  const char *conf = "[caps.overrides]\n"
                     "Note.text = 128\n"
                     "Note.blob.item = 16\n";
  assert(config_load_text(&cfg, conf, strlen(conf), &diag));
  Generated gen = generate_ok("type Note struct {\n"
                              "  text: str\n"
                              "  blob: list<data>\n"
                              "}",
                              &cfg);
  assert(strstr(gen.header.data, "BareStr128 text;") != nullptr);
  assert(strstr(gen.header.data, "BareData16 items[8];") != nullptr);
  assert(strstr(gen.header.data, "BareStr128;") != nullptr);
  free_generated(&gen);
  config_free(&cfg);
}

static void test_unused_override(void) {
  Config cfg = config_default();
  Diag diag = {};
  const char *conf = "[caps.overrides]\nBogus.field = 1\n";
  assert(config_load_text(&cfg, conf, strlen(conf), &diag));
  diag = generate_fail("type Foo u8", &cfg);
  assert(strstr(diag.message, "cap override 'Bogus.field' does not match") != nullptr);
  config_free(&cfg);
}

static void test_prefix_and_styles(void) {
  Config cfg = config_default();
  cfg.prefix = STR("acme");
  Generated gen = generate_ok("type Customer struct { name: str }", &cfg);
  assert(strstr(gen.header.data, "} AcmeCustomer;") != nullptr);
  assert(strstr(gen.header.data, "acme_customer_read(BareReader *r, AcmeCustomer *out);") !=
         nullptr);
  free_generated(&gen);
}

static void test_keyword_escape(void) {
  Config cfg = config_default();
  Generated gen = generate_ok("type S struct { if: u8 int: u16 }", &cfg);
  assert(strstr(gen.header.data, "uint8_t if_;") != nullptr);
  assert(strstr(gen.header.data, "uint16_t int_;") != nullptr);
  free_generated(&gen);
}

static void test_union_primitive_members(void) {
  Config cfg = config_default();
  Generated gen = generate_ok("type V union {int | uint = 255 | str}", &cfg);
  assert(strstr(gen.header.data, "VTag_INT = 0,") != nullptr);
  assert(strstr(gen.header.data, "VTag_UINT = 255,") != nullptr);
  assert(strstr(gen.header.data, "VTag_STR = 256,") != nullptr);
  assert(strstr(gen.header.data, "int64_t int_;") != nullptr);
  assert(strstr(gen.header.data, "uint64_t uint;") != nullptr);
  assert(strstr(gen.header.data, "BareStr64 str;") != nullptr);
  free_generated(&gen);
}

static void test_screaming_enum_variants(void) {
  Config cfg = config_default();
  cfg.prefix = STR("acme");
  cfg.enum_variant_style = EnumVariantStyle_SCREAMING;
  Generated gen = generate_ok("type Department enum { ACCOUNTING JSMITH = 99 }\n"
                              "type Person union { Department | str }",
                              &cfg);
  assert(strstr(gen.header.data, "ACME_DEPARTMENT_ACCOUNTING = 0,") != nullptr);
  assert(strstr(gen.header.data, "ACME_DEPARTMENT_JSMITH = 99,") != nullptr);
  assert(strstr(gen.header.data, "ACME_PERSON_TAG_DEPARTMENT = 0,") != nullptr);
  assert(strstr(gen.header.data, "ACME_PERSON_TAG_STR = 1,") != nullptr);
  free_generated(&gen);
}

static void test_type_suffix(void) {
  Config cfg = config_default();
  cfg.type_case = CaseStyle_SNAKE;
  cfg.type_suffix = STR("_t");
  Generated gen = generate_ok("type Level enum { LOW HIGH }\n"
                              "type Customer struct { level: Level name: str }",
                              &cfg);
  assert(strstr(gen.header.data, "} level_t;") != nullptr);
  assert(strstr(gen.header.data, "} customer_t;") != nullptr);
  assert(strstr(gen.header.data, "level_LOW = 0,") != nullptr);
  assert(strstr(gen.header.data, "level_t level;") != nullptr);
  assert(strstr(gen.header.data, "level_read(BareReader *r, level_t *out);") != nullptr);
  assert(strstr(gen.header.data, "customer_decode(customer_t *out") != nullptr);
  assert(strstr(gen.header.data, "customer_t_") == nullptr);
  free_generated(&gen);
}

static void test_name_collision(void) {
  Config cfg = config_default();
  Diag diag = generate_fail("type FooBar u8\n"
                            "type Foo struct { bar: enum { X } }",
                            &cfg);
  assert(strstr(diag.message, "'FooBar' is used more than once") != nullptr);
}

int main(void) {
  test_golden_default();
  test_function_bodies();
  test_c99_mode();
  test_c99_large_enum();
  test_cap_overrides();
  test_unused_override();
  test_prefix_and_styles();
  test_keyword_escape();
  test_union_primitive_members();
  test_screaming_enum_variants();
  test_type_suffix();
  test_name_collision();
  return 0;
}
