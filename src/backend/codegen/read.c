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

static void emit_read_step(Gen *g, const Type *t, const char *expr, int indent, int depth);

static void emit_read_call(Gen *g, const char *cname, const char *expr, int indent) {
  char *fn = codegen_type_fn_name(g, cname, "read");
  codegen_indent(g->out, indent);
  strbuf_appendf(g->out, "BARE_TRY(%s(r, &%s));\n", fn, expr);
  free(fn);
}

static void emit_read_optional(Gen *g, const Type *inner, const char *has_expr,
                               const char *value_expr, int indent, int depth) {
  StrBuf *out = g->out;
  codegen_indent(out, indent);
  strbuf_append(out, "{\n");
  codegen_indent(out, indent + 2);
  strbuf_appendf(out, "uint8_t p%d;\n", depth);
  codegen_indent(out, indent + 2);
  strbuf_appendf(out, "BARE_TRY(bare_read_u8(r, &p%d));\n", depth);
  codegen_indent(out, indent + 2);
  strbuf_appendf(out, "if (p%d > 1) {\n", depth);
  codegen_indent(out, indent + 4);
  strbuf_append(out, "return BareStatus_INVALID_OPTIONAL;\n");
  codegen_indent(out, indent + 2);
  strbuf_append(out, "}\n");
  codegen_indent(out, indent + 2);
  strbuf_appendf(out, "%s = p%d == 1;\n", has_expr, depth);
  codegen_indent(out, indent + 2);
  strbuf_appendf(out, "if (%s) {\n", has_expr);
  emit_read_step(g, inner, value_expr, indent + 4, depth + 1);
  codegen_indent(out, indent + 2);
  strbuf_append(out, "}\n");
  codegen_indent(out, indent);
  strbuf_append(out, "}\n");
}

static void emit_read_list_fixed(Gen *g, const Type *elem, const char *arr_expr, u64 n, int indent,
                                 int depth) {
  StrBuf *out = g->out;
  codegen_indent(out, indent);
  strbuf_appendf(out, "for (uint64_t i%d = 0; i%d < %" PRIu64 "; i%d++) {\n", depth, depth, n,
                 depth);
  StrBuf elem_expr = {};
  strbuf_appendf(&elem_expr, "%s[i%d]", arr_expr, depth);
  emit_read_step(g, elem, elem_expr.data, indent + 2, depth + 1);
  strbuf_free(&elem_expr);
  codegen_indent(out, indent);
  strbuf_append(out, "}\n");
}

static void emit_read_list_var(Gen *g, const Type *elem, const char *items_expr,
                               const char *len_expr, u32 cap, int indent, int depth) {
  StrBuf *out = g->out;
  codegen_indent(out, indent);
  strbuf_append(out, "{\n");
  codegen_indent(out, indent + 2);
  strbuf_appendf(out, "uint64_t n%d;\n", depth);
  codegen_indent(out, indent + 2);
  strbuf_appendf(out, "BARE_TRY(bare_read_uint(r, &n%d));\n", depth);
  codegen_indent(out, indent + 2);
  strbuf_appendf(out, "if (n%d > %" PRIu32 ") {\n", depth, cap);
  codegen_indent(out, indent + 4);
  strbuf_append(out, "return BareStatus_CAP_EXCEEDED;\n");
  codegen_indent(out, indent + 2);
  strbuf_append(out, "}\n");
  codegen_indent(out, indent + 2);
  strbuf_appendf(out, "%s = (uint32_t)n%d;\n", len_expr, depth);
  codegen_indent(out, indent + 2);
  strbuf_appendf(out, "for (uint32_t i%d = 0; i%d < %s; i%d++) {\n", depth, depth, len_expr, depth);
  StrBuf elem_expr = {};
  strbuf_appendf(&elem_expr, "%s[i%d]", items_expr, depth);
  emit_read_step(g, elem, elem_expr.data, indent + 4, depth + 1);
  strbuf_free(&elem_expr);
  codegen_indent(out, indent + 2);
  strbuf_append(out, "}\n");
  codegen_indent(out, indent);
  strbuf_append(out, "}\n");
}

static void emit_key_equal(const Type *key, const char *a, const char *b, StrBuf *out) {
  const Type *k = type_underlying(key);
  switch (k->kind) {
  case TypeKind_STR:
  case TypeKind_DATA:
    if (k->kind == TypeKind_DATA && k->data.length.has_value) {
      strbuf_appendf(out, "memcmp(%s, %s, %" PRIu64 ") == 0", a, b, k->data.length.value);
    } else {
      strbuf_appendf(out, "%s.len == %s.len && memcmp(%s.data, %s.data, %s.len) == 0", a, b, a, b,
                     a);
    }
    break;
  default:
    strbuf_appendf(out, "%s == %s", a, b);
    break;
  }
}

static void emit_read_map(Gen *g, const Type *key, const Type *value, const char *entries_expr,
                          const char *len_expr, u32 cap, int indent, int depth) {
  StrBuf *out = g->out;
  codegen_indent(out, indent);
  strbuf_append(out, "{\n");
  codegen_indent(out, indent + 2);
  strbuf_appendf(out, "uint64_t n%d;\n", depth);
  codegen_indent(out, indent + 2);
  strbuf_appendf(out, "BARE_TRY(bare_read_uint(r, &n%d));\n", depth);
  codegen_indent(out, indent + 2);
  strbuf_appendf(out, "if (n%d > %" PRIu32 ") {\n", depth, cap);
  codegen_indent(out, indent + 4);
  strbuf_append(out, "return BareStatus_CAP_EXCEEDED;\n");
  codegen_indent(out, indent + 2);
  strbuf_append(out, "}\n");
  codegen_indent(out, indent + 2);
  strbuf_appendf(out, "%s = (uint32_t)n%d;\n", len_expr, depth);
  codegen_indent(out, indent + 2);
  strbuf_appendf(out, "for (uint32_t i%d = 0; i%d < %s; i%d++) {\n", depth, depth, len_expr, depth);
  StrBuf key_expr = {};
  strbuf_appendf(&key_expr, "%s[i%d].key", entries_expr, depth);
  StrBuf value_expr = {};
  strbuf_appendf(&value_expr, "%s[i%d].value", entries_expr, depth);
  emit_read_step(g, key, key_expr.data, indent + 4, depth + 1);
  emit_read_step(g, value, value_expr.data, indent + 4, depth + 1);
  codegen_indent(out, indent + 4);
  strbuf_appendf(out, "for (uint32_t j%d = 0; j%d < i%d; j%d++) {\n", depth, depth, depth, depth);
  StrBuf prev_key = {};
  strbuf_appendf(&prev_key, "%s[j%d].key", entries_expr, depth);
  StrBuf eq = {};
  emit_key_equal(key, prev_key.data, key_expr.data, &eq);
  codegen_indent(out, indent + 6);
  strbuf_appendf(out, "if (%s) {\n", eq.data);
  codegen_indent(out, indent + 8);
  strbuf_append(out, "return BareStatus_DUPLICATE_KEY;\n");
  codegen_indent(out, indent + 6);
  strbuf_append(out, "}\n");
  codegen_indent(out, indent + 4);
  strbuf_append(out, "}\n");
  strbuf_free(&eq);
  strbuf_free(&prev_key);
  strbuf_free(&key_expr);
  strbuf_free(&value_expr);
  codegen_indent(out, indent + 2);
  strbuf_append(out, "}\n");
  codegen_indent(out, indent);
  strbuf_append(out, "}\n");
}

static void emit_read_step(Gen *g, const Type *t, const char *expr, int indent, int depth) {
  StrBuf *out = g->out;
  switch (t->kind) {
  case TypeKind_STR:
    codegen_indent(out, indent);
    strbuf_appendf(out, "BARE_TRY(bare_read_str(r, %s.data, %" PRIu32 ", &%s.len));\n", expr,
                   codegen_cap_of(g, t), expr);
    break;
  case TypeKind_DATA:
    codegen_indent(out, indent);
    if (t->data.length.has_value) {
      strbuf_appendf(out, "BARE_TRY(bare_read_data_fixed(r, %s, %" PRIu64 "));\n", expr,
                     t->data.length.value);
    } else {
      strbuf_appendf(out, "BARE_TRY(bare_read_data(r, %s.data, %" PRIu32 ", &%s.len));\n", expr,
                     codegen_cap_of(g, t), expr);
    }
    break;
  case TypeKind_USER: {
    char *cname = codegen_render_ident(g->cfg, t->user.name, g->cfg->type_case, true);
    emit_read_call(g, cname, expr, indent);
    free(cname);
    break;
  }
  case TypeKind_ENUM:
  case TypeKind_STRUCT:
  case TypeKind_UNION:
    emit_read_call(g, codegen_name_of(g, t), expr, indent);
    break;
  case TypeKind_OPTIONAL: {
    StrBuf has = {};
    strbuf_appendf(&has, "%s.has_value", expr);
    StrBuf value = {};
    strbuf_appendf(&value, "%s.value", expr);
    emit_read_optional(g, t->optional.inner, has.data, value.data, indent, depth);
    strbuf_free(&has);
    strbuf_free(&value);
    break;
  }
  case TypeKind_LIST:
    if (t->list.length.has_value) {
      emit_read_list_fixed(g, t->list.elem, expr, t->list.length.value, indent, depth);
    } else {
      StrBuf items = {};
      strbuf_appendf(&items, "%s.items", expr);
      StrBuf len = {};
      strbuf_appendf(&len, "%s.len", expr);
      emit_read_list_var(g, t->list.elem, items.data, len.data, codegen_cap_of(g, t), indent,
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
    emit_read_map(g, t->map.key, t->map.value, entries.data, len.data, codegen_cap_of(g, t), indent,
                  depth);
    strbuf_free(&entries);
    strbuf_free(&len);
    break;
  }
  case TypeKind_VOID:
    UNREACHABLE();
  default:
    codegen_indent(out, indent);
    strbuf_appendf(out, "BARE_TRY(bare_read_%s(r, &%s));\n", codegen_bare_type_name(t->kind), expr);
    break;
  }
}

static void emit_read_enum_body(Gen *g, const Type *t, const char *cname) {
  StrBuf *out = g->out;
  strbuf_append(out, "  uint64_t raw;\n  BARE_TRY(bare_read_uint(r, &raw));\n  switch (raw) {\n");
  for (size_t i = 0; i < t->enum_values.len; i++) {
    strbuf_appendf(out, "  case %" PRIu64 ":\n", t->enum_values.values[i].value.value);
  }
  strbuf_append(out, "    break;\n  default:\n    return BareStatus_INVALID_ENUM;\n  }\n");
  strbuf_appendf(out, "  *out = (%s)raw;\n  return BareStatus_OK;\n", cname);
}

static void emit_read_union_body(Gen *g, const Type *t) {
  StrBuf *out = g->out;
  const char *tag_cname = codegen_tag_name_of(g, t);
  const VEC(GenName) *bases = codegen_bases_of(g, t);
  strbuf_append(out, "  uint64_t tag;\n  BARE_TRY(bare_read_uint(r, &tag));\n  switch (tag) {\n");
  for (size_t i = 0; i < t->union_members.len; i++) {
    const UnionMember *m = &t->union_members.members[i];
    char *variant = codegen_render_variant(
        g, tag_cname, (Str){.data = bases->ptr[i], .len = strlen(bases->ptr[i])});
    strbuf_appendf(out, "  case %" PRIu64 ":\n    out->tag = %s;\n", m->tag.value, variant);
    free(variant);
    if (type_underlying(m->type)->kind != TypeKind_VOID) {
      char *arm = codegen_render_ident_cstr(g->cfg, bases->ptr[i], g->cfg->field_case, false);
      StrBuf expr = {};
      strbuf_appendf(&expr, "out->value.%s", arm);
      emit_read_step(g, m->type, expr.data, 4, 0);
      strbuf_free(&expr);
      free(arm);
    }
    strbuf_append(out, "    break;\n");
  }
  strbuf_append(out, "  default:\n    return BareStatus_INVALID_TAG;\n  }\n"
                     "  return BareStatus_OK;\n");
}

static void emit_read_body(Gen *g, const Type *t, const char *cname) {
  StrBuf *out = g->out;
  switch (t->kind) {
  case TypeKind_USER: {
    char *target = codegen_render_ident(g->cfg, t->user.name, g->cfg->type_case, true);
    char *fn = codegen_type_fn_name(g, target, "read");
    strbuf_appendf(out, "  return %s(r, out);\n", fn);
    free(fn);
    free(target);
    break;
  }
  case TypeKind_STR:
    strbuf_appendf(out, "  return bare_read_str(r, out->data, %" PRIu32 ", &out->len);\n",
                   codegen_cap_of(g, t));
    break;
  case TypeKind_DATA:
    if (t->data.length.has_value) {
      strbuf_appendf(out, "  return bare_read_data_fixed(r, out->data, %" PRIu64 ");\n",
                     t->data.length.value);
    } else {
      strbuf_appendf(out, "  return bare_read_data(r, out->data, %" PRIu32 ", &out->len);\n",
                     codegen_cap_of(g, t));
    }
    break;
  case TypeKind_ENUM:
    emit_read_enum_body(g, t, cname);
    break;
  case TypeKind_STRUCT:
    for (size_t i = 0; i < t->struct_fields.len; i++) {
      const StructField *field = &t->struct_fields.fields[i];
      char *fname = codegen_render_ident(g->cfg, field->name, g->cfg->field_case, false);
      StrBuf expr = {};
      strbuf_appendf(&expr, "out->%s", fname);
      emit_read_step(g, field->type, expr.data, 2, 0);
      strbuf_free(&expr);
      free(fname);
    }
    strbuf_append(out, "  return BareStatus_OK;\n");
    break;
  case TypeKind_UNION:
    emit_read_union_body(g, t);
    break;
  case TypeKind_OPTIONAL:
    emit_read_optional(g, t->optional.inner, "out->has_value", "out->value", 2, 0);
    strbuf_append(out, "  return BareStatus_OK;\n");
    break;
  case TypeKind_LIST:
    if (t->list.length.has_value) {
      emit_read_list_fixed(g, t->list.elem, "out->items", t->list.length.value, 2, 0);
    } else {
      emit_read_list_var(g, t->list.elem, "out->items", "out->len", codegen_cap_of(g, t), 2, 0);
    }
    strbuf_append(out, "  return BareStatus_OK;\n");
    break;
  case TypeKind_MAP:
    emit_read_map(g, t->map.key, t->map.value, "out->entries", "out->len", codegen_cap_of(g, t), 2,
                  0);
    strbuf_append(out, "  return BareStatus_OK;\n");
    break;
  case TypeKind_VOID:
    UNREACHABLE();
  default:
    strbuf_appendf(out, "  return bare_read_%s(r, out);\n", codegen_bare_type_name(t->kind));
    break;
  }
}

void codegen_emit_read_fn(Gen *g, const Type *t, const char *cname, bool is_public) {
  char *fn = codegen_type_fn_name(g, cname, "read");
  const char *linkage = "static ";
  if (is_public) {
    linkage = "";
  }
  strbuf_appendf(g->out, "%sBareStatus %s(BareReader *r, %s *out) {\n", linkage, fn, cname);
  emit_read_body(g, t, cname);
  strbuf_append(g->out, "}\n\n");
  free(fn);
}
