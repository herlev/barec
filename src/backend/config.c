#include "backend/config.h"

#include "util/diag.h"
#include "util/types.h"
#include "util/vec.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum : u8 {
  Section_NONE,
  Section_NAMING,
  Section_CAPS,
  Section_CAP_OVERRIDES,
} Section;

typedef struct {
  Config cfg;
  VEC(CapOverride) overrides;
  Section section;
  Diag *diag;
  u32 line_no;
  const char *line_start;
} Loader;

static SrcLoc loc_of(const Loader *ld, const char *at) {
  return (SrcLoc){.line = ld->line_no, .column = (u32)(at - ld->line_start) + 1};
}

static bool is_ws(char c) { return (bool)(c == ' ' || c == '\t'); }

static Str trim(Str s) {
  while (s.len > 0 && is_ws(s.data[0])) {
    s.data += 1;
    s.len -= 1;
  }
  while (s.len > 0 && is_ws(s.data[s.len - 1])) {
    s.len -= 1;
  }
  return s;
}

static bool is_ident_char(char c) {
  return (bool)((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                c == '_');
}

[[nodiscard]] static bool parse_case(Loader *ld, Str value, CaseStyle *out) {
  if (str_eq(value, STR("snake"))) {
    *out = CaseStyle_SNAKE;
  } else if (str_eq(value, STR("camel"))) {
    *out = CaseStyle_CAMEL;
  } else if (str_eq(value, STR("pascal"))) {
    *out = CaseStyle_PASCAL;
  } else if (str_eq(value, STR("screaming"))) {
    *out = CaseStyle_SCREAMING;
  } else {
    diag_set(ld->diag, loc_of(ld, value.data),
             "invalid case style '%.*s', expected snake, camel, pascal, or screaming",
             (int)value.len, value.data);
    return false;
  }
  return true;
}

[[nodiscard]] static bool parse_enum_variant_style(Loader *ld, Str value, EnumVariantStyle *out) {
  if (str_eq(value, STR("Type_UPPER"))) {
    *out = EnumVariantStyle_TYPE_UPPER;
  } else if (str_eq(value, STR("UPPER"))) {
    *out = EnumVariantStyle_UPPER;
  } else if (str_eq(value, STR("Type_Pascal"))) {
    *out = EnumVariantStyle_TYPE_PASCAL;
  } else {
    diag_set(ld->diag, loc_of(ld, value.data),
             "invalid enum variant style '%.*s', expected Type_UPPER, UPPER, or Type_Pascal",
             (int)value.len, value.data);
    return false;
  }
  return true;
}

[[nodiscard]] static bool parse_prefix(Loader *ld, Str value) {
  for (size_t i = 0; i < value.len; i++) {
    char c = value.data[i];
    bool valid = (bool)(is_ident_char(c) && !(i == 0 && c >= '0' && c <= '9'));
    if (!valid) {
      diag_set(ld->diag, loc_of(ld, value.data),
               "invalid prefix '%.*s', expected letters, digits, and underscores not starting "
               "with a digit",
               (int)value.len, value.data);
      return false;
    }
  }
  ld->cfg.prefix = value;
  return true;
}

[[nodiscard]] static bool parse_cap(Loader *ld, Str value, u32 *out) {
  if (value.len == 0) {
    diag_set(ld->diag, loc_of(ld, value.data), "expected an integer");
    return false;
  }
  u64 acc = 0;
  for (size_t i = 0; i < value.len; i++) {
    char c = value.data[i];
    if (c < '0' || c > '9') {
      diag_set(ld->diag, loc_of(ld, value.data), "invalid integer '%.*s'", (int)value.len,
               value.data);
      return false;
    }
    acc = (acc * 10) + (u64)(c - '0');
    if (acc > UINT32_MAX) {
      diag_set(ld->diag, loc_of(ld, value.data), "cap does not fit in 32 bits");
      return false;
    }
  }
  if (acc == 0) {
    diag_set(ld->diag, loc_of(ld, value.data), "cap must be at least one");
    return false;
  }
  *out = (u32)acc;
  return true;
}

[[nodiscard]] static bool load_section(Loader *ld, Str line) {
  if (line.data[line.len - 1] != ']') {
    diag_set(ld->diag, loc_of(ld, line.data), "expected ']' at end of section header");
    return false;
  }
  Str name = trim((Str){.data = line.data + 1, .len = line.len - 2});
  if (str_eq(name, STR("naming"))) {
    ld->section = Section_NAMING;
  } else if (str_eq(name, STR("caps"))) {
    ld->section = Section_CAPS;
  } else if (str_eq(name, STR("caps.overrides"))) {
    ld->section = Section_CAP_OVERRIDES;
  } else {
    diag_set(ld->diag, loc_of(ld, line.data),
             "unknown section '[%.*s]', expected [naming], [caps], or [caps.overrides]",
             (int)name.len, name.data);
    return false;
  }
  return true;
}

[[nodiscard]] static bool load_naming_key(Loader *ld, Str key, Str value) {
  if (str_eq(key, STR("type_case"))) {
    return parse_case(ld, value, &ld->cfg.type_case);
  }
  if (str_eq(key, STR("field_case"))) {
    return parse_case(ld, value, &ld->cfg.field_case);
  }
  if (str_eq(key, STR("function_case"))) {
    return parse_case(ld, value, &ld->cfg.function_case);
  }
  if (str_eq(key, STR("enum_variant"))) {
    return parse_enum_variant_style(ld, value, &ld->cfg.enum_variant_style);
  }
  if (str_eq(key, STR("prefix"))) {
    return parse_prefix(ld, value);
  }
  diag_set(ld->diag, loc_of(ld, key.data), "unknown key '%.*s' in [naming]", (int)key.len,
           key.data);
  return false;
}

[[nodiscard]] static bool load_caps_key(Loader *ld, Str key, Str value) {
  if (str_eq(key, STR("str"))) {
    return parse_cap(ld, value, &ld->cfg.str_cap);
  }
  if (str_eq(key, STR("data"))) {
    return parse_cap(ld, value, &ld->cfg.data_cap);
  }
  if (str_eq(key, STR("list"))) {
    return parse_cap(ld, value, &ld->cfg.list_cap);
  }
  if (str_eq(key, STR("map"))) {
    return parse_cap(ld, value, &ld->cfg.map_cap);
  }
  diag_set(ld->diag, loc_of(ld, key.data), "unknown key '%.*s' in [caps]", (int)key.len, key.data);
  return false;
}

[[nodiscard]] static bool load_override_key(Loader *ld, Str key, Str value) {
  for (size_t i = 0; i < key.len; i++) {
    if (!is_ident_char(key.data[i]) && key.data[i] != '.') {
      diag_set(ld->diag, loc_of(ld, key.data), "invalid override path '%.*s'", (int)key.len,
               key.data);
      return false;
    }
  }
  for (size_t i = 0; i < ld->overrides.len; i++) {
    if (str_eq(ld->overrides.ptr[i].path, key)) {
      diag_set(ld->diag, loc_of(ld, key.data), "duplicate override for '%.*s'", (int)key.len,
               key.data);
      return false;
    }
  }
  CapOverride override = {.path = key};
  if (!parse_cap(ld, value, &override.cap)) {
    return false;
  }
  VEC_PUSH(&ld->overrides, override);
  return true;
}

[[nodiscard]] static bool load_line(Loader *ld, Str line) {
  for (size_t i = 0; i < line.len; i++) {
    if (line.data[i] == '#') {
      line.len = i;
      break;
    }
  }
  line = trim(line);
  if (line.len == 0) {
    return true;
  }
  if (line.data[0] == '[') {
    return load_section(ld, line);
  }
  size_t eq = 0;
  while (eq < line.len && line.data[eq] != '=') {
    eq += 1;
  }
  if (eq == line.len) {
    diag_set(ld->diag, loc_of(ld, line.data), "expected 'key = value' or a section header");
    return false;
  }
  Str key = trim((Str){.data = line.data, .len = eq});
  Str value = trim((Str){.data = line.data + eq + 1, .len = line.len - eq - 1});
  if (key.len == 0) {
    diag_set(ld->diag, loc_of(ld, line.data), "missing key before '='");
    return false;
  }
  switch (ld->section) {
  case Section_NONE:
    diag_set(ld->diag, loc_of(ld, key.data), "key outside of a section");
    return false;
  case Section_NAMING:
    return load_naming_key(ld, key, value);
  case Section_CAPS:
    return load_caps_key(ld, key, value);
  case Section_CAP_OVERRIDES:
    return load_override_key(ld, key, value);
  }
  return false;
}

bool config_load_text(Config *cfg, const char *text, size_t len, Diag *diag) {
  char *owned = malloc(len + 1);
  if (owned == nullptr) {
    abort();
  }
  memcpy(owned, text, len);
  owned[len] = '\0';
  Loader ld = {.cfg = *cfg, .section = Section_NONE, .diag = diag};
  bool ok = true;
  size_t pos = 0;
  u32 line_no = 1;
  while (ok && pos < len) {
    size_t end = pos;
    while (end < len && owned[end] != '\n') {
      end += 1;
    }
    ld.line_no = line_no;
    ld.line_start = owned + pos;
    ok = load_line(&ld, (Str){.data = owned + pos, .len = end - pos});
    pos = end + 1;
    line_no += 1;
  }
  if (!ok) {
    free(ld.overrides.ptr);
    free(owned);
    return false;
  }
  ld.cfg.overrides = ld.overrides.ptr;
  ld.cfg.overrides_len = ld.overrides.len;
  ld.cfg.source_text = owned;
  *cfg = ld.cfg;
  return true;
}

bool config_load(Config *cfg, const char *path, Diag *diag) {
  FILE *file = fopen(path, "rb");
  if (file == nullptr) {
    diag_set(diag, (SrcLoc){}, "cannot open config file '%s'", path);
    return false;
  }
  char *text = nullptr;
  size_t len = 0;
  size_t cap = 0;
  bool ok = true;
  for (;;) {
    if (len == cap) {
      cap = cap == 0 ? 4096 : cap * 2;
      char *grown = realloc(text, cap);
      if (grown == nullptr) {
        abort();
      }
      text = grown;
    }
    size_t n = fread(text + len, 1, cap - len, file);
    len += n;
    if (n == 0) {
      if (ferror(file) != 0) {
        diag_set(diag, (SrcLoc){}, "cannot read config file '%s'", path);
        ok = false;
      }
      break;
    }
  }
  (void)fclose(file);
  if (ok) {
    ok = config_load_text(cfg, text, len, diag);
  }
  free(text);
  return ok;
}

Config config_default(void) {
  return (Config){
      .type_case = CaseStyle_PASCAL,
      .field_case = CaseStyle_SNAKE,
      .function_case = CaseStyle_SNAKE,
      .enum_variant_style = EnumVariantStyle_TYPE_UPPER,
      .prefix = {},
      .str_cap = 64,
      .data_cap = 64,
      .list_cap = 8,
      .map_cap = 8,
  };
}

void config_free(Config *cfg) {
  free(cfg->overrides);
  free(cfg->source_text);
  *cfg = (Config){};
}
