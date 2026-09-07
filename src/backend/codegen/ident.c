#include "backend/codegen/internal.h"

#include "backend/config.h"
#include "backend/names.h"
#include "util/macros.h"
#include "util/strbuf.h"
#include "util/types.h"

#include <stddef.h>
#include <string.h>

static bool is_c_keyword(const char *s) {
  static const char *const KEYWORDS[] = {
      "alignas",      "alignof",  "auto",          "bool",      "break",
      "case",         "char",     "const",         "constexpr", "continue",
      "default",      "do",       "double",        "else",      "enum",
      "extern",       "false",    "float",         "for",       "goto",
      "if",           "inline",   "int",           "long",      "nullptr",
      "register",     "restrict", "return",        "short",     "signed",
      "sizeof",       "static",   "static_assert", "struct",    "switch",
      "thread_local", "true",     "typedef",       "typeof",    "union",
      "unsigned",     "void",     "volatile",      "while",
  };
  for (size_t i = 0; i < ARRAY_LEN(KEYWORDS); i++) {
    if (strcmp(s, KEYWORDS[i]) == 0) {
      return true;
    }
  }
  return false;
}

char *codegen_render_ident(const Config *cfg, Str raw, CaseStyle style, bool with_prefix) {
  StrBuf composed = {};
  if (with_prefix && cfg->prefix.len > 0) {
    strbuf_append_str(&composed, cfg->prefix);
    strbuf_append_char(&composed, '_');
  }
  for (size_t i = 0; i < raw.len; i++) {
    strbuf_append_char(&composed, raw.data[i] == '.' ? '_' : raw.data[i]);
  }
  StrBuf out = {};
  name_render((Str){.data = composed.data, .len = composed.len}, style, &out);
  strbuf_free(&composed);
  if (out.data == nullptr) {
    strbuf_append(&out, "");
  }
  if (is_c_keyword(out.data)) {
    strbuf_append_char(&out, '_');
  }
  return out.data;
}

char *codegen_render_ident_cstr(const Config *cfg, const char *raw, CaseStyle style,
                                bool with_prefix) {
  return codegen_render_ident(cfg, (Str){.data = raw, .len = strlen(raw)}, style, with_prefix);
}

char *codegen_type_fn_name(const Gen *g, const char *cname, const char *op) {
  StrBuf raw = {};
  strbuf_append(&raw, cname);
  strbuf_append_char(&raw, '_');
  strbuf_append(&raw, op);
  char *name = codegen_render_ident(g->cfg, (Str){.data = raw.data, .len = raw.len},
                                    g->cfg->function_case, false);
  strbuf_free(&raw);
  return name;
}
