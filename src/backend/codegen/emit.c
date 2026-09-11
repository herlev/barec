#include "backend/codegen/internal.h"

#include "backend/config.h"
#include "backend/names.h"
#include "frontend/schema.h"
#include "util/macros.h"
#include "util/strbuf.h"
#include "util/types.h"
#include "util/vec.h"

#include <assert.h>
#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  Str raw;
  u64 value;
  Doc doc;
} EnumEntry;

static const char *const PRIMITIVE_CTYPE[] = {
    [TypeKind_UINT] = "uint64_t", [TypeKind_U8] = "uint8_t",   [TypeKind_U16] = "uint16_t",
    [TypeKind_U32] = "uint32_t",  [TypeKind_U64] = "uint64_t", [TypeKind_INT] = "int64_t",
    [TypeKind_I8] = "int8_t",     [TypeKind_I16] = "int16_t",  [TypeKind_I32] = "int32_t",
    [TypeKind_I64] = "int64_t",   [TypeKind_F32] = "float",    [TypeKind_F64] = "double",
    [TypeKind_BOOL] = "bool",
};

U64Lit codegen_u64_lit(u64 value) {
  U64Lit lit;
  if (value > INT64_MAX) {
    snprintf(lit.text, sizeof(lit.text), "UINT64_C(%" PRIu64 ")", value);
  } else {
    snprintf(lit.text, sizeof(lit.text), "%" PRIu64, value);
  }
  return lit;
}

void codegen_indent(StrBuf *out, int indent) {
  for (int i = 0; i < indent; i++) {
    strbuf_append_char(out, ' ');
  }
}

/// A trailing backslash would splice the next generated line into the
/// comment, so it is stripped along with any whitespace before it.
static Str doc_text(Str line) {
  while (line.len > 0 && (line.data[line.len - 1] == '\\' || line.data[line.len - 1] == ' ' ||
                          line.data[line.len - 1] == '\t')) {
    line.len -= 1;
  }
  return line;
}

static void emit_doc_line(StrBuf *out, Str line, int indent) {
  line = doc_text(line);
  codegen_indent(out, indent);
  if (line.len == 0) {
    strbuf_append(out, "///\n");
  } else {
    strbuf_appendf(out, "/// %.*s\n", (int)line.len, line.data);
  }
}

static void emit_doc_above(StrBuf *out, const Doc *doc, int indent) {
  for (size_t i = 0; i < doc->above.len; i++) {
    emit_doc_line(out, doc->above.ptr[i], indent);
  }
}

static void emit_doc(StrBuf *out, const Doc *doc, int indent) {
  emit_doc_above(out, doc, indent);
  if (doc->trailing.has_value) {
    emit_doc_line(out, doc->trailing.value, indent);
  }
}

/// Appends the trailing comment to the line just emitted, before its
/// newline. Plain // rather than /// so clangd hover attaches it to that
/// line's declaration.
static void emit_trailing_doc(StrBuf *out, const Doc *doc) {
  if (!doc->trailing.has_value) {
    return;
  }
  assert(out->len > 0 && out->data[out->len - 1] == '\n' &&
         "member emission ends with its newline");
  out->len -= 1;
  Str text = doc_text(doc->trailing.value);
  if (text.len == 0) {
    strbuf_append(out, " //\n");
  } else {
    strbuf_appendf(out, " // %.*s\n", (int)text.len, text.data);
  }
}

void codegen_emit_member(const Gen *g, const Type *t, const char *name, const char *dims,
                         int indent) {
  StrBuf *out = g->out;
  switch (t->kind) {
  case TypeKind_STR:
    codegen_indent(out, indent);
    strbuf_appendf(out, "BareStr%" PRIu32 " %s%s;\n", codegen_cap_of(g, t), name, dims);
    break;
  case TypeKind_DATA:
    codegen_indent(out, indent);
    if (t->data.length.has_value) {
      strbuf_appendf(out, "uint8_t %s%s[%" PRIu64 "];\n", name, dims, t->data.length.value);
    } else {
      strbuf_appendf(out, "BareData%" PRIu32 " %s%s;\n", codegen_cap_of(g, t), name, dims);
    }
    break;
  case TypeKind_USER: {
    char *ref = codegen_render_type_name(g->cfg, t->user.name);
    codegen_indent(out, indent);
    strbuf_appendf(out, "%s %s%s;\n", ref, name, dims);
    free(ref);
    break;
  }
  case TypeKind_ENUM:
  case TypeKind_UNION:
    codegen_indent(out, indent);
    strbuf_appendf(out, "%s %s%s;\n", codegen_name_of(g, t), name, dims);
    break;
  case TypeKind_STRUCT:
    codegen_indent(out, indent);
    strbuf_append(out, "struct {\n");
    for (size_t i = 0; i < t->struct_fields.len; i++) {
      const StructField *field = &t->struct_fields.fields[i];
      emit_doc_above(out, &field->doc, indent + 2);
      char *fname = codegen_render_ident(g->cfg, field->name, g->cfg->field_case, false);
      codegen_emit_member(g, field->type, fname, "", indent + 2);
      emit_trailing_doc(out, &field->doc);
      free(fname);
    }
    codegen_indent(out, indent);
    strbuf_appendf(out, "} %s%s;\n", name, dims);
    break;
  case TypeKind_OPTIONAL:
    codegen_indent(out, indent);
    strbuf_append(out, "struct {\n");
    codegen_indent(out, indent + 2);
    strbuf_appendf(out, "bool %s;\n", g->members.has_value);
    codegen_emit_member(g, t->optional.inner, g->members.value, "", indent + 2);
    codegen_indent(out, indent);
    strbuf_appendf(out, "} %s%s;\n", name, dims);
    break;
  case TypeKind_LIST:
    if (t->list.length.has_value) {
      StrBuf nd = {};
      strbuf_appendf(&nd, "%s[%" PRIu64 "]", dims, t->list.length.value);
      codegen_emit_member(g, t->list.elem, name, nd.data, indent);
      strbuf_free(&nd);
    } else {
      StrBuf idims = {};
      strbuf_appendf(&idims, "[%" PRIu32 "]", codegen_cap_of(g, t));
      codegen_indent(out, indent);
      strbuf_append(out, "struct {\n");
      codegen_emit_member(g, t->list.elem, g->members.items, idims.data, indent + 2);
      codegen_indent(out, indent + 2);
      strbuf_appendf(out, "uint32_t %s;\n", g->members.len);
      codegen_indent(out, indent);
      strbuf_appendf(out, "} %s%s;\n", name, dims);
      strbuf_free(&idims);
    }
    break;
  case TypeKind_MAP:
    codegen_indent(out, indent);
    strbuf_append(out, "struct {\n");
    codegen_indent(out, indent + 2);
    strbuf_append(out, "struct {\n");
    codegen_emit_member(g, t->map.key, g->members.key, "", indent + 4);
    codegen_emit_member(g, t->map.value, g->members.value, "", indent + 4);
    codegen_indent(out, indent + 2);
    strbuf_appendf(out, "} %s[%" PRIu32 "];\n", g->members.entries, codegen_cap_of(g, t));
    codegen_indent(out, indent + 2);
    strbuf_appendf(out, "uint32_t %s;\n", g->members.len);
    codegen_indent(out, indent);
    strbuf_appendf(out, "} %s%s;\n", name, dims);
    break;
  case TypeKind_VOID:
    UNREACHABLE();
  default:
    codegen_indent(out, indent);
    strbuf_appendf(out, "%s %s%s;\n", PRIMITIVE_CTYPE[t->kind], name, dims);
    break;
  }
}

static const char *smallest_uint(u64 max) {
  if (max <= UINT8_MAX) {
    return "uint8_t";
  }
  if (max <= UINT16_MAX) {
    return "uint16_t";
  }
  if (max <= UINT32_MAX) {
    return "uint32_t";
  }
  return "uint64_t";
}

char *codegen_render_variant(const Gen *g, const char *type_cname, Str raw) {
  StrBuf out = {};
  Str base = {.data = type_cname, .len = codegen_type_base_len(g->cfg, type_cname)};
  switch (g->cfg->enum_variant_style) {
  case EnumVariantStyle_TYPE_UPPER:
    strbuf_append_str(&out, base);
    strbuf_append_char(&out, '_');
    name_render(raw, CaseStyle_SCREAMING, &out);
    break;
  case EnumVariantStyle_UPPER: {
    StrBuf composed = {};
    if (g->cfg->prefix.len > 0) {
      strbuf_append_str(&composed, g->cfg->prefix);
      strbuf_append_char(&composed, '_');
    }
    strbuf_append_str(&composed, raw);
    name_render((Str){.data = composed.data, .len = composed.len}, CaseStyle_SCREAMING, &out);
    strbuf_free(&composed);
    break;
  }
  case EnumVariantStyle_TYPE_PASCAL:
    strbuf_append_str(&out, base);
    strbuf_append_char(&out, '_');
    name_render(raw, CaseStyle_PASCAL, &out);
    break;
  case EnumVariantStyle_SCREAMING:
    name_render(base, CaseStyle_SCREAMING, &out);
    strbuf_append_char(&out, '_');
    name_render(raw, CaseStyle_SCREAMING, &out);
    break;
  }
  return out.data;
}

static void emit_enum_def(const Gen *g, const char *cname, const EnumEntry entries[], size_t n) {
  StrBuf *out = g->out;
  u64 max = 0;
  for (size_t i = 0; i < n; i++) {
    if (entries[i].value > max) {
      max = entries[i].value;
    }
  }
  const char *base = smallest_uint(max);
  if (g->cfg->std == CStd_C23) {
    strbuf_appendf(out, "typedef enum : %s {\n", base);
    for (size_t i = 0; i < n; i++) {
      emit_doc_above(out, &entries[i].doc, 2);
      char *variant = codegen_render_variant(g, cname, entries[i].raw);
      strbuf_appendf(out, "  %s = %s,\n", variant, codegen_u64_lit(entries[i].value).text);
      emit_trailing_doc(out, &entries[i].doc);
      free(variant);
    }
    strbuf_appendf(out, "} %s;\n\n", cname);
  } else if (max <= INT32_MAX) {
    strbuf_appendf(out, "typedef %s %s;\n", base, cname);
    strbuf_append(out, "enum {\n");
    for (size_t i = 0; i < n; i++) {
      emit_doc_above(out, &entries[i].doc, 2);
      char *variant = codegen_render_variant(g, cname, entries[i].raw);
      strbuf_appendf(out, "  %s = %" PRIu64 ",\n", variant, entries[i].value);
      emit_trailing_doc(out, &entries[i].doc);
      free(variant);
    }
    strbuf_append(out, "};\n\n");
  } else {
    strbuf_appendf(out, "typedef %s %s;\n", base, cname);
    for (size_t i = 0; i < n; i++) {
      emit_doc_above(out, &entries[i].doc, 0);
      char *variant = codegen_render_variant(g, cname, entries[i].raw);
      strbuf_appendf(out, "#define %s UINT64_C(%" PRIu64 ")\n", variant, entries[i].value);
      emit_trailing_doc(out, &entries[i].doc);
      free(variant);
    }
    strbuf_append(out, "\n");
  }
}

static void emit_schema_enum_def(const Gen *g, const Type *t) {
  size_t n = t->enum_values.len;
  assert(n > 0);
  EnumEntry *entries = malloc(n * sizeof(EnumEntry));
  if (entries == nullptr) {
    abort();
  }
  for (size_t i = 0; i < n; i++) {
    assert(t->enum_values.values[i].value.has_value && "check_schema assigns implicit enum values");
    entries[i].raw = t->enum_values.values[i].name;
    entries[i].value = t->enum_values.values[i].value.value;
    entries[i].doc = t->enum_values.values[i].doc;
  }
  emit_enum_def(g, codegen_name_of(g, t), entries, n);
  free(entries);
}

static void emit_struct_def(const Gen *g, const Type *t, const char *cname) {
  StrBuf *out = g->out;
  strbuf_append(out, "typedef struct {\n");
  for (size_t i = 0; i < t->struct_fields.len; i++) {
    const StructField *field = &t->struct_fields.fields[i];
    emit_doc_above(out, &field->doc, 2);
    char *fname = codegen_render_ident(g->cfg, field->name, g->cfg->field_case, false);
    codegen_emit_member(g, field->type, fname, "", 2);
    emit_trailing_doc(out, &field->doc);
    free(fname);
  }
  strbuf_appendf(out, "} %s;\n\n", cname);
}

static void emit_union_def(const Gen *g, const Type *t) {
  StrBuf *out = g->out;
  size_t n = t->union_members.len;
  assert(n > 0);
  const char *tag_cname = codegen_tag_name_of(g, t);
  const VEC(GenName) *bases = codegen_bases_of(g, t);
  EnumEntry *entries = malloc(n * sizeof(EnumEntry));
  if (entries == nullptr) {
    abort();
  }
  for (size_t i = 0; i < n; i++) {
    assert(t->union_members.members[i].tag.has_value && "check_schema assigns implicit union tags");
    entries[i].raw = (Str){.data = bases->ptr[i], .len = strlen(bases->ptr[i])};
    entries[i].value = t->union_members.members[i].tag.value;
    entries[i].doc = (Doc){};
  }
  emit_enum_def(g, tag_cname, entries, n);
  free(entries);
  strbuf_append(out, "typedef struct {\n");
  strbuf_appendf(out, "  %s %s;\n", tag_cname, g->members.tag);
  bool any_value = false;
  for (size_t i = 0; i < n; i++) {
    if (type_underlying(t->union_members.members[i].type)->kind != TypeKind_VOID) {
      any_value = true;
    }
  }
  if (any_value) {
    strbuf_append(out, "  union {\n");
    for (size_t i = 0; i < n; i++) {
      const Type *mt = t->union_members.members[i].type;
      if (type_underlying(mt)->kind == TypeKind_VOID) {
        continue;
      }
      char *arm = codegen_render_ident_cstr(g->cfg, bases->ptr[i], g->cfg->field_case, false);
      codegen_emit_member(g, mt, arm, "", 4);
      free(arm);
    }
    strbuf_appendf(out, "  } %s;\n", g->members.value);
  }
  strbuf_appendf(out, "} %s;\n\n", codegen_name_of(g, t));
}

void codegen_emit_derived_defs(const Gen *g, const Type *t, bool is_root) {
  switch (t->kind) {
  case TypeKind_OPTIONAL:
    codegen_emit_derived_defs(g, t->optional.inner, false);
    break;
  case TypeKind_LIST:
    codegen_emit_derived_defs(g, t->list.elem, false);
    break;
  case TypeKind_MAP:
    codegen_emit_derived_defs(g, t->map.key, false);
    codegen_emit_derived_defs(g, t->map.value, false);
    break;
  case TypeKind_ENUM:
    if (!is_root) {
      emit_schema_enum_def(g, t);
    }
    break;
  case TypeKind_STRUCT:
    for (size_t i = 0; i < t->struct_fields.len; i++) {
      codegen_emit_derived_defs(g, t->struct_fields.fields[i].type, false);
    }
    break;
  case TypeKind_UNION:
    for (size_t i = 0; i < t->union_members.len; i++) {
      codegen_emit_derived_defs(g, t->union_members.members[i].type, false);
    }
    if (!is_root) {
      emit_union_def(g, t);
    }
    break;
  default:
    break;
  }
}

void codegen_emit_root_def(const Gen *g, const UserType *ut, const char *cname) {
  StrBuf *out = g->out;
  emit_doc(out, &ut->doc, 0);
  const Type *t = ut->type;
  switch (t->kind) {
  case TypeKind_USER: {
    char *ref = codegen_render_type_name(g->cfg, t->user.name);
    strbuf_appendf(out, "typedef %s %s;\n\n", ref, cname);
    free(ref);
    break;
  }
  case TypeKind_STR:
    strbuf_appendf(out, "typedef BareStr%" PRIu32 " %s;\n\n", codegen_cap_of(g, t), cname);
    break;
  case TypeKind_DATA:
    if (t->data.length.has_value) {
      strbuf_appendf(out, "typedef struct {\n  uint8_t %s[%" PRIu64 "];\n} %s;\n\n",
                     g->members.data, t->data.length.value, cname);
    } else {
      strbuf_appendf(out, "typedef BareData%" PRIu32 " %s;\n\n", codegen_cap_of(g, t), cname);
    }
    break;
  case TypeKind_ENUM:
    emit_schema_enum_def(g, t);
    break;
  case TypeKind_STRUCT:
    emit_struct_def(g, t, cname);
    break;
  case TypeKind_UNION:
    emit_union_def(g, t);
    break;
  case TypeKind_OPTIONAL:
    strbuf_appendf(out, "typedef struct {\n  bool %s;\n", g->members.has_value);
    codegen_emit_member(g, t->optional.inner, g->members.value, "", 2);
    strbuf_appendf(out, "} %s;\n\n", cname);
    break;
  case TypeKind_LIST:
    strbuf_append(out, "typedef struct {\n");
    if (t->list.length.has_value) {
      StrBuf dims = {};
      strbuf_appendf(&dims, "[%" PRIu64 "]", t->list.length.value);
      codegen_emit_member(g, t->list.elem, g->members.items, dims.data, 2);
      strbuf_free(&dims);
    } else {
      StrBuf dims = {};
      strbuf_appendf(&dims, "[%" PRIu32 "]", codegen_cap_of(g, t));
      codegen_emit_member(g, t->list.elem, g->members.items, dims.data, 2);
      strbuf_free(&dims);
      strbuf_appendf(out, "  uint32_t %s;\n", g->members.len);
    }
    strbuf_appendf(out, "} %s;\n\n", cname);
    break;
  case TypeKind_MAP:
    strbuf_append(out, "typedef struct {\n  struct {\n");
    codegen_emit_member(g, t->map.key, g->members.key, "", 4);
    codegen_emit_member(g, t->map.value, g->members.value, "", 4);
    strbuf_appendf(out, "  } %s[%" PRIu32 "];\n  uint32_t %s;\n", g->members.entries,
                   codegen_cap_of(g, t), g->members.len);
    strbuf_appendf(out, "} %s;\n\n", cname);
    break;
  case TypeKind_VOID:
    UNREACHABLE();
  default:
    strbuf_appendf(out, "typedef %s %s;\n\n", PRIMITIVE_CTYPE[t->kind], cname);
    break;
  }
}

static int cmp_u32(const void *a, const void *b) {
  u32 ua = *(const u32 *)a;
  u32 ub = *(const u32 *)b;
  if (ua < ub) {
    return -1;
  }
  return ua > ub ? 1 : 0;
}

void codegen_emit_shared_typedefs(const Gen *g) {
  StrBuf *out = g->out;
  if (g->str_caps.len > 0) {
    qsort(g->str_caps.ptr, g->str_caps.len, sizeof(u32), cmp_u32);
  }
  if (g->data_caps.len > 0) {
    qsort(g->data_caps.ptr, g->data_caps.len, sizeof(u32), cmp_u32);
  }
  for (size_t i = 0; i < g->str_caps.len; i++) {
    u32 cap = g->str_caps.ptr[i];
    strbuf_appendf(out,
                   "#ifndef BARE_STR%" PRIu32 "_DEFINED\n#define BARE_STR%" PRIu32
                   "_DEFINED\ntypedef struct {\n  char data[%" PRIu32
                   "] BARE_NONSTRING;\n  uint32_t len;\n} BareStr%" PRIu32 ";\n",
                   cap, cap, cap, cap);
    strbuf_appendf(out,
                   "#define BARE_STR%" PRIu32 "(lit) ((BareStr%" PRIu32
                   "){.data = \"\" lit, .len = sizeof(lit) - 1})\n#endif\n\n",
                   cap, cap);
  }
  for (size_t i = 0; i < g->data_caps.len; i++) {
    u32 cap = g->data_caps.ptr[i];
    strbuf_appendf(out,
                   "#ifndef BARE_DATA%" PRIu32 "_DEFINED\n#define BARE_DATA%" PRIu32
                   "_DEFINED\ntypedef struct {\n  uint8_t data[%" PRIu32
                   "];\n  uint32_t len;\n} BareData%" PRIu32 ";\n#endif\n\n",
                   cap, cap, cap, cap);
  }
}
