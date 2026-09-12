#include "backend/codegen/internal.h"

#include "backend/config.h"
#include "backend/names.h"
#include "util/macros.h"
#include "util/strbuf.h"
#include "util/types.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

static bool is_reserved_word(const char *s) {
  static const char *const RESERVED[] = {
      "alignas",  "alignof",   "auto",         "bool",     "break",   "case",    "char",
      "const",    "constexpr", "continue",     "default",  "do",      "double",  "else",
      "enum",     "extern",    "false",        "float",    "for",     "goto",    "i386",
      "if",       "inline",    "int",          "linux",    "long",    "nullptr", "register",
      "restrict", "return",    "short",        "signed",   "sizeof",  "static",  "static_assert",
      "struct",   "switch",    "thread_local", "true",     "typedef", "typeof",  "union",
      "unix",     "unsigned",  "void",         "volatile", "while",
  };
  for (size_t i = 0; i < ARRAY_LEN(RESERVED); i++) {
    if (strcmp(s, RESERVED[i]) == 0) {
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
  if (is_reserved_word(out.data)) {
    strbuf_append_char(&out, '_');
  }
  return out.data;
}

char *codegen_render_ident_cstr(const Config *cfg, const char *raw, CaseStyle style,
                                bool with_prefix) {
  return codegen_render_ident(cfg, (Str){.data = raw, .len = strlen(raw)}, style, with_prefix);
}

char *codegen_render_type_name(const Config *cfg, Str raw) {
  char *name = codegen_render_ident(cfg, raw, cfg->type_case, true);
  if (cfg->type_suffix.len == 0) {
    return name;
  }
  StrBuf out = {};
  strbuf_append(&out, name);
  strbuf_append_str(&out, cfg->type_suffix);
  free(name);
  return out.data;
}

size_t codegen_type_base_len(const Config *cfg, const char *cname) {
  size_t n = strlen(cname);
  size_t s = cfg->type_suffix.len;
  if (s > 0 && n > s && strncmp(cname + n - s, cfg->type_suffix.data, s) == 0) {
    return n - s;
  }
  return n;
}

char *codegen_type_fn_name(const Gen *g, const char *cname, const char *op) {
  StrBuf raw = {};
  strbuf_append_str(&raw, (Str){.data = cname, .len = codegen_type_base_len(g->cfg, cname)});
  strbuf_append_char(&raw, '_');
  strbuf_append(&raw, op);
  char *name = codegen_render_ident(g->cfg, (Str){.data = raw.data, .len = raw.len},
                                    g->cfg->function_case, false);
  strbuf_free(&raw);
  return name;
}
