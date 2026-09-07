#include "backend/config.h"
#include "backend/names.h"
#include "util/strbuf.h"
#include "util/types.h"

#include <assert.h>
#include <string.h>

static void check(const char *name, CaseStyle style, const char *expect) {
  StrBuf sb = {};
  name_render((Str){.data = name, .len = strlen(name)}, style, &sb);
  assert(sb.data != nullptr);
  assert(strcmp(sb.data, expect) == 0);
  strbuf_free(&sb);
}

static void test_snake(void) {
  check("orderId", CaseStyle_SNAKE, "order_id");
  check("PublicKey", CaseStyle_SNAKE, "public_key");
  check("FOO_BAR", CaseStyle_SNAKE, "foo_bar");
  check("HTTPServer", CaseStyle_SNAKE, "http_server");
  check("hireDate", CaseStyle_SNAKE, "hire_date");
  check("TerminatedEmployee", CaseStyle_SNAKE, "terminated_employee");
  check("u8", CaseStyle_SNAKE, "u8");
  check("B2X", CaseStyle_SNAKE, "b2_x");
  check("already_snake", CaseStyle_SNAKE, "already_snake");
}

static void test_screaming(void) {
  check("orderId", CaseStyle_SCREAMING, "ORDER_ID");
  check("FOO_BAR", CaseStyle_SCREAMING, "FOO_BAR");
  check("PublicKey", CaseStyle_SCREAMING, "PUBLIC_KEY");
}

static void test_pascal(void) {
  check("orderId", CaseStyle_PASCAL, "OrderId");
  check("FOO_BAR", CaseStyle_PASCAL, "FooBar");
  check("public_key", CaseStyle_PASCAL, "PublicKey");
  check("HTTPServer", CaseStyle_PASCAL, "HttpServer");
  check("x", CaseStyle_PASCAL, "X");
  check("u8", CaseStyle_PASCAL, "U8");
}

static void test_camel(void) {
  check("orderId", CaseStyle_CAMEL, "orderId");
  check("FOO_BAR", CaseStyle_CAMEL, "fooBar");
  check("public_key", CaseStyle_CAMEL, "publicKey");
  check("PublicKey", CaseStyle_CAMEL, "publicKey");
}

static void test_strbuf(void) {
  StrBuf sb = {};
  strbuf_append(&sb, "foo");
  strbuf_append_char(&sb, '_');
  strbuf_append_str(&sb, STR("bar"));
  strbuf_appendf(&sb, "_%d_%s", 42, "baz");
  assert(strcmp(sb.data, "foo_bar_42_baz") == 0);
  assert(sb.len == strlen("foo_bar_42_baz"));
  strbuf_free(&sb);
  assert(sb.data == nullptr);
}

int main(void) {
  test_snake();
  test_screaming();
  test_pascal();
  test_camel();
  test_strbuf();
  return 0;
}
