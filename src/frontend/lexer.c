#include "frontend/lexer.h"

#include "util/diag.h"
#include "util/macros.h"
#include "util/types.h"

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  const char *src;
  size_t len;
  size_t pos;
  SrcLoc loc;
} Lexer;

typedef struct {
  Token *ptr;
  size_t len;
  size_t cap;
} TokenVec;

static const char *const KEYWORD_NAMES[] = {
    [TokenKind_KW_TYPE] = "type",
    [TokenKind_KW_UINT] = "uint",
    [TokenKind_KW_U8] = "u8",
    [TokenKind_KW_U16] = "u16",
    [TokenKind_KW_U32] = "u32",
    [TokenKind_KW_U64] = "u64",
    [TokenKind_KW_INT] = "int",
    [TokenKind_KW_I8] = "i8",
    [TokenKind_KW_I16] = "i16",
    [TokenKind_KW_I32] = "i32",
    [TokenKind_KW_I64] = "i64",
    [TokenKind_KW_F32] = "f32",
    [TokenKind_KW_F64] = "f64",
    [TokenKind_KW_BOOL] = "bool",
    [TokenKind_KW_STR] = "str",
    [TokenKind_KW_DATA] = "data",
    [TokenKind_KW_VOID] = "void",
    [TokenKind_KW_ENUM] = "enum",
    [TokenKind_KW_OPTIONAL] = "optional",
    [TokenKind_KW_LIST] = "list",
    [TokenKind_KW_MAP] = "map",
    [TokenKind_KW_UNION] = "union",
    [TokenKind_KW_STRUCT] = "struct",
};

static_assert(ARRAY_LEN(KEYWORD_NAMES) == (size_t)TokenKind_IDENT);

static bool at_end(const Lexer *lx) { return lx->pos == lx->len; }

static char peek(const Lexer *lx) { return lx->src[lx->pos]; }

static void advance(Lexer *lx) {
  if (lx->src[lx->pos] == '\n') {
    lx->loc.line += 1;
    lx->loc.column = 1;
  } else {
    lx->loc.column += 1;
  }
  lx->pos += 1;
}

static void skip_ws_and_comments(Lexer *lx) {
  while (!at_end(lx)) {
    char c = peek(lx);
    if (c == ' ' || c == '\t' || c == '\n') {
      advance(lx);
    } else if (c == '#') {
      while (!at_end(lx) && peek(lx) != '\n') {
        advance(lx);
      }
    } else {
      break;
    }
  }
}

static bool is_alpha(char c) { return (bool)((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')); }

static bool is_digit(char c) { return (bool)(c >= '0' && c <= '9'); }

static bool is_ident_char(char c) { return (bool)(is_alpha(c) || is_digit(c) || c == '_'); }

static void vec_push(TokenVec *vec, Token token) {
  if (vec->len == vec->cap) {
    size_t new_cap = vec->cap == 0 ? 64 : vec->cap * 2;
    Token *grown = realloc(vec->ptr, new_cap * sizeof(Token));
    if (grown == nullptr) {
      abort();
    }
    vec->ptr = grown;
    vec->cap = new_cap;
  }
  vec->ptr[vec->len] = token;
  vec->len += 1;
}

static Token lex_ident(Lexer *lx) {
  SrcLoc loc = lx->loc;
  size_t start = lx->pos;
  while (!at_end(lx) && is_ident_char(peek(lx))) {
    advance(lx);
  }
  Str text = {.data = lx->src + start, .len = lx->pos - start};
  TokenKind kind = TokenKind_IDENT;
  for (size_t i = 0; i < ARRAY_LEN(KEYWORD_NAMES); i++) {
    assert(KEYWORD_NAMES[i] != nullptr);
    Str keyword = {.data = KEYWORD_NAMES[i], .len = strlen(KEYWORD_NAMES[i])};
    if (str_eq(text, keyword)) {
      kind = (TokenKind)(TokenKind_KW_TYPE + i);
      break;
    }
  }
  return (Token){.kind = kind, .text = text, .loc = loc};
}

[[nodiscard]] static bool lex_integer(Lexer *lx, Token *out, Diag *diag) {
  SrcLoc loc = lx->loc;
  size_t start = lx->pos;
  u64 value = 0;
  while (!at_end(lx) && is_digit(peek(lx))) {
    u64 digit = (u64)(peek(lx) - '0');
    if (value > (UINT64_MAX - digit) / 10) {
      diag_set(diag, loc, "integer literal does not fit in 64 bits");
      return false;
    }
    value = (value * 10) + digit;
    advance(lx);
  }
  if (!at_end(lx) && is_ident_char(peek(lx))) {
    diag_set(diag, lx->loc, "missing whitespace after integer literal");
    return false;
  }
  *out = (Token){.kind = TokenKind_INTEGER,
                 .text = {.data = lx->src + start, .len = lx->pos - start},
                 .integer = value,
                 .loc = loc};
  return true;
}

bool lexer_tokenize(const char *src, size_t len, TokenList *out, Diag *diag) {
  Lexer lx = {.src = src, .len = len, .loc = {.line = 1, .column = 1}};
  TokenVec vec = {};
  for (;;) {
    skip_ws_and_comments(&lx);
    if (at_end(&lx)) {
      vec_push(&vec, (Token){.kind = TokenKind_EOF, .loc = lx.loc});
      break;
    }
    char c = peek(&lx);
    if (is_alpha(c)) {
      vec_push(&vec, lex_ident(&lx));
      continue;
    }
    if (is_digit(c)) {
      Token token;
      if (!lex_integer(&lx, &token, diag)) {
        goto fail;
      }
      vec_push(&vec, token);
      continue;
    }
    TokenKind kind;
    switch (c) {
    case '{':
      kind = TokenKind_LBRACE;
      break;
    case '}':
      kind = TokenKind_RBRACE;
      break;
    case '<':
      kind = TokenKind_LANGLE;
      break;
    case '>':
      kind = TokenKind_RANGLE;
      break;
    case '[':
      kind = TokenKind_LBRACKET;
      break;
    case ']':
      kind = TokenKind_RBRACKET;
      break;
    case ':':
      kind = TokenKind_COLON;
      break;
    case '|':
      kind = TokenKind_PIPE;
      break;
    case '=':
      kind = TokenKind_EQUALS;
      break;
    case '\r':
      diag_set(diag, lx.loc, "carriage return is not valid whitespace, use LF line endings");
      goto fail;
    default:
      if (c >= ' ' && c < 0x7f) {
        diag_set(diag, lx.loc, "unexpected character '%c'", c);
      } else {
        diag_set(diag, lx.loc, "unexpected byte 0x%02x", (unsigned int)(unsigned char)c);
      }
      goto fail;
    }
    Token token = {.kind = kind, .text = {.data = lx.src + lx.pos, .len = 1}, .loc = lx.loc};
    advance(&lx);
    vec_push(&vec, token);
  }
  *out = (TokenList){.tokens = vec.ptr, .len = vec.len};
  return true;

fail:
  free(vec.ptr);
  return false;
}

void token_list_free(TokenList *list) {
  free(list->tokens);
  *list = (TokenList){};
}
