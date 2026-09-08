#include "bare.h"
#include "example.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

static const uint8_t CUSTOMER_MSG[] = {
    0x00, 0x0b, 0x4a, 0x61, 0x6d, 0x65, 0x73, 0x20, 0x53, 0x6d, 0x69, 0x74, 0x68, 0x12, 0x6a,
    0x73, 0x6d, 0x69, 0x74, 0x68, 0x40, 0x65, 0x78, 0x61, 0x6d, 0x70, 0x6c, 0x65, 0x2e, 0x6f,
    0x72, 0x67, 0x0b, 0x31, 0x32, 0x33, 0x20, 0x4d, 0x61, 0x69, 0x6e, 0x20, 0x53, 0x74, 0x0c,
    0x50, 0x68, 0x69, 0x6c, 0x61, 0x64, 0x65, 0x6c, 0x70, 0x68, 0x69, 0x61, 0x02, 0x50, 0x41,
    0x0d, 0x55, 0x6e, 0x69, 0x74, 0x65, 0x64, 0x20, 0x53, 0x74, 0x61, 0x74, 0x65, 0x73, 0x01,
    0xb2, 0x41, 0xde, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x00};

static const uint8_t EMPLOYEE_MSG[] = {
    0x01, 0x0b, 0x54, 0x69, 0x66, 0x66, 0x61, 0x6e, 0x79, 0x20, 0x44, 0x6f, 0x65, 0x12,
    0x74, 0x69, 0x66, 0x66, 0x61, 0x6e, 0x79, 0x64, 0x40, 0x61, 0x63, 0x6d, 0x65, 0x2e,
    0x63, 0x6f, 0x72, 0x70, 0x0b, 0x31, 0x32, 0x33, 0x20, 0x4d, 0x61, 0x69, 0x6e, 0x20,
    0x53, 0x74, 0x0c, 0x50, 0x68, 0x69, 0x6c, 0x61, 0x64, 0x65, 0x6c, 0x70, 0x68, 0x69,
    0x61, 0x02, 0x50, 0x41, 0x0d, 0x55, 0x6e, 0x69, 0x74, 0x65, 0x64, 0x20, 0x53, 0x74,
    0x61, 0x74, 0x65, 0x73, 0x01, 0x14, 0x32, 0x30, 0x32, 0x30, 0x2d, 0x30, 0x36, 0x2d,
    0x32, 0x31, 0x54, 0x32, 0x31, 0x3a, 0x31, 0x38, 0x3a, 0x30, 0x35, 0x5a, 0x00, 0x00};

static bool str_field_eq(const BareStr64 *field, const char *expect) {
  return BARE_STR_EQ(field, expect);
}

static void set_str_field(BareStr64 *field, const char *text) {
  assert(BARE_STR_SET(field, text) == BareStatus_OK);
}

static void reencode_exact(const Person *p, const uint8_t expect[], size_t expect_len) {
  uint8_t buf[512];
  size_t written = 0;
  assert(person_encode(p, buf, sizeof(buf), &written) == BareStatus_OK);
  assert(written == expect_len);
  assert(memcmp(buf, expect, expect_len) == 0);
}

static void test_customer_message(void) {
  Person p = {};
  assert(person_decode(&p, CUSTOMER_MSG, sizeof(CUSTOMER_MSG)) == BareStatus_OK);
  assert(p.tag == PersonTag_CUSTOMER);
  const Customer *c = &p.value.customer;
  assert(str_field_eq(&c->name, "James Smith"));
  assert(str_field_eq(&c->email, "jsmith@example.org"));
  assert(str_field_eq(&c->address.items[0], "123 Main St"));
  assert(str_field_eq(&c->address.items[1], "Philadelphia"));
  assert(str_field_eq(&c->address.items[2], "PA"));
  assert(str_field_eq(&c->address.items[3], "United States"));
  assert(c->orders.len == 1);
  assert(c->orders.items[0].order_id == 4242424242);
  assert(c->orders.items[0].quantity == 5);
  assert(c->metadata.len == 0);
  reencode_exact(&p, CUSTOMER_MSG, sizeof(CUSTOMER_MSG));
}

static void test_employee_message(void) {
  Person p = {};
  assert(person_decode(&p, EMPLOYEE_MSG, sizeof(EMPLOYEE_MSG)) == BareStatus_OK);
  assert(p.tag == PersonTag_EMPLOYEE);
  const Employee *e = &p.value.employee;
  assert(str_field_eq(&e->name, "Tiffany Doe"));
  assert(str_field_eq(&e->email, "tiffanyd@acme.corp"));
  assert(str_field_eq(&e->address.items[1], "Philadelphia"));
  assert(e->department == Department_ADMINISTRATION);
  assert(str_field_eq(&e->hire_date, "2020-06-21T21:18:05Z"));
  assert(!e->public_key.has_value);
  assert(e->metadata.len == 0);
  reencode_exact(&p, EMPLOYEE_MSG, sizeof(EMPLOYEE_MSG));
}

static void test_str_value_macro(void) {
  BareStr64 s = BARE_STR64("testing123");
  assert(str_field_eq(&s, "testing123"));
}

static void test_terminated_employee_message(void) {
  const uint8_t msg[] = {0x02};
  Person p = {};
  assert(person_decode(&p, msg, sizeof(msg)) == BareStatus_OK);
  assert(p.tag == PersonTag_TERMINATED_EMPLOYEE);
  reencode_exact(&p, msg, sizeof(msg));
}

static void test_full_roundtrip(void) {
  Person p = {};
  p.tag = PersonTag_EMPLOYEE;
  Employee *e = &p.value.employee;
  set_str_field(&e->name, "Ada Lovelace");
  set_str_field(&e->email, "ada@example.org");
  set_str_field(&e->address.items[0], "12 Analytical Row");
  set_str_field(&e->address.items[1], "London");
  set_str_field(&e->address.items[2], "LDN");
  set_str_field(&e->address.items[3], "United Kingdom");
  e->department = Department_JSMITH;
  set_str_field(&e->hire_date, "1843-09-01T00:00:00Z");
  e->public_key.has_value = true;
  for (size_t i = 0; i < sizeof(e->public_key.value.data); i++) {
    e->public_key.value.data[i] = (uint8_t)i;
  }
  e->metadata.len = 2;
  set_str_field(&e->metadata.entries[0].key, "badge");
  e->metadata.entries[0].value.len = 3;
  memcpy(e->metadata.entries[0].value.data, "\x01\x02\x03", 3);
  set_str_field(&e->metadata.entries[1].key, "clearance");
  e->metadata.entries[1].value.len = 0;

  uint8_t buf[512];
  size_t written = 0;
  assert(person_encode(&p, buf, sizeof(buf), &written) == BareStatus_OK);

  Person q = {};
  assert(person_decode(&q, buf, written) == BareStatus_OK);
  assert(q.tag == PersonTag_EMPLOYEE);
  const Employee *d = &q.value.employee;
  assert(str_field_eq(&d->name, "Ada Lovelace"));
  assert(d->department == Department_JSMITH);
  assert(d->public_key.has_value);
  assert(memcmp(d->public_key.value.data, e->public_key.value.data, 128) == 0);
  assert(d->metadata.len == 2);
  assert(str_field_eq(&d->metadata.entries[1].key, "clearance"));
  assert(d->metadata.entries[1].value.len == 0);
}

static void test_errors(void) {
  Department dept = Department_ACCOUNTING;
  const uint8_t bad_enum[] = {0x05};
  assert(department_decode(&dept, bad_enum, 1) == BareStatus_INVALID_ENUM);
  const uint8_t jsmith[] = {0x63};
  assert(department_decode(&dept, jsmith, 1) == BareStatus_OK);
  assert(dept == Department_JSMITH);

  Person p = {};
  const uint8_t bad_tag[] = {0x07};
  assert(person_decode(&p, bad_tag, 1) == BareStatus_INVALID_TAG);

  const uint8_t trailing[] = {0x02, 0x00};
  assert(person_decode(&p, trailing, 2) == BareStatus_TRAILING_DATA);

  assert(person_decode(&p, CUSTOMER_MSG, sizeof(CUSTOMER_MSG) - 1) == BareStatus_SHORT_READ);

  Person dup = {};
  dup.tag = PersonTag_CUSTOMER;
  Customer *c = &dup.value.customer;
  set_str_field(&c->name, "x");
  set_str_field(&c->email, "y");
  c->metadata.len = 2;
  set_str_field(&c->metadata.entries[0].key, "same");
  set_str_field(&c->metadata.entries[1].key, "same");
  uint8_t buf[512];
  size_t written = 0;
  assert(person_encode(&dup, buf, sizeof(buf), &written) == BareStatus_OK);
  assert(person_decode(&p, buf, written) == BareStatus_DUPLICATE_KEY);

  c->metadata.len = 99;
  assert(person_encode(&dup, buf, sizeof(buf), &written) == BareStatus_CAP_EXCEEDED);
}

int main(void) {
  test_customer_message();
  test_employee_message();
  test_str_value_macro();
  test_terminated_employee_message();
  test_full_roundtrip();
  test_errors();
  return 0;
}
