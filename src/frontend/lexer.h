#pragma once

#include "util/diag.h"
#include "util/types.h"

#include <stddef.h>

typedef enum : u8 {
  TokenKind_KW_TYPE,
  TokenKind_KW_UINT,
  TokenKind_KW_U8,
  TokenKind_KW_U16,
  TokenKind_KW_U32,
  TokenKind_KW_U64,
  TokenKind_KW_INT,
  TokenKind_KW_I8,
  TokenKind_KW_I16,
  TokenKind_KW_I32,
  TokenKind_KW_I64,
  TokenKind_KW_F32,
  TokenKind_KW_F64,
  TokenKind_KW_BOOL,
  TokenKind_KW_STR,
  TokenKind_KW_DATA,
  TokenKind_KW_VOID,
  TokenKind_KW_ENUM,
  TokenKind_KW_OPTIONAL,
  TokenKind_KW_LIST,
  TokenKind_KW_MAP,
  TokenKind_KW_UNION,
  TokenKind_KW_STRUCT,
  TokenKind_IDENT,
  TokenKind_INTEGER,
  TokenKind_LBRACE,
  TokenKind_RBRACE,
  TokenKind_LANGLE,
  TokenKind_RANGLE,
  TokenKind_LBRACKET,
  TokenKind_RBRACKET,
  TokenKind_COLON,
  TokenKind_PIPE,
  TokenKind_EQUALS,
  TokenKind_EOF,
} TokenKind;

/// text views the source buffer. integer is the parsed value for INTEGER
/// tokens. Keywords keep their spelling in text so the parser can accept
/// them where the grammar allows a lowercase name (every keyword is a valid
/// struct-field-name per the ABNF).
typedef struct {
  TokenKind kind;
  Str text;
  u64 integer;
  SrcLoc loc;
} Token;

/// One # comment with the marker and one following space stripped. text
/// views the source buffer. own_line is false for a comment that follows
/// other content on its line.
typedef struct {
  Str text;
  u32 line;
  bool own_line;
} Comment;

typedef struct {
  Token *tokens;
  size_t len;
  Comment *comments;
  size_t comments_len;
} TokenList;

/// Tokenizes the whole input, ending with a TokenKind_EOF token on success.
/// Comments are collected in source order instead of being discarded.
[[nodiscard]] bool lexer_tokenize(const char *src, size_t len, TokenList *out, Diag *diag);
void token_list_free(TokenList *list);
