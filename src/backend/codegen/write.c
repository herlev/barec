#include "backend/codegen/internal.h"

#include "frontend/schema.h"
#include "util/macros.h"
#include "util/strbuf.h"
#include "util/types.h"
#include "util/vec.h"

#include <inttypes.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

static void emit_write_step(Gen *g, const Type *t, const char *expr, int indent, int depth);

static void emit_write_call(Gen *g, const char *cname, const char *expr, int indent) {
  char *fn = codegen_type_fn_name(g, cname, "write");
  codegen_indent(g->out, indent);
  strbuf_appendf(g->out, "BARE_TRY(%s(w, &%s));\n", fn, expr);
  free(fn);
}

static void emit_cap_check(Gen *g, const char *len_expr, u32 cap, int indent) {
  StrBuf *out = g->out;
  codegen_indent(out, indent);
  strbuf_appendf(out, "if (%s > %" PRIu32 ") {\n", len_expr, cap);
  codegen_indent(out, indent + 2);
  strbuf_append(out, "return BareStatus_CAP_EXCEEDED;\n");
  codegen_indent(out, indent);
  strbuf_append(out, "}\n");
}

static void emit_write_optional(Gen *g, const Type *inner, const char *has_expr,
                                const char *value_expr, int indent, int depth) {
  StrBuf *out = g->out;
  codegen_indent(out, indent);
  strbuf_appendf(out, "BARE_TRY(bare_write_u8(w, (uint8_t)%s));\n", has_expr);
  codegen_indent(out, indent);
  strbuf_appendf(out, "if (%s) {\n", has_expr);
  emit_write_step(g, inner, value_expr, indent + 2, depth + 1);
  codegen_indent(out, indent);
  strbuf_append(out, "}\n");
}

static void emit_write_list_fixed(Gen *g, const Type *elem, const char *arr_expr, u64 n, int indent,
                                  int depth) {
  StrBuf *out = g->out;
  codegen_indent(out, indent);
  strbuf_appendf(out, "for (uint64_t i%d = 0; i%d < %" PRIu64 "; i%d++) {\n", depth, depth, n,
                 depth);
  StrBuf elem_expr = {};
  strbuf_appendf(&elem_expr, "%s[i%d]", arr_expr, depth);
  emit_write_step(g, elem, elem_expr.data, indent + 2, depth + 1);
  strbuf_free(&elem_expr);
  codegen_indent(out, indent);
  strbuf_append(out, "}\n");
}

static void emit_write_list_var(Gen *g, const Type *elem, const char *items_expr,
                                const char *len_expr, u32 cap, int indent, int depth) {
  StrBuf *out = g->out;
  emit_cap_check(g, len_expr, cap, indent);
  codegen_indent(out, indent);
  strbuf_appendf(out, "BARE_TRY(bare_write_uint(w, %s));\n", len_expr);
  codegen_indent(out, indent);
  strbuf_appendf(out, "for (uint32_t i%d = 0; i%d < %s; i%d++) {\n", depth, depth, len_expr, depth);
  StrBuf elem_expr = {};
  strbuf_appendf(&elem_expr, "%s[i%d]", items_expr, depth);
  emit_write_step(g, elem, elem_expr.data, indent + 2, depth + 1);
  strbuf_free(&elem_expr);
  codegen_indent(out, indent);
  strbuf_append(out, "}\n");
}

static void emit_write_map(Gen *g, const Type *key, const Type *value, const char *entries_expr,
                           const char *len_expr, u32 cap, int indent, int depth) {
  StrBuf *out = g->out;
  emit_cap_check(g, len_expr, cap, indent);
  codegen_indent(out, indent);
  strbuf_appendf(out, "BARE_TRY(bare_write_uint(w, %s));\n", len_expr);
  codegen_indent(out, indent);
  strbuf_appendf(out, "for (uint32_t i%d = 0; i%d < %s; i%d++) {\n", depth, depth, len_expr, depth);
  StrBuf key_expr = {};
  strbuf_appendf(&key_expr, "%s[i%d].key", entries_expr, depth);
  StrBuf value_expr = {};
  strbuf_appendf(&value_expr, "%s[i%d].value", entries_expr, depth);
  emit_write_step(g, key, key_expr.data, indent + 2, depth + 1);
  emit_write_step(g, value, value_expr.data, indent + 2, depth + 1);
  strbuf_free(&key_expr);
  strbuf_free(&value_expr);
  codegen_indent(out, indent);
  strbuf_append(out, "}\n");
}

static void emit_write_step(Gen *g, const Type *t, const char *expr, int indent, int depth) {
  StrBuf *out = g->out;
  switch (t->kind) {
  case TypeKind_STR: {
    StrBuf len = {};
    strbuf_appendf(&len, "%s.len", expr);
    emit_cap_check(g, len.data, codegen_cap_of(g, t), indent);
    strbuf_free(&len);
    codegen_indent(out, indent);
    strbuf_appendf(out, "BARE_TRY(bare_write_str(w, %s.data, %s.len));\n", expr, expr);
    break;
  }
  case TypeKind_DATA:
    if (t->data.length.has_value) {
      codegen_indent(out, indent);
      strbuf_appendf(out, "BARE_TRY(bare_write_data_fixed(w, %s, %" PRIu64 "));\n", expr,
                     t->data.length.value);
    } else {
      StrBuf len = {};
      strbuf_appendf(&len, "%s.len", expr);
      emit_cap_check(g, len.data, codegen_cap_of(g, t), indent);
      strbuf_free(&len);
      codegen_indent(out, indent);
      strbuf_appendf(out, "BARE_TRY(bare_write_data(w, %s.data, %s.len));\n", expr, expr);
    }
    break;
  case TypeKind_USER: {
    char *cname = codegen_render_ident(g->cfg, t->user.name, g->cfg->type_case, true);
    emit_write_call(g, cname, expr, indent);
    free(cname);
    break;
  }
  case TypeKind_ENUM:
  case TypeKind_STRUCT:
  case TypeKind_UNION:
    emit_write_call(g, codegen_name_of(g, t), expr, indent);
    break;
  case TypeKind_OPTIONAL: {
    StrBuf has = {};
    strbuf_appendf(&has, "%s.has_value", expr);
    StrBuf value = {};
    strbuf_appendf(&value, "%s.value", expr);
    emit_write_optional(g, t->optional.inner, has.data, value.data, indent, depth);
    strbuf_free(&has);
    strbuf_free(&value);
    break;
  }
  case TypeKind_LIST:
    if (t->list.length.has_value) {
      emit_write_list_fixed(g, t->list.elem, expr, t->list.length.value, indent, depth);
    } else {
      StrBuf items = {};
      strbuf_appendf(&items, "%s.items", expr);
      StrBuf len = {};
      strbuf_appendf(&len, "%s.len", expr);
      emit_write_list_var(g, t->list.elem, items.data, len.data, codegen_cap_of(g, t), indent,
                          depth);
      strbuf_free(&items);
      strbuf_free(&len);
    }
    break;
  case TypeKind_MAP: {
    StrBuf entries = {};
    strbuf_appendf(&entries, "%s.entries", expr);
    StrBuf len = {};
    strbuf_appendf(&len, "%s.len", expr);
    emit_write_map(g, t->map.key, t->map.value, entries.data, len.data, codegen_cap_of(g, t),
                   indent, depth);
    strbuf_free(&entries);
    strbuf_free(&len);
    break;
  }
  case TypeKind_VOID:
    UNREACHABLE();
  default:
    codegen_indent(out, indent);
    strbuf_appendf(out, "BARE_TRY(bare_write_%s(w, %s));\n", codegen_bare_type_name(t->kind), expr);
    break;
  }
}

static void emit_write_enum_body(Gen *g, const Type *t) {
  StrBuf *out = g->out;
  strbuf_append(out, "  switch ((uint64_t)*value) {\n");
  for (size_t i = 0; i < t->enum_values.len; i++) {
    strbuf_appendf(out, "  case %" PRIu64 ":\n", t->enum_values.values[i].value.value);
  }
  strbuf_append(out, "    break;\n  default:\n    return BareStatus_INVALID_ENUM;\n  }\n");
  strbuf_append(out, "  return bare_write_uint(w, (uint64_t)*value);\n");
}

static void emit_write_union_body(Gen *g, const Type *t) {
  StrBuf *out = g->out;
  const char *tag_cname = codegen_tag_name_of(g, t);
  const VEC(GenName) *bases = codegen_bases_of(g, t);
  strbuf_append(out, "  switch (value->tag) {\n");
  for (size_t i = 0; i < t->union_members.len; i++) {
    const UnionMember *m = &t->union_members.members[i];
    char *variant = codegen_render_variant(
        g, tag_cname, (Str){.data = bases->ptr[i], .len = strlen(bases->ptr[i])});
    strbuf_appendf(out, "  case %s:\n", variant);
    free(variant);
    codegen_indent(out, 4);
    strbuf_appendf(out, "BARE_TRY(bare_write_uint(w, %" PRIu64 "));\n", m->tag.value);
    if (type_underlying(m->type)->kind != TypeKind_VOID) {
      char *arm = codegen_render_ident_cstr(g->cfg, bases->ptr[i], g->cfg->field_case, false);
      StrBuf expr = {};
      strbuf_appendf(&expr, "value->value.%s", arm);
      emit_write_step(g, m->type, expr.data, 4, 0);
      strbuf_free(&expr);
      free(arm);
    }
    strbuf_append(out, "    break;\n");
  }
  strbuf_append(out, "  default:\n    return BareStatus_INVALID_TAG;\n  }\n"
                     "  return BareStatus_OK;\n");
}

static void emit_write_body(Gen *g, const Type *t) {
  StrBuf *out = g->out;
  switch (t->kind) {
  case TypeKind_USER: {
    char *target = codegen_render_ident(g->cfg, t->user.name, g->cfg->type_case, true);
    char *fn = codegen_type_fn_name(g, target, "write");
    strbuf_appendf(out, "  return %s(w, value);\n", fn);
    free(fn);
    free(target);
    break;
  }
  case TypeKind_STR:
    emit_cap_check(g, "value->len", codegen_cap_of(g, t), 2);
    strbuf_append(out, "  return bare_write_str(w, value->data, value->len);\n");
    break;
  case TypeKind_DATA:
    if (t->data.length.has_value) {
      strbuf_appendf(out, "  return bare_write_data_fixed(w, value->data, %" PRIu64 ");\n",
                     t->data.length.value);
    } else {
      emit_cap_check(g, "value->len", codegen_cap_of(g, t), 2);
      strbuf_append(out, "  return bare_write_data(w, value->data, value->len);\n");
    }
    break;
  case TypeKind_ENUM:
    emit_write_enum_body(g, t);
    break;
  case TypeKind_STRUCT:
    for (size_t i = 0; i < t->struct_fields.len; i++) {
      const StructField *field = &t->struct_fields.fields[i];
      char *fname = codegen_render_ident(g->cfg, field->name, g->cfg->field_case, false);
      StrBuf expr = {};
      strbuf_appendf(&expr, "value->%s", fname);
      emit_write_step(g, field->type, expr.data, 2, 0);
      strbuf_free(&expr);
      free(fname);
    }
    strbuf_append(out, "  return BareStatus_OK;\n");
    break;
  case TypeKind_UNION:
    emit_write_union_body(g, t);
    break;
  case TypeKind_OPTIONAL:
    emit_write_optional(g, t->optional.inner, "value->has_value", "value->value", 2, 0);
    strbuf_append(out, "  return BareStatus_OK;\n");
    break;
  case TypeKind_LIST:
    if (t->list.length.has_value) {
      emit_write_list_fixed(g, t->list.elem, "value->items", t->list.length.value, 2, 0);
    } else {
      emit_write_list_var(g, t->list.elem, "value->items", "value->len", codegen_cap_of(g, t), 2,
                          0);
    }
    strbuf_append(out, "  return BareStatus_OK;\n");
    break;
  case TypeKind_MAP:
    emit_write_map(g, t->map.key, t->map.value, "value->entries", "value->len",
                   codegen_cap_of(g, t), 2, 0);
    strbuf_append(out, "  return BareStatus_OK;\n");
    break;
  case TypeKind_VOID:
    UNREACHABLE();
  default:
    strbuf_appendf(out, "  return bare_write_%s(w, *value);\n", codegen_bare_type_name(t->kind));
    break;
  }
}

void codegen_emit_write_fn(Gen *g, const Type *t, const char *cname, bool is_public) {
  char *fn = codegen_type_fn_name(g, cname, "write");
  const char *linkage = "static ";
  if (is_public) {
    linkage = "";
  }
  strbuf_appendf(g->out, "%sBareStatus %s(BareWriter *w, const %s *value) {\n", linkage, fn, cname);
  emit_write_body(g, t);
  strbuf_append(g->out, "}\n\n");
  free(fn);
}
