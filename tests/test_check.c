#include "check.h"
#include "diag.h"
#include "parser.h"
#include "schema.h"

#include <assert.h>
#include <string.h>

static const char EXAMPLE_SCHEMA[] = {
#embed "example.bare"
    , '\0'};

static Schema check_ok(const char *src) {
  Schema schema = {};
  Diag diag = {};
  assert(parser_parse(src, strlen(src), &schema, &diag));
  assert(check_schema(&schema, &diag));
  return schema;
}

static Diag check_fail(const char *src) {
  Schema schema = {};
  Diag diag = {};
  assert(parser_parse(src, strlen(src), &schema, &diag));
  assert(!check_schema(&schema, &diag));
  schema_free(&schema);
  return diag;
}

static void test_resolution(void) {
  Schema schema = check_ok("type A u8 type B A type C optional<B>");
  assert(schema.types[1].type->user.resolved == schema.types[0].type);
  assert(schema.types[2].type->optional.inner->user.resolved == schema.types[1].type);
  schema_free(&schema);
}

static void test_resolution_errors(void) {
  Diag diag = check_fail("type B A");
  assert(strstr(diag.message, "unknown type 'A'") != nullptr);

  diag = check_fail("type B A type A u8");
  assert(strstr(diag.message, "used before its definition") != nullptr);

  diag = check_fail("type A optional<A>");
  assert(strstr(diag.message, "recursive") != nullptr);

  diag = check_fail("type A u8 type A u16");
  assert(strstr(diag.message, "redefinition of type 'A'") != nullptr);
}

static void test_enum_assignment(void) {
  Schema schema = check_ok("type E enum {FOO BAR = 255 BUZZ}");
  const EnumValue *values = schema.types[0].type->enum_values.values;
  assert(values[0].value.has_value && values[0].value.value == 0);
  assert(values[1].value.value == 255);
  assert(values[2].value.has_value && values[2].value.value == 256);
  schema_free(&schema);
}

static void test_enum_errors(void) {
  Diag diag = check_fail("type E enum {A = 5 B = 3}");
  assert(strstr(diag.message, "ascending") != nullptr);

  diag = check_fail("type E enum {A B = 0}");
  assert(strstr(diag.message, "ascending") != nullptr);

  diag = check_fail("type E enum {A A}");
  assert(strstr(diag.message, "duplicate enum value name") != nullptr);

  diag = check_fail("type E enum {A = 18446744073709551615 B}");
  assert(strstr(diag.message, "overflows") != nullptr);
}

static void test_union_tags(void) {
  Schema schema = check_ok("type U union {u8 | str = 5 | int}");
  const UnionMember *members = schema.types[0].type->union_members.members;
  assert(members[0].tag.has_value && members[0].tag.value == 0);
  assert(members[1].tag.value == 5);
  assert(members[2].tag.has_value && members[2].tag.value == 6);
  schema_free(&schema);
}

static void test_union_errors(void) {
  Diag diag = check_fail("type U union {u8 | u8}");
  assert(strstr(diag.message, "duplicate union member type") != nullptr);

  diag = check_fail("type U union {list<u8> | list<u8>}");
  assert(strstr(diag.message, "duplicate union member type") != nullptr);

  diag = check_fail("type U union {u8 = 5 | str = 3}");
  assert(strstr(diag.message, "ascending") != nullptr);
}

static void test_union_distinct_user_types(void) {
  Schema schema = check_ok("type MyU8 u8 type U union {MyU8 | u8}");
  schema_free(&schema);
}

static void test_void_restrictions(void) {
  Diag diag = check_fail("type V void type S struct {x: V}");
  assert(strstr(diag.message, "void is not allowed as a struct field") != nullptr);

  diag = check_fail("type L list<void>");
  assert(strstr(diag.message, "void is not allowed as a list element") != nullptr);

  diag = check_fail("type O optional<void>");
  assert(strstr(diag.message, "void is not allowed as an optional") != nullptr);

  diag = check_fail("type M map<u8><void>");
  assert(strstr(diag.message, "void is not allowed as a map value") != nullptr);

  diag = check_fail("type M map<void><u8>");
  assert(strstr(diag.message, "void is not allowed as a map key") != nullptr);

  diag = check_fail("type V void type W V type S struct {x: W}");
  assert(strstr(diag.message, "void is not allowed as a struct field") != nullptr);

  Schema schema = check_ok("type V void type U union {V | u8}");
  schema_free(&schema);
}

static void test_map_key_restrictions(void) {
  Diag diag = check_fail("type M map<f32><u8>");
  assert(strstr(diag.message, "floating-point") != nullptr);

  diag = check_fail("type T f64 type M map<T><u8>");
  assert(strstr(diag.message, "floating-point") != nullptr);

  diag = check_fail("type M map<list<u8>><u8>");
  assert(strstr(diag.message, "primitive") != nullptr);

  diag = check_fail("type M map<struct {a: u8}><u8>");
  assert(strstr(diag.message, "primitive") != nullptr);

  Schema schema = check_ok("type E enum {A} type M map<E><str> type N map<str><u8>");
  schema_free(&schema);
}

static void test_fixed_lengths(void) {
  Diag diag = check_fail("type D data[0]");
  assert(strstr(diag.message, "at least one") != nullptr);

  diag = check_fail("type L list<u8>[0]");
  assert(strstr(diag.message, "at least one") != nullptr);
}

static void test_struct_duplicate_field(void) {
  Diag diag = check_fail("type S struct {a: u8 a: u16}");
  assert(strstr(diag.message, "duplicate field name 'a'") != nullptr);
}

static void test_nested_validation(void) {
  Diag diag = check_fail("type V void type C list<struct {x: V}>");
  assert(strstr(diag.message, "void is not allowed as a struct field") != nullptr);
}

static void test_example_schema(void) {
  Schema schema = check_ok(EXAMPLE_SCHEMA);

  const Type *department = schema.types[2].type;
  assert(department->enum_values.values[3].value.value == 3);
  assert(department->enum_values.values[4].value.value == 99);

  const Type *employee = schema.types[5].type;
  const Type *public_key = employee->struct_fields.fields[5].type->optional.inner;
  assert(public_key->user.resolved == schema.types[0].type);

  const Type *person = schema.types[7].type;
  assert(person->union_members.members[0].tag.value == 0);
  assert(person->union_members.members[2].tag.value == 2);

  schema_free(&schema);
}

int main(void) {
  test_resolution();
  test_resolution_errors();
  test_enum_assignment();
  test_enum_errors();
  test_union_tags();
  test_union_errors();
  test_union_distinct_user_types();
  test_void_restrictions();
  test_map_key_restrictions();
  test_fixed_lengths();
  test_struct_duplicate_field();
  test_nested_validation();
  test_example_schema();
  return 0;
}
