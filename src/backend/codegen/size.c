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
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static u64 sat_add(u64 a, u64 b) {
  if (a > UINT64_MAX - b) {
    return UINT64_MAX;
  }
  return a + b;
}

static u64 sat_mul(u64 a, u64 b) {
  if (b != 0 && a > UINT64_MAX / b) {
    return UINT64_MAX;
  }
  return a * b;
}

static u64 uleb_len(u64 value) {
  u64 n = 1;
  while (value > 0x7f) {
    value >>= 7;
    n += 1;
  }
  return n;
}

static WireSize enum_wire_size(const Type *t) {
  assert(t->enum_values.len > 0 && "the parser rejects empty enums");
  u64 min = UINT64_MAX;
  u64 max = 0;
  for (size_t i = 0; i < t->enum_values.len; i++) {
    assert(t->enum_values.ptr[i].value.has_value && "check_schema assigns implicit enum values");
    u64 n = uleb_len(t->enum_values.ptr[i].value.value);
    min = MIN(min, n);
    max = MAX(max, n);
  }
  return (WireSize){.max = max, .fixed = (bool)(min == max)};
}

static WireSize union_wire_size(const Gen *g, const Type *t) {
  assert(t->union_members.len > 0 && "the parser rejects empty unions");
  u64 max = 0;
  u64 min = UINT64_MAX;
  bool members_fixed = true;
  for (size_t i = 0; i < t->union_members.len; i++) {
    const UnionMember *m = &t->union_members.ptr[i];
    assert(m->tag.has_value && "check_schema assigns implicit union tags");
    WireSize member = codegen_wire_size(g, m->type);
    u64 total = sat_add(uleb_len(m->tag.value), member.max);
    max = MAX(max, total);
    min = MIN(min, total);
    members_fixed = (bool)(members_fixed && member.fixed);
  }
  return (WireSize){.max = max, .fixed = (bool)(members_fixed && min == max)};
}

static WireSize struct_wire_size(const Gen *g, const Type *t) {
  u64 sum = 0;
  bool fixed = true;
  for (size_t i = 0; i < t->struct_fields.len; i++) {
    WireSize field = codegen_wire_size(g, t->struct_fields.ptr[i].type);
    sum = sat_add(sum, field.max);
    fixed = (bool)(fixed && field.fixed);
  }
  return (WireSize){.max = sum, .fixed = fixed};
}

WireSize codegen_wire_size(const Gen *g, const Type *t) {
  switch (t->kind) {
  case TypeKind_UINT:
  case TypeKind_INT:
    return (WireSize){.max = 10, .fixed = false};
  case TypeKind_U8:
  case TypeKind_I8:
  case TypeKind_BOOL:
    return (WireSize){.max = 1, .fixed = true};
  case TypeKind_U16:
  case TypeKind_I16:
    return (WireSize){.max = 2, .fixed = true};
  case TypeKind_U32:
  case TypeKind_I32:
  case TypeKind_F32:
    return (WireSize){.max = 4, .fixed = true};
  case TypeKind_U64:
  case TypeKind_I64:
  case TypeKind_F64:
    return (WireSize){.max = 8, .fixed = true};
  case TypeKind_VOID:
    return (WireSize){.max = 0, .fixed = true};
  case TypeKind_STR: {
    u64 cap = codegen_cap_of(g, t);
    return (WireSize){.max = sat_add(uleb_len(cap), cap), .fixed = false};
  }
  case TypeKind_DATA: {
    if (t->data.length.has_value) {
      return (WireSize){.max = t->data.length.value, .fixed = true};
    }
    u64 cap = codegen_cap_of(g, t);
    return (WireSize){.max = sat_add(uleb_len(cap), cap), .fixed = false};
  }
  case TypeKind_ENUM:
    return enum_wire_size(t);
  case TypeKind_OPTIONAL: {
    WireSize inner = codegen_wire_size(g, t->optional.inner);
    return (WireSize){.max = sat_add(1, inner.max), .fixed = false};
  }
  case TypeKind_LIST: {
    WireSize elem = codegen_wire_size(g, t->list.elem);
    if (t->list.length.has_value) {
      return (WireSize){.max = sat_mul(t->list.length.value, elem.max), .fixed = elem.fixed};
    }
    u64 cap = codegen_cap_of(g, t);
    return (WireSize){.max = sat_add(uleb_len(cap), sat_mul(cap, elem.max)), .fixed = false};
  }
  case TypeKind_MAP: {
    WireSize key = codegen_wire_size(g, t->map.key);
    WireSize value = codegen_wire_size(g, t->map.value);
    u64 cap = codegen_cap_of(g, t);
    u64 entry = sat_add(key.max, value.max);
    return (WireSize){.max = sat_add(uleb_len(cap), sat_mul(cap, entry)), .fixed = false};
  }
  case TypeKind_UNION:
    return union_wire_size(g, t);
  case TypeKind_STRUCT:
    return struct_wire_size(g, t);
  case TypeKind_USER:
    assert(t->user.resolved != nullptr && "check_schema resolves user references");
    return codegen_wire_size(g, t->user.resolved);
  }
  UNREACHABLE();
}

static void emit_size_step(const Gen *g, const Type *t, const char *v, int indent, int depth);

[[gnu::format(printf, 3, 4)]]
static void emit_size_line(const Gen *g, int indent, const char *fmt, ...) {
  codegen_indent(g->out, indent);
  va_list args;
  va_start(args, fmt);
  strbuf_vappendf(g->out, fmt, args);
  va_end(args);
}

static void emit_size_call(const Gen *g, const char *cname, const char *v, int indent) {
  char *fn = codegen_type_fn_name(g, cname, "size");
  emit_size_line(g, indent, "n += %s(&%s);\n", fn, v);
  free(fn);
}

static void emit_size_list(const Gen *g, const Type *elem, const char *items, const char *bound,
                           int indent, int depth) {
  emit_size_line(g, indent, "for (uint64_t i%d = 0; i%d < %s; i%d++) {\n", depth, depth, bound,
                 depth);
  StrBuf elem_v = {};
  strbuf_appendf(&elem_v, "%s[i%d]", items, depth);
  emit_size_step(g, elem, elem_v.data, indent + 2, depth + 1);
  strbuf_free(&elem_v);
  emit_size_line(g, indent, "}\n");
}

static void emit_size_step(const Gen *g, const Type *t, const char *v, int indent, int depth) {
  WireSize ws = codegen_wire_size(g, t);
  if (ws.fixed) {
    emit_size_line(g, indent, "n += %s;\n", codegen_u64_lit(ws.max).text);
    return;
  }
  switch (t->kind) {
  case TypeKind_UINT:
    emit_size_line(g, indent, "n += bare_uint_size(%s);\n", v);
    break;
  case TypeKind_INT:
    emit_size_line(g, indent, "n += bare_int_size(%s);\n", v);
    break;
  case TypeKind_ENUM:
    emit_size_line(g, indent, "n += bare_uint_size((uint64_t)%s);\n", v);
    break;
  case TypeKind_STR:
  case TypeKind_DATA:
    emit_size_line(g, indent, "BARE_ASSERT(%s.len <= %" PRIu32 ");\n", v, codegen_cap_of(g, t));
    emit_size_line(g, indent, "n += bare_uint_size(%s.len) + %s.len;\n", v, v);
    break;
  case TypeKind_USER: {
    char *cname = codegen_render_type_name(g->cfg, t->user.name);
    emit_size_call(g, cname, v, indent);
    free(cname);
    break;
  }
  case TypeKind_UNION:
    emit_size_call(g, codegen_name_of(g, t), v, indent);
    break;
  case TypeKind_OPTIONAL: {
    emit_size_line(g, indent, "n += 1;\n");
    emit_size_line(g, indent, "if (%s.%s) {\n", v, g->members.has_value);
    StrBuf inner = {};
    strbuf_appendf(&inner, "%s.%s", v, g->members.value);
    emit_size_step(g, t->optional.inner, inner.data, indent + 2, depth);
    strbuf_free(&inner);
    emit_size_line(g, indent, "}\n");
    break;
  }
  case TypeKind_STRUCT:
    for (size_t i = 0; i < t->struct_fields.len; i++) {
      const StructField *field = &t->struct_fields.ptr[i];
      char *fname = codegen_render_ident(g->cfg, field->name, g->cfg->field_case, false);
      StrBuf field_v = {};
      strbuf_appendf(&field_v, "%s.%s", v, fname);
      emit_size_step(g, field->type, field_v.data, indent, depth);
      strbuf_free(&field_v);
      free(fname);
    }
    break;
  case TypeKind_LIST:
    if (t->list.length.has_value) {
      StrBuf bound = {};
      strbuf_appendf(&bound, "%" PRIu64, t->list.length.value);
      emit_size_list(g, t->list.elem, v, bound.data, indent, depth);
      strbuf_free(&bound);
    } else {
      WireSize elem = codegen_wire_size(g, t->list.elem);
      emit_size_line(g, indent, "BARE_ASSERT(%s.%s <= %" PRIu32 ");\n", v, g->members.len,
                     codegen_cap_of(g, t));
      emit_size_line(g, indent, "n += bare_uint_size(%s.%s);\n", v, g->members.len);
      if (elem.fixed) {
        emit_size_line(g, indent, "n += (uint64_t)%s.%s * %s;\n", v, g->members.len,
                       codegen_u64_lit(elem.max).text);
      } else {
        StrBuf items = {};
        StrBuf bound = {};
        strbuf_appendf(&items, "%s.%s", v, g->members.items);
        strbuf_appendf(&bound, "%s.%s", v, g->members.len);
        emit_size_list(g, t->list.elem, items.data, bound.data, indent, depth);
        strbuf_free(&items);
        strbuf_free(&bound);
      }
    }
    break;
  case TypeKind_MAP: {
    WireSize key = codegen_wire_size(g, t->map.key);
    WireSize value = codegen_wire_size(g, t->map.value);
    emit_size_line(g, indent, "BARE_ASSERT(%s.%s <= %" PRIu32 ");\n", v, g->members.len,
                   codegen_cap_of(g, t));
    emit_size_line(g, indent, "n += bare_uint_size(%s.%s);\n", v, g->members.len);
    if (key.fixed && value.fixed) {
      emit_size_line(g, indent, "n += (uint64_t)%s.%s * %s;\n", v, g->members.len,
                     codegen_u64_lit(sat_add(key.max, value.max)).text);
      break;
    }
    emit_size_line(g, indent, "for (uint64_t i%d = 0; i%d < %s.%s; i%d++) {\n", depth, depth, v,
                   g->members.len, depth);
    StrBuf entry = {};
    strbuf_appendf(&entry, "%s.%s[i%d].%s", v, g->members.entries, depth, g->members.key);
    emit_size_step(g, t->map.key, entry.data, indent + 2, depth + 1);
    entry.len = 0;
    strbuf_appendf(&entry, "%s.%s[i%d].%s", v, g->members.entries, depth, g->members.value);
    emit_size_step(g, t->map.value, entry.data, indent + 2, depth + 1);
    strbuf_free(&entry);
    emit_size_line(g, indent, "}\n");
    break;
  }
  default:
    UNREACHABLE();
  }
}

static void emit_union_size_body(const Gen *g, const Type *t) {
  StrBuf *out = g->out;
  const char *tag_cname = codegen_tag_name_of(g, t);
  const VEC(GenName) *bases = codegen_bases_of(g, t);
  strbuf_appendf(out, "  switch (value->%s) {\n", g->members.tag);
  for (size_t i = 0; i < t->union_members.len; i++) {
    const UnionMember *m = &t->union_members.ptr[i];
    assert(m->tag.has_value && "check_schema assigns implicit union tags");
    u64 tag_size = uleb_len(m->tag.value);
    char *variant = codegen_render_variant(
        g, tag_cname, (Str){.data = bases->ptr[i], .len = strlen(bases->ptr[i])});
    WireSize member = codegen_wire_size(g, m->type);
    if (member.fixed) {
      strbuf_appendf(out, "  case %s:\n    return %s;\n", variant,
                     codegen_u64_lit(sat_add(tag_size, member.max)).text);
      free(variant);
      continue;
    }
    strbuf_appendf(out, "  case %s: {\n    uint64_t n = %s;\n", variant,
                   codegen_u64_lit(tag_size).text);
    free(variant);
    char *arm = codegen_render_ident_cstr(g->cfg, bases->ptr[i], g->cfg->field_case, false);
    StrBuf arm_v = {};
    strbuf_appendf(&arm_v, "value->%s.%s", g->members.value, arm);
    emit_size_step(g, m->type, arm_v.data, 4, 0);
    strbuf_free(&arm_v);
    free(arm);
    strbuf_append(out, "    return n;\n  }\n");
  }
  strbuf_appendf(out, "  }\n  BARE_ASSERT(false && \"value->%s is a valid %s\");\n  return 0;\n",
                 g->members.tag, tag_cname);
}

void codegen_emit_size_fn(const Gen *g, const Type *t, const char *cname, bool is_public) {
  StrBuf *out = g->out;
  char *fn = codegen_type_fn_name(g, cname, "size");
  if (!is_public) {
    strbuf_append(out, "static ");
  }
  strbuf_appendf(out, "uint64_t %s(const %s *value) {\n", fn, cname);
  free(fn);
  WireSize ws = codegen_wire_size(g, t);
  if (ws.fixed) {
    strbuf_appendf(out, "  (void)value;\n  return %s;\n}\n\n", codegen_u64_lit(ws.max).text);
    return;
  }
  switch (t->kind) {
  case TypeKind_USER: {
    char *target = codegen_render_type_name(g->cfg, t->user.name);
    char *target_fn = codegen_type_fn_name(g, target, "size");
    strbuf_appendf(out, "  return %s(value);\n", target_fn);
    free(target_fn);
    free(target);
    break;
  }
  case TypeKind_UNION:
    emit_union_size_body(g, t);
    break;
  case TypeKind_UINT:
    strbuf_append(out, "  return bare_uint_size(*value);\n");
    break;
  case TypeKind_INT:
    strbuf_append(out, "  return bare_int_size(*value);\n");
    break;
  case TypeKind_ENUM:
    strbuf_append(out, "  return bare_uint_size((uint64_t)*value);\n");
    break;
  case TypeKind_STR:
  case TypeKind_DATA:
    strbuf_appendf(out, "  BARE_ASSERT(value->len <= %" PRIu32 ");\n", codegen_cap_of(g, t));
    strbuf_append(out, "  return bare_uint_size(value->len) + value->len;\n");
    break;
  case TypeKind_STRUCT:
    strbuf_append(out, "  uint64_t n = 0;\n");
    for (size_t i = 0; i < t->struct_fields.len; i++) {
      const StructField *field = &t->struct_fields.ptr[i];
      char *fname = codegen_render_ident(g->cfg, field->name, g->cfg->field_case, false);
      StrBuf field_v = {};
      strbuf_appendf(&field_v, "value->%s", fname);
      emit_size_step(g, field->type, field_v.data, 2, 0);
      strbuf_free(&field_v);
      free(fname);
    }
    strbuf_append(out, "  return n;\n");
    break;
  case TypeKind_LIST:
    if (t->list.length.has_value) {
      strbuf_append(out, "  uint64_t n = 0;\n");
      StrBuf items = {};
      StrBuf bound = {};
      strbuf_appendf(&items, "value->%s", g->members.items);
      strbuf_appendf(&bound, "%" PRIu64, t->list.length.value);
      emit_size_list(g, t->list.elem, items.data, bound.data, 2, 0);
      strbuf_free(&items);
      strbuf_free(&bound);
      strbuf_append(out, "  return n;\n");
      break;
    }
    [[fallthrough]];
  default:
    strbuf_append(out, "  uint64_t n = 0;\n");
    emit_size_step(g, t, "(*value)", 2, 0);
    strbuf_append(out, "  return n;\n");
    break;
  }
  strbuf_append(out, "}\n\n");
}

void codegen_emit_size_define(const Gen *g, const Type *t, const char *cname) {
  WireSize size = codegen_wire_size(g, t);
  StrBuf name = {};
  Str base = {.data = cname, .len = codegen_type_base_len(g->cfg, cname)};
  name_render(base, CaseStyle_SCREAMING, &name);
  const char *suffix = "_MAX_SIZE";
  if (size.fixed) {
    suffix = "_SIZE";
  }
  strbuf_append(&name, suffix);
  strbuf_appendf(g->out, "#define %s %s\n\n", name.data, codegen_u64_lit(size.max).text);
  strbuf_free(&name);
}
