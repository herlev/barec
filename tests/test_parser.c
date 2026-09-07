#include "frontend/parser.h"
#include "frontend/schema.h"
#include "util/diag.h"
#include "util/types.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static const char EXAMPLE_SCHEMA[] = {
#embed "example.bare"
    , '\0'};

static Schema parse_ok(const char *src) {
  Schema schema = {};
  Diag diag = {};
  assert(parser_parse(src, strlen(src), &schema, &diag));
  return schema;
}

static Diag parse_fail(const char *src) {
  Schema schema = {};
  Diag diag = {};
  assert(!parser_parse(src, strlen(src), &schema, &diag));
  return diag;
}

static void test_primitives(void) {
  static const struct {
    const char *src;
    TypeKind kind;
  } cases[] = {
      {"type T uint", TypeKind_UINT}, {"type T u8", TypeKind_U8},   {"type T u16", TypeKind_U16},
      {"type T u32", TypeKind_U32},   {"type T u64", TypeKind_U64}, {"type T int", TypeKind_INT},
      {"type T i8", TypeKind_I8},     {"type T i16", TypeKind_I16}, {"type T i32", TypeKind_I32},
      {"type T i64", TypeKind_I64},   {"type T f32", TypeKind_F32}, {"type T f64", TypeKind_F64},
      {"type T bool", TypeKind_BOOL}, {"type T str", TypeKind_STR}, {"type T void", TypeKind_VOID},
  };
  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
    Schema schema = parse_ok(cases[i].src);
    assert(schema.len == 1);
    assert(str_eq(schema.types[0].name, STR("T")));
    assert(schema.types[0].type->kind == cases[i].kind);
    schema_free(&schema);
  }
}

static void test_data(void) {
  Schema schema = parse_ok("type A data type B data[16]");
  assert(schema.len == 2);
  assert(schema.types[0].type->kind == TypeKind_DATA);
  assert(!schema.types[0].type->data.length.has_value);
  assert(schema.types[1].type->kind == TypeKind_DATA);
  assert(schema.types[1].type->data.length.has_value);
  assert(schema.types[1].type->data.length.value == 16);
  schema_free(&schema);
}

static void test_enum(void) {
  Schema schema = parse_ok("type E enum { FOO BAR = 255 BUZZ B2_X }");
  assert(schema.len == 1);
  const Type *type = schema.types[0].type;
  assert(type->kind == TypeKind_ENUM);
  assert(type->enum_values.len == 4);
  assert(str_eq(type->enum_values.values[0].name, STR("FOO")));
  assert(!type->enum_values.values[0].value.has_value);
  assert(str_eq(type->enum_values.values[1].name, STR("BAR")));
  assert(type->enum_values.values[1].value.has_value);
  assert(type->enum_values.values[1].value.value == 255);
  assert(!type->enum_values.values[2].value.has_value);
  assert(str_eq(type->enum_values.values[3].name, STR("B2_X")));
  schema_free(&schema);
}

static void test_optional(void) {
  Schema schema = parse_ok("type O optional<u32>");
  assert(schema.types[0].type->kind == TypeKind_OPTIONAL);
  assert(schema.types[0].type->optional.inner->kind == TypeKind_U32);
  schema_free(&schema);
}

static void test_list(void) {
  Schema schema = parse_ok("type A list<str> type B list<uint>[10]");
  assert(schema.types[0].type->kind == TypeKind_LIST);
  assert(schema.types[0].type->list.elem->kind == TypeKind_STR);
  assert(!schema.types[0].type->list.length.has_value);
  assert(schema.types[1].type->list.length.has_value);
  assert(schema.types[1].type->list.length.value == 10);
  assert(schema.types[1].type->list.elem->kind == TypeKind_UINT);
  schema_free(&schema);
}

static void test_map(void) {
  Schema schema = parse_ok("type M map<u32><str>");
  assert(schema.types[0].type->kind == TypeKind_MAP);
  assert(schema.types[0].type->map.key->kind == TypeKind_U32);
  assert(schema.types[0].type->map.value->kind == TypeKind_STR);
  schema_free(&schema);
}

static void test_union(void) {
  Schema schema = parse_ok("type U union {int | uint = 255 | str}");
  const Type *type = schema.types[0].type;
  assert(type->kind == TypeKind_UNION);
  assert(type->union_members.len == 3);
  assert(type->union_members.members[0].type->kind == TypeKind_INT);
  assert(!type->union_members.members[0].tag.has_value);
  assert(type->union_members.members[1].type->kind == TypeKind_UINT);
  assert(type->union_members.members[1].tag.has_value);
  assert(type->union_members.members[1].tag.value == 255);
  assert(type->union_members.members[2].type->kind == TypeKind_STR);
  schema_free(&schema);
}

static void test_union_pipe_style(void) {
  Schema schema = parse_ok("type U union {\n  | u8\n  | str\n}");
  assert(schema.types[0].type->union_members.len == 2);
  schema_free(&schema);

  schema = parse_ok("type U union {| u8 | str |}");
  assert(schema.types[0].type->union_members.len == 2);
  schema_free(&schema);
}

static void test_struct(void) {
  Schema schema = parse_ok("type S struct { foo: uint bar: int buzz: str }");
  const Type *type = schema.types[0].type;
  assert(type->kind == TypeKind_STRUCT);
  assert(type->struct_fields.len == 3);
  assert(str_eq(type->struct_fields.fields[0].name, STR("foo")));
  assert(type->struct_fields.fields[0].type->kind == TypeKind_UINT);
  assert(str_eq(type->struct_fields.fields[2].name, STR("buzz")));
  assert(type->struct_fields.fields[2].type->kind == TypeKind_STR);
  schema_free(&schema);
}

static void test_keyword_field_names(void) {
  Schema schema = parse_ok("type S struct { type: u8 data: str list: bool }");
  const Type *type = schema.types[0].type;
  assert(type->struct_fields.len == 3);
  assert(str_eq(type->struct_fields.fields[0].name, STR("type")));
  assert(str_eq(type->struct_fields.fields[1].name, STR("data")));
  assert(str_eq(type->struct_fields.fields[2].name, STR("list")));
  schema_free(&schema);
}

static void test_user_type_reference(void) {
  Schema schema = parse_ok("type A u8 type B A");
  assert(schema.types[1].type->kind == TypeKind_USER);
  assert(str_eq(schema.types[1].type->user.name, STR("A")));
  assert(schema.types[1].type->user.resolved == nullptr);
  schema_free(&schema);
}

static void test_example_schema(void) {
  Schema schema = parse_ok(EXAMPLE_SCHEMA);
  assert(schema.len == 8);

  assert(str_eq(schema.types[0].name, STR("PublicKey")));
  assert(schema.types[0].type->kind == TypeKind_DATA);
  assert(schema.types[0].type->data.length.value == 128);

  const Type *customer = schema.types[4].type;
  assert(str_eq(schema.types[4].name, STR("Customer")));
  assert(customer->kind == TypeKind_STRUCT);
  assert(customer->struct_fields.len == 5);
  const Type *orders = customer->struct_fields.fields[3].type;
  assert(orders->kind == TypeKind_LIST);
  assert(orders->list.elem->kind == TypeKind_STRUCT);
  assert(orders->list.elem->struct_fields.len == 2);
  assert(str_eq(orders->list.elem->struct_fields.fields[0].name, STR("orderId")));
  const Type *metadata = customer->struct_fields.fields[4].type;
  assert(metadata->kind == TypeKind_MAP);
  assert(metadata->map.key->kind == TypeKind_STR);
  assert(metadata->map.value->kind == TypeKind_DATA);

  const Type *person = schema.types[7].type;
  assert(person->kind == TypeKind_UNION);
  assert(person->union_members.len == 3);
  assert(str_eq(person->union_members.members[2].type->user.name, STR("TerminatedEmployee")));

  schema_free(&schema);
}

static void test_errors(void) {
  Diag diag = parse_fail("");
  assert(strstr(diag.message, "at least one type") != nullptr);

  diag = parse_fail("type foo u8");
  assert(strstr(diag.message, "invalid type name") != nullptr);

  diag = parse_fail("type Foo_Bar u8");
  assert(strstr(diag.message, "invalid type name") != nullptr);

  diag = parse_fail("type E enum { }");
  assert(strstr(diag.message, "at least one value") != nullptr);

  diag = parse_fail("type E enum { bad }");
  assert(strstr(diag.message, "invalid enum value name") != nullptr);

  diag = parse_fail("type S struct { }");
  assert(strstr(diag.message, "at least one field") != nullptr);

  diag = parse_fail("type S struct { Bad: u8 }");
  assert(strstr(diag.message, "invalid field name") != nullptr);

  diag = parse_fail("type U union { }");
  assert(strstr(diag.message, "at least one member") != nullptr);

  diag = parse_fail("type U union { u8 u16 }");
  assert(strstr(diag.message, "expected '|' or '}'") != nullptr);

  diag = parse_fail("type L list<u8");
  assert(strstr(diag.message, "expected '>'") != nullptr);

  diag = parse_fail("type M map<u8>");
  assert(strstr(diag.message, "expected '<'") != nullptr);

  diag = parse_fail("type Foo u8 }");
  assert(strstr(diag.message, "expected 'type'") != nullptr);

  diag = parse_fail("type Foo");
  assert(strstr(diag.message, "end of input") != nullptr);
}

static void test_error_location(void) {
  Diag diag = parse_fail("type Foo struct {\n  x u8\n}");
  assert(strstr(diag.message, "expected ':'") != nullptr);
  assert(diag.loc.line == 2);
  assert(diag.loc.column == 5);
}

static void test_deep_nesting(void) {
  enum { DEPTH = 200 };
  char src[4096];
  size_t off = (size_t)snprintf(src, sizeof(src), "type A ");
  for (int i = 0; i < DEPTH; i++) {
    off += (size_t)snprintf(src + off, sizeof(src) - off, "optional<");
  }
  off += (size_t)snprintf(src + off, sizeof(src) - off, "u8");
  for (int i = 0; i < DEPTH; i++) {
    off += (size_t)snprintf(src + off, sizeof(src) - off, ">");
  }
  assert(off < sizeof(src));
  Diag diag = parse_fail(src);
  assert(strstr(diag.message, "nesting") != nullptr);
}

int main(void) {
  test_primitives();
  test_data();
  test_enum();
  test_optional();
  test_list();
  test_map();
  test_union();
  test_union_pipe_style();
  test_struct();
  test_keyword_field_names();
  test_user_type_reference();
  test_example_schema();
  test_errors();
  test_error_location();
  test_deep_nesting();
  return 0;
}
