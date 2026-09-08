#pragma once

/// ASCII character classes shared by the lexer, parser, config parser,
/// and name renderer. The schema language and config format are ASCII.
static inline bool ascii_is_upper(char c) { return (bool)(c >= 'A' && c <= 'Z'); }

static inline bool ascii_is_lower(char c) { return (bool)(c >= 'a' && c <= 'z'); }

static inline bool ascii_is_alpha(char c) { return (bool)(ascii_is_upper(c) || ascii_is_lower(c)); }

static inline bool ascii_is_digit(char c) { return (bool)(c >= '0' && c <= '9'); }

static inline bool ascii_is_ident(char c) {
  return (bool)(ascii_is_alpha(c) || ascii_is_digit(c) || c == '_');
}

static inline char ascii_to_lower(char c) {
  if (ascii_is_upper(c)) {
    return (char)(c + ('a' - 'A'));
  }
  return c;
}

static inline char ascii_to_upper(char c) {
  if (ascii_is_lower(c)) {
    return (char)(c - ('a' - 'A'));
  }
  return c;
}
