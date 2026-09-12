#include "backend/codegen/internal.h"

#include "backend/config.h"
#include "frontend/schema.h"
#include "util/macros.h"
#include "util/strbuf.h"
#include "util/types.h"
#include "util/vec.h"

#include <inttypes.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

static void emit_equal_step(const Gen *g, const Type *t, const char *a, const char *b, int indent,
                            int depth);

static void emit_len_assert(const Gen *g, const char *value, u32 cap, int indent) {
  codegen_indent(g->out, indent);
  strbuf_appendf(g->out, "BARE_ASSERT(%s.len <= %" PRIu32 ");\n", value, cap);
}

static void emit_fail_if(const Gen *g, int indent, const char *cond) {
  StrBuf *out = g->out;
  codegen_indent(out, indent);
  strbuf_appendf(out, "if (%s) {\n", cond);
  codegen_indent(out, indent + 2);
  strbuf_append(out, "return false;\n");
  codegen_indent(out, indent);
  strbuf_append(out, "}\n");
}

static void emit_equal_call(const Gen *g, const char *cname, const char *a, const char *b,
                            int indent) {
  char *fn = codegen_type_fn_name(g, cname, "equal");
  StrBuf cond = {};
  strbuf_appendf(&cond, "!%s(&%s, &%s)", fn, a, b);
  emit_fail_if(g, indent, cond.data);
  strbuf_free(&cond);
  free(fn);
}

static void emit_equal_list(const Gen *g, const Type *elem, const char *a_items,
                            const char *b_items, const char *bound, int indent, int depth) {
  StrBuf *out = g->out;
  codegen_indent(out, indent);
  strbuf_appendf(out, "for (uint64_t i%d = 0; i%d < %s; i%d++) {\n", depth, depth, bound, depth);
  StrBuf a_elem = {};
  StrBuf b_elem = {};
  strbuf_appendf(&a_elem, "%s[i%d]", a_items, depth);
  strbuf_appendf(&b_elem, "%s[i%d]", b_items, depth);
  emit_equal_step(g, elem, a_elem.data, b_elem.data, indent + 2, depth + 1);
  strbuf_free(&a_elem);
  strbuf_free(&b_elem);
  codegen_indent(out, indent);
  strbuf_append(out, "}\n");
}

static void emit_equal_step(const Gen *g, const Type *t, const char *a, const char *b, int indent,
                            int depth) {
  StrBuf *out = g->out;
  StrBuf cond = {};
  switch (t->kind) {
  case TypeKind_F32:
  case TypeKind_F64:
    strbuf_appendf(&cond, "memcmp(&%s, &%s, sizeof(%s)) != 0", a, b, a);
    emit_fail_if(g, indent, cond.data);
    break;
  case TypeKind_STR:
    emit_len_assert(g, a, codegen_cap_of(g, t), indent);
    emit_len_assert(g, b, codegen_cap_of(g, t), indent);
    strbuf_appendf(&cond, "%s.len != %s.len || memcmp(%s.data, %s.data, %s.len) != 0", a, b, a, b,
                   a);
    emit_fail_if(g, indent, cond.data);
    break;
  case TypeKind_DATA:
    if (t->data.length.has_value) {
      strbuf_appendf(&cond, "memcmp(%s, %s, %" PRIu64 ") != 0", a, b, t->data.length.value);
    } else {
      emit_len_assert(g, a, codegen_cap_of(g, t), indent);
      emit_len_assert(g, b, codegen_cap_of(g, t), indent);
      strbuf_appendf(&cond, "%s.len != %s.len || memcmp(%s.data, %s.data, %s.len) != 0", a, b, a, b,
                     a);
    }
    emit_fail_if(g, indent, cond.data);
    break;
  case TypeKind_USER: {
    char *cname = codegen_render_type_name(g->cfg, t->user.name);
    emit_equal_call(g, cname, a, b, indent);
    free(cname);
    break;
  }
  case TypeKind_UNION:
    emit_equal_call(g, codegen_name_of(g, t), a, b, indent);
    break;
  case TypeKind_OPTIONAL: {
    strbuf_appendf(&cond, "%s.%s != %s.%s", a, g->members.has_value, b, g->members.has_value);
    emit_fail_if(g, indent, cond.data);
    codegen_indent(out, indent);
    strbuf_appendf(out, "if (%s.%s) {\n", a, g->members.has_value);
    StrBuf a_value = {};
    StrBuf b_value = {};
    strbuf_appendf(&a_value, "%s.%s", a, g->members.value);
    strbuf_appendf(&b_value, "%s.%s", b, g->members.value);
    emit_equal_step(g, t->optional.inner, a_value.data, b_value.data, indent + 2, depth);
    strbuf_free(&a_value);
    strbuf_free(&b_value);
    codegen_indent(out, indent);
    strbuf_append(out, "}\n");
    break;
  }
  case TypeKind_STRUCT:
    for (size_t i = 0; i < t->struct_fields.len; i++) {
      const StructField *field = &t->struct_fields.ptr[i];
      char *fname = codegen_render_ident(g->cfg, field->name, g->cfg->field_case, false);
      StrBuf a_field = {};
      StrBuf b_field = {};
      strbuf_appendf(&a_field, "%s.%s", a, fname);
      strbuf_appendf(&b_field, "%s.%s", b, fname);
      emit_equal_step(g, field->type, a_field.data, b_field.data, indent, depth);
      strbuf_free(&a_field);
      strbuf_free(&b_field);
      free(fname);
    }
    break;
  case TypeKind_LIST:
    if (t->list.length.has_value) {
      StrBuf bound = {};
      strbuf_appendf(&bound, "%" PRIu64, t->list.length.value);
      emit_equal_list(g, t->list.elem, a, b, bound.data, indent, depth);
      strbuf_free(&bound);
    } else {
      emit_len_assert(g, a, codegen_cap_of(g, t), indent);
      emit_len_assert(g, b, codegen_cap_of(g, t), indent);
      strbuf_appendf(&cond, "%s.%s != %s.%s", a, g->members.len, b, g->members.len);
      emit_fail_if(g, indent, cond.data);
      StrBuf a_items = {};
      StrBuf b_items = {};
      StrBuf bound = {};
      strbuf_appendf(&a_items, "%s.%s", a, g->members.items);
      strbuf_appendf(&b_items, "%s.%s", b, g->members.items);
      strbuf_appendf(&bound, "%s.%s", a, g->members.len);
      emit_equal_list(g, t->list.elem, a_items.data, b_items.data, bound.data, indent, depth);
      strbuf_free(&a_items);
      strbuf_free(&b_items);
      strbuf_free(&bound);
    }
    break;
  case TypeKind_MAP: {
    emit_len_assert(g, a, codegen_cap_of(g, t), indent);
    emit_len_assert(g, b, codegen_cap_of(g, t), indent);
    strbuf_appendf(&cond, "%s.%s != %s.%s", a, g->members.len, b, g->members.len);
    emit_fail_if(g, indent, cond.data);
    codegen_indent(out, indent);
    strbuf_appendf(out, "for (uint64_t i%d = 0; i%d < %s.%s; i%d++) {\n", depth, depth, a,
                   g->members.len, depth);
    StrBuf a_entry = {};
    StrBuf b_entry = {};
    strbuf_appendf(&a_entry, "%s.%s[i%d].%s", a, g->members.entries, depth, g->members.key);
    strbuf_appendf(&b_entry, "%s.%s[i%d].%s", b, g->members.entries, depth, g->members.key);
    emit_equal_step(g, t->map.key, a_entry.data, b_entry.data, indent + 2, depth + 1);
    a_entry.len = 0;
    b_entry.len = 0;
    strbuf_appendf(&a_entry, "%s.%s[i%d].%s", a, g->members.entries, depth, g->members.value);
    strbuf_appendf(&b_entry, "%s.%s[i%d].%s", b, g->members.entries, depth, g->members.value);
    emit_equal_step(g, t->map.value, a_entry.data, b_entry.data, indent + 2, depth + 1);
    strbuf_free(&a_entry);
    strbuf_free(&b_entry);
    codegen_indent(out, indent);
    strbuf_append(out, "}\n");
    break;
  }
  case TypeKind_VOID:
    UNREACHABLE();
  default:
    strbuf_appendf(&cond, "%s != %s", a, b);
    emit_fail_if(g, indent, cond.data);
    break;
  }
  strbuf_free(&cond);
}

static void emit_union_equal_body(const Gen *g, const Type *t) {
  StrBuf *out = g->out;
  const char *tag_cname = codegen_tag_name_of(g, t);
  const VEC(GenName) *bases = codegen_bases_of(g, t);
  StrBuf cond = {};
  strbuf_appendf(&cond, "a->%s != b->%s", g->members.tag, g->members.tag);
  emit_fail_if(g, 2, cond.data);
  strbuf_free(&cond);
  strbuf_appendf(out, "  switch (a->%s) {\n", g->members.tag);
  for (size_t i = 0; i < t->union_members.len; i++) {
    const UnionMember *m = &t->union_members.ptr[i];
    char *variant = codegen_render_variant(
        g, tag_cname, (Str){.data = bases->ptr[i], .len = strlen(bases->ptr[i])});
    strbuf_appendf(out, "  case %s:\n", variant);
    free(variant);
    if (type_underlying(m->type)->kind != TypeKind_VOID) {
      char *arm = codegen_render_ident_cstr(g->cfg, bases->ptr[i], g->cfg->field_case, false);
      StrBuf a_arm = {};
      StrBuf b_arm = {};
      strbuf_appendf(&a_arm, "a->%s.%s", g->members.value, arm);
      strbuf_appendf(&b_arm, "b->%s.%s", g->members.value, arm);
      emit_equal_step(g, m->type, a_arm.data, b_arm.data, 4, 0);
      strbuf_free(&a_arm);
      strbuf_free(&b_arm);
      free(arm);
    }
    strbuf_append(out, "    return true;\n");
  }
  strbuf_appendf(out, "  }\n  BARE_ASSERT(false && \"a->%s is a valid %s\");\n  return false;\n",
                 g->members.tag, tag_cname);
}

void codegen_emit_equal_fn(const Gen *g, const Type *t, const char *cname, bool is_public) {
  StrBuf *out = g->out;
  char *fn = codegen_type_fn_name(g, cname, "equal");
  if (!is_public) {
    strbuf_append(out, "static ");
  }
  strbuf_appendf(out, "bool %s(const %s *a, const %s *b) {\n", fn, cname, cname);
  free(fn);
  switch (t->kind) {
  case TypeKind_USER: {
    char *target = codegen_render_type_name(g->cfg, t->user.name);
    char *target_fn = codegen_type_fn_name(g, target, "equal");
    strbuf_appendf(out, "  return %s(a, b);\n", target_fn);
    free(target_fn);
    free(target);
    break;
  }
  case TypeKind_F32:
  case TypeKind_F64:
    strbuf_append(out, "  return memcmp(a, b, sizeof(*a)) == 0;\n");
    break;
  case TypeKind_STR:
    strbuf_appendf(out,
                   "  BARE_ASSERT(a->len <= %" PRIu32 ");\n  BARE_ASSERT(b->len <= %" PRIu32 ");\n",
                   codegen_cap_of(g, t), codegen_cap_of(g, t));
    strbuf_append(out, "  return a->len == b->len && memcmp(a->data, b->data, a->len) == 0;\n");
    break;
  case TypeKind_DATA:
    if (t->data.length.has_value) {
      strbuf_appendf(out, "  return memcmp(a->%s, b->%s, %" PRIu64 ") == 0;\n", g->members.data,
                     g->members.data, t->data.length.value);
    } else {
      strbuf_appendf(
          out, "  BARE_ASSERT(a->len <= %" PRIu32 ");\n  BARE_ASSERT(b->len <= %" PRIu32 ");\n",
          codegen_cap_of(g, t), codegen_cap_of(g, t));
      strbuf_append(out, "  return a->len == b->len && memcmp(a->data, b->data, a->len) == 0;\n");
    }
    break;
  case TypeKind_UNION:
    emit_union_equal_body(g, t);
    break;
  case TypeKind_LIST:
    if (t->list.length.has_value) {
      StrBuf a_items = {};
      StrBuf b_items = {};
      StrBuf bound = {};
      strbuf_appendf(&a_items, "a->%s", g->members.items);
      strbuf_appendf(&b_items, "b->%s", g->members.items);
      strbuf_appendf(&bound, "%" PRIu64, t->list.length.value);
      emit_equal_list(g, t->list.elem, a_items.data, b_items.data, bound.data, 2, 0);
      strbuf_free(&a_items);
      strbuf_free(&b_items);
      strbuf_free(&bound);
      strbuf_append(out, "  return true;\n");
      break;
    }
    [[fallthrough]];
  case TypeKind_OPTIONAL:
  case TypeKind_MAP:
    emit_equal_step(g, t, "(*a)", "(*b)", 2, 0);
    strbuf_append(out, "  return true;\n");
    break;
  case TypeKind_STRUCT:
    for (size_t i = 0; i < t->struct_fields.len; i++) {
      const StructField *field = &t->struct_fields.ptr[i];
      char *fname = codegen_render_ident(g->cfg, field->name, g->cfg->field_case, false);
      StrBuf a_field = {};
      StrBuf b_field = {};
      strbuf_appendf(&a_field, "a->%s", fname);
      strbuf_appendf(&b_field, "b->%s", fname);
      emit_equal_step(g, field->type, a_field.data, b_field.data, 2, 0);
      strbuf_free(&a_field);
      strbuf_free(&b_field);
      free(fname);
    }
    strbuf_append(out, "  return true;\n");
    break;
  case TypeKind_VOID:
    UNREACHABLE();
  default:
    strbuf_append(out, "  return *a == *b;\n");
    break;
  }
  strbuf_append(out, "}\n\n");
}
