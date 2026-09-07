#include "diag.h"
#include "lexer.h"
#include "macros.h"
#include "types.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

static TokenList lex_ok(const char *src) {
  TokenList list = {};
  Diag diag = {};
  assert(lexer_tokenize(src, strlen(src), &list, &diag));
  return list;
}

static Diag lex_fail(const char *src) {
  TokenList list = {};
  Diag diag = {};
  assert(!lexer_tokenize(src, strlen(src), &list, &diag));
  return diag;
}

static void test_empty(void) {
  TokenList list = lex_ok("");
  assert(list.len == 1);
  assert(list.tokens[0].kind == TokenKind_EOF);
  token_list_free(&list);
}

static void test_ws_and_comments(void) {
  TokenList list = lex_ok(" \t\n# comment <>{} 123\n# comment at eof");
  assert(list.len == 1);
  assert(list.tokens[0].kind == TokenKind_EOF);
  assert(list.tokens[0].loc.line == 3);
  token_list_free(&list);
}

static void test_keywords(void) {
  TokenList list = lex_ok("type uint u8 u16 u32 u64 int i8 i16 i32 i64 "
                          "f32 f64 bool str data void enum optional list map union struct");
  size_t keyword_count = TokenKind_IDENT - TokenKind_KW_TYPE;
  assert(list.len == keyword_count + 1);
  for (size_t i = 0; i < keyword_count; i++) {
    assert(list.tokens[i].kind == (TokenKind)(TokenKind_KW_TYPE + i));
  }
  assert(list.tokens[keyword_count].kind == TokenKind_EOF);
  token_list_free(&list);
}

static void test_identifiers(void) {
  TokenList list = lex_ok("PublicKey FOO_BAR fooBar f32x u8_ orderId");
  assert(list.len == 7);
  for (size_t i = 0; i < 6; i++) {
    assert(list.tokens[i].kind == TokenKind_IDENT);
  }
  assert(str_eq(list.tokens[0].text, STR("PublicKey")));
  assert(str_eq(list.tokens[1].text, STR("FOO_BAR")));
  assert(str_eq(list.tokens[3].text, STR("f32x")));
  assert(str_eq(list.tokens[4].text, STR("u8_")));
  token_list_free(&list);
}

static void test_integers(void) {
  TokenList list = lex_ok("0 127 007 18446744073709551615");
  assert(list.len == 5);
  for (size_t i = 0; i < 4; i++) {
    assert(list.tokens[i].kind == TokenKind_INTEGER);
  }
  assert(list.tokens[0].integer == 0);
  assert(list.tokens[1].integer == 127);
  assert(list.tokens[2].integer == 7);
  assert(list.tokens[3].integer == UINT64_MAX);
  assert(str_eq(list.tokens[3].text, STR("18446744073709551615")));
  token_list_free(&list);
}

static void test_punctuation(void) {
  TokenList list = lex_ok("{}<>[]:|=");
  TokenKind expected[] = {
      TokenKind_LBRACE,   TokenKind_RBRACE, TokenKind_LANGLE, TokenKind_RANGLE, TokenKind_LBRACKET,
      TokenKind_RBRACKET, TokenKind_COLON,  TokenKind_PIPE,   TokenKind_EQUALS, TokenKind_EOF};
  assert(list.len == ARRAY_LEN(expected));
  for (size_t i = 0; i < ARRAY_LEN(expected); i++) {
    assert(list.tokens[i].kind == expected[i]);
  }
  token_list_free(&list);
}

static void test_no_space_separation(void) {
  TokenList list = lex_ok("list<str>[4]");
  TokenKind expected[] = {TokenKind_KW_LIST,  TokenKind_LANGLE,   TokenKind_KW_STR,
                          TokenKind_RANGLE,   TokenKind_LBRACKET, TokenKind_INTEGER,
                          TokenKind_RBRACKET, TokenKind_EOF};
  assert(list.len == ARRAY_LEN(expected));
  for (size_t i = 0; i < ARRAY_LEN(expected); i++) {
    assert(list.tokens[i].kind == expected[i]);
  }
  assert(list.tokens[5].integer == 4);
  token_list_free(&list);
}

static void test_integer_ident_boundary(void) {
  TokenList list = lex_ok("1abc");
  assert(list.len == 3);
  assert(list.tokens[0].kind == TokenKind_INTEGER);
  assert(list.tokens[0].integer == 1);
  assert(list.tokens[1].kind == TokenKind_IDENT);
  assert(str_eq(list.tokens[1].text, STR("abc")));
  token_list_free(&list);
}

static void test_locations(void) {
  TokenList list = lex_ok("type Foo\n  u8 # x\ndata");
  assert(list.len == 5);
  assert(list.tokens[0].loc.line == 1 && list.tokens[0].loc.column == 1);
  assert(list.tokens[1].loc.line == 1 && list.tokens[1].loc.column == 6);
  assert(list.tokens[2].loc.line == 2 && list.tokens[2].loc.column == 3);
  assert(list.tokens[3].loc.line == 3 && list.tokens[3].loc.column == 1);
  assert(list.tokens[4].kind == TokenKind_EOF);
  assert(list.tokens[4].loc.line == 3 && list.tokens[4].loc.column == 5);
  token_list_free(&list);
}

static void test_errors(void) {
  Diag diag = lex_fail("$");
  assert(diag.loc.line == 1 && diag.loc.column == 1);

  diag = lex_fail("type Foo\n u8 @");
  assert(diag.loc.line == 2 && diag.loc.column == 5);

  diag = lex_fail("18446744073709551616");
  assert(diag.loc.line == 1 && diag.loc.column == 1);

  diag = lex_fail("a\r\nb");
  assert(diag.loc.line == 1 && diag.loc.column == 2);
}

static const char EXAMPLE_SCHEMA[] = {
#embed "example.bare"
    , '\0'};

static void test_example_schema(void) {
  TokenList list = lex_ok(EXAMPLE_SCHEMA);
  assert(list.len > 50);
  assert(list.tokens[0].kind == TokenKind_KW_TYPE);
  assert(list.tokens[list.len - 1].kind == TokenKind_EOF);
  token_list_free(&list);
}

int main(void) {
  test_empty();
  test_ws_and_comments();
  test_keywords();
  test_identifiers();
  test_integers();
  test_punctuation();
  test_no_space_separation();
  test_integer_ident_boundary();
  test_locations();
  test_errors();
  test_example_schema();
  return 0;
}
