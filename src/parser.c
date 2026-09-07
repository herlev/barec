#include "parser.h"

#include "diag.h"
#include "lexer.h"
#include "optional.h"
#include "schema.h"
#include "types.h"
#include "vec.h"

#include <assert.h>
#include <stddef.h>
#include <stdlib.h>

enum { MAX_TYPE_DEPTH = 100 };

typedef struct {
  const Token *tokens;
  size_t pos;
  Diag *diag;
  int depth;
} Parser;

static const TypeKind PRIMITIVE_KIND[] = {
    [TokenKind_KW_UINT] = TypeKind_UINT, [TokenKind_KW_U8] = TypeKind_U8,
    [TokenKind_KW_U16] = TypeKind_U16,   [TokenKind_KW_U32] = TypeKind_U32,
    [TokenKind_KW_U64] = TypeKind_U64,   [TokenKind_KW_INT] = TypeKind_INT,
    [TokenKind_KW_I8] = TypeKind_I8,     [TokenKind_KW_I16] = TypeKind_I16,
    [TokenKind_KW_I32] = TypeKind_I32,   [TokenKind_KW_I64] = TypeKind_I64,
    [TokenKind_KW_F32] = TypeKind_F32,   [TokenKind_KW_F64] = TypeKind_F64,
    [TokenKind_KW_BOOL] = TypeKind_BOOL, [TokenKind_KW_STR] = TypeKind_STR,
    [TokenKind_KW_VOID] = TypeKind_VOID,
};

static const Token *peek(const Parser *p) { return &p->tokens[p->pos]; }

static void advance(Parser *p) {
  assert(p->tokens[p->pos].kind != TokenKind_EOF);
  p->pos += 1;
}

static void err_unexpected(Parser *p, const char *expected) {
  const Token *tok = peek(p);
  if (tok->kind == TokenKind_EOF) {
    diag_set(p->diag, tok->loc, "expected %s, found end of input", expected);
  } else {
    diag_set(p->diag, tok->loc, "expected %s, found '%.*s'", expected, (int)tok->text.len,
             tok->text.data);
  }
}

[[nodiscard]] static bool expect(Parser *p, TokenKind kind, const char *what) {
  if (peek(p)->kind != kind) {
    err_unexpected(p, what);
    return false;
  }
  advance(p);
  return true;
}

static bool is_keyword(TokenKind kind) { return kind < TokenKind_IDENT; }

static bool is_upper_char(char c) { return (bool)(c >= 'A' && c <= 'Z'); }

static bool is_lower_char(char c) { return (bool)(c >= 'a' && c <= 'z'); }

static bool is_alpha_char(char c) { return (bool)(is_upper_char(c) || is_lower_char(c)); }

static bool is_digit_char(char c) { return (bool)(c >= '0' && c <= '9'); }

static bool is_type_name(Str s) {
  if (s.len == 0 || !is_upper_char(s.data[0])) {
    return false;
  }
  for (size_t i = 1; i < s.len; i++) {
    if (!is_alpha_char(s.data[i]) && !is_digit_char(s.data[i])) {
      return false;
    }
  }
  return true;
}

static bool is_enum_value_name(Str s) {
  if (s.len == 0 || !is_upper_char(s.data[0])) {
    return false;
  }
  for (size_t i = 1; i < s.len; i++) {
    char c = s.data[i];
    if (!is_upper_char(c) && !is_digit_char(c) && c != '_') {
      return false;
    }
  }
  return true;
}

static bool is_field_name(Str s) {
  if (s.len == 0 || !is_lower_char(s.data[0])) {
    return false;
  }
  for (size_t i = 1; i < s.len; i++) {
    char c = s.data[i];
    if (!is_alpha_char(c) && !is_digit_char(c) && c != '_') {
      return false;
    }
  }
  return true;
}

static Type *type_new(Type type) {
  Type *node = malloc(sizeof(Type));
  if (node == nullptr) {
    abort();
  }
  *node = type;
  return node;
}

[[nodiscard]] static bool parse_any_type(Parser *p, Type **out);

[[nodiscard]] static bool parse_angle_type(Parser *p, Type **out) {
  if (!expect(p, TokenKind_LANGLE, "'<'")) {
    return false;
  }
  Type *inner = nullptr;
  if (!parse_any_type(p, &inner)) {
    return false;
  }
  if (!expect(p, TokenKind_RANGLE, "'>'")) {
    type_free(inner);
    return false;
  }
  *out = inner;
  return true;
}

[[nodiscard]] static bool parse_length(Parser *p, OPTIONAL(u64) * out) {
  *out = (typeof(*out)){};
  if (peek(p)->kind != TokenKind_LBRACKET) {
    return true;
  }
  advance(p);
  const Token *tok = peek(p);
  if (tok->kind != TokenKind_INTEGER) {
    err_unexpected(p, "an integer");
    return false;
  }
  advance(p);
  if (!expect(p, TokenKind_RBRACKET, "']'")) {
    return false;
  }
  out->has_value = true;
  out->value = tok->integer;
  return true;
}

[[nodiscard]] static bool parse_enum(Parser *p, SrcLoc loc, Type **out) {
  if (!expect(p, TokenKind_LBRACE, "'{'")) {
    return false;
  }
  VEC(EnumValue) values = {};
  for (;;) {
    const Token *tok = peek(p);
    if (tok->kind == TokenKind_RBRACE) {
      if (values.len == 0) {
        diag_set(p->diag, tok->loc, "enum must have at least one value");
        goto fail;
      }
      advance(p);
      break;
    }
    if (tok->kind != TokenKind_IDENT || !is_enum_value_name(tok->text)) {
      if (tok->kind == TokenKind_IDENT) {
        diag_set(p->diag, tok->loc,
                 "invalid enum value name '%.*s', expected uppercase letters, digits, and "
                 "underscores starting with a letter",
                 (int)tok->text.len, tok->text.data);
      } else {
        err_unexpected(p, "an enum value name or '}'");
      }
      goto fail;
    }
    EnumValue value = {.name = tok->text, .loc = tok->loc};
    advance(p);
    if (peek(p)->kind == TokenKind_EQUALS) {
      advance(p);
      const Token *num = peek(p);
      if (num->kind != TokenKind_INTEGER) {
        err_unexpected(p, "an integer");
        goto fail;
      }
      advance(p);
      value.value.has_value = true;
      value.value.value = num->integer;
    }
    VEC_PUSH(&values, value);
  }
  Type *node = type_new((Type){.kind = TypeKind_ENUM, .loc = loc});
  node->enum_values.values = values.ptr;
  node->enum_values.len = values.len;
  *out = node;
  return true;

fail:
  free(values.ptr);
  return false;
}

[[nodiscard]] static bool parse_union(Parser *p, SrcLoc loc, Type **out) {
  if (!expect(p, TokenKind_LBRACE, "'{'")) {
    return false;
  }
  VEC(UnionMember) members = {};
  if (peek(p)->kind == TokenKind_PIPE) {
    advance(p);
  }
  if (peek(p)->kind == TokenKind_RBRACE) {
    diag_set(p->diag, peek(p)->loc, "union must have at least one member");
    goto fail;
  }
  for (;;) {
    Type *member_type = nullptr;
    if (!parse_any_type(p, &member_type)) {
      goto fail;
    }
    UnionMember member = {.type = member_type};
    if (peek(p)->kind == TokenKind_EQUALS) {
      advance(p);
      const Token *num = peek(p);
      if (num->kind != TokenKind_INTEGER) {
        err_unexpected(p, "an integer");
        type_free(member_type);
        goto fail;
      }
      advance(p);
      member.tag.has_value = true;
      member.tag.value = num->integer;
    }
    VEC_PUSH(&members, member);
    if (peek(p)->kind == TokenKind_PIPE) {
      advance(p);
      if (peek(p)->kind == TokenKind_RBRACE) {
        advance(p);
        break;
      }
      continue;
    }
    if (peek(p)->kind == TokenKind_RBRACE) {
      advance(p);
      break;
    }
    err_unexpected(p, "'|' or '}'");
    goto fail;
  }
  Type *node = type_new((Type){.kind = TypeKind_UNION, .loc = loc});
  node->union_members.members = members.ptr;
  node->union_members.len = members.len;
  *out = node;
  return true;

fail:
  for (size_t i = 0; i < members.len; i++) {
    type_free(members.ptr[i].type);
  }
  free(members.ptr);
  return false;
}

[[nodiscard]] static bool parse_struct(Parser *p, SrcLoc loc, Type **out) {
  if (!expect(p, TokenKind_LBRACE, "'{'")) {
    return false;
  }
  VEC(StructField) fields = {};
  for (;;) {
    const Token *tok = peek(p);
    if (tok->kind == TokenKind_RBRACE) {
      if (fields.len == 0) {
        diag_set(p->diag, tok->loc, "struct must have at least one field");
        goto fail;
      }
      advance(p);
      break;
    }
    bool valid_name =
        (bool)((tok->kind == TokenKind_IDENT && is_field_name(tok->text)) || is_keyword(tok->kind));
    if (!valid_name) {
      if (tok->kind == TokenKind_IDENT) {
        diag_set(p->diag, tok->loc,
                 "invalid field name '%.*s', field names start with a lowercase letter",
                 (int)tok->text.len, tok->text.data);
      } else {
        err_unexpected(p, "a field name or '}'");
      }
      goto fail;
    }
    StructField field = {.name = tok->text, .loc = tok->loc};
    advance(p);
    if (!expect(p, TokenKind_COLON, "':'")) {
      goto fail;
    }
    if (!parse_any_type(p, &field.type)) {
      goto fail;
    }
    VEC_PUSH(&fields, field);
  }
  Type *node = type_new((Type){.kind = TypeKind_STRUCT, .loc = loc});
  node->struct_fields.fields = fields.ptr;
  node->struct_fields.len = fields.len;
  *out = node;
  return true;

fail:
  for (size_t i = 0; i < fields.len; i++) {
    type_free(fields.ptr[i].type);
  }
  free(fields.ptr);
  return false;
}

[[nodiscard]] static bool parse_any_type_inner(Parser *p, Type **out) {
  const Token *tok = peek(p);
  switch (tok->kind) {
  case TokenKind_KW_UINT:
  case TokenKind_KW_U8:
  case TokenKind_KW_U16:
  case TokenKind_KW_U32:
  case TokenKind_KW_U64:
  case TokenKind_KW_INT:
  case TokenKind_KW_I8:
  case TokenKind_KW_I16:
  case TokenKind_KW_I32:
  case TokenKind_KW_I64:
  case TokenKind_KW_F32:
  case TokenKind_KW_F64:
  case TokenKind_KW_BOOL:
  case TokenKind_KW_STR:
  case TokenKind_KW_VOID:
    advance(p);
    *out = type_new((Type){.kind = PRIMITIVE_KIND[tok->kind], .loc = tok->loc});
    return true;
  case TokenKind_KW_DATA: {
    advance(p);
    Type type = {.kind = TypeKind_DATA, .loc = tok->loc};
    if (!parse_length(p, &type.data.length)) {
      return false;
    }
    *out = type_new(type);
    return true;
  }
  case TokenKind_KW_OPTIONAL: {
    advance(p);
    Type type = {.kind = TypeKind_OPTIONAL, .loc = tok->loc};
    if (!parse_angle_type(p, &type.optional.inner)) {
      return false;
    }
    *out = type_new(type);
    return true;
  }
  case TokenKind_KW_LIST: {
    advance(p);
    Type type = {.kind = TypeKind_LIST, .loc = tok->loc};
    if (!parse_angle_type(p, &type.list.elem)) {
      return false;
    }
    if (!parse_length(p, &type.list.length)) {
      type_free(type.list.elem);
      return false;
    }
    *out = type_new(type);
    return true;
  }
  case TokenKind_KW_MAP: {
    advance(p);
    Type type = {.kind = TypeKind_MAP, .loc = tok->loc};
    if (!parse_angle_type(p, &type.map.key)) {
      return false;
    }
    if (!parse_angle_type(p, &type.map.value)) {
      type_free(type.map.key);
      return false;
    }
    *out = type_new(type);
    return true;
  }
  case TokenKind_KW_ENUM:
    advance(p);
    return parse_enum(p, tok->loc, out);
  case TokenKind_KW_UNION:
    advance(p);
    return parse_union(p, tok->loc, out);
  case TokenKind_KW_STRUCT:
    advance(p);
    return parse_struct(p, tok->loc, out);
  case TokenKind_IDENT:
    if (!is_type_name(tok->text)) {
      diag_set(p->diag, tok->loc,
               "invalid type name '%.*s', type names start with an uppercase letter and contain "
               "only letters and digits",
               (int)tok->text.len, tok->text.data);
      return false;
    }
    advance(p);
    *out = type_new((Type){.kind = TypeKind_USER, .user = {.name = tok->text}, .loc = tok->loc});
    return true;
  default:
    err_unexpected(p, "a type");
    return false;
  }
}

[[nodiscard]] static bool parse_any_type(Parser *p, Type **out) {
  if (p->depth == MAX_TYPE_DEPTH) {
    diag_set(p->diag, peek(p)->loc, "type nesting exceeds %d levels", MAX_TYPE_DEPTH);
    return false;
  }
  p->depth += 1;
  bool ok = parse_any_type_inner(p, out);
  p->depth -= 1;
  return ok;
}

[[nodiscard]] static bool parse_user_type(Parser *p, UserType *out) {
  if (!expect(p, TokenKind_KW_TYPE, "'type'")) {
    return false;
  }
  const Token *tok = peek(p);
  if (tok->kind != TokenKind_IDENT || !is_type_name(tok->text)) {
    if (tok->kind == TokenKind_IDENT) {
      diag_set(p->diag, tok->loc,
               "invalid type name '%.*s', type names start with an uppercase letter and contain "
               "only letters and digits",
               (int)tok->text.len, tok->text.data);
    } else {
      err_unexpected(p, "a type name");
    }
    return false;
  }
  advance(p);
  Type *type = nullptr;
  if (!parse_any_type(p, &type)) {
    return false;
  }
  *out = (UserType){.name = tok->text, .type = type, .loc = tok->loc};
  return true;
}

bool parser_parse(const char *src, size_t len, Schema *out, Diag *diag) {
  TokenList tokens = {};
  if (!lexer_tokenize(src, len, &tokens, diag)) {
    return false;
  }
  Parser p = {.tokens = tokens.tokens, .diag = diag};
  VEC(UserType) types = {};
  if (peek(&p)->kind == TokenKind_EOF) {
    diag_set(diag, peek(&p)->loc, "schema must define at least one type");
    goto fail;
  }
  while (peek(&p)->kind != TokenKind_EOF) {
    UserType user_type;
    if (!parse_user_type(&p, &user_type)) {
      goto fail;
    }
    VEC_PUSH(&types, user_type);
  }
  token_list_free(&tokens);
  *out = (Schema){.types = types.ptr, .len = types.len};
  return true;

fail:
  for (size_t i = 0; i < types.len; i++) {
    type_free(types.ptr[i].type);
  }
  free(types.ptr);
  token_list_free(&tokens);
  return false;
}
