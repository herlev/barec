#include "backend/codegen/internal.h"

#include "backend/config.h"
#include "backend/names.h"
#include "frontend/schema.h"
#include "util/macros.h"
#include "util/strbuf.h"
#include "util/types.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

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
    assert(t->enum_values.values[i].value.has_value && "check_schema assigns implicit enum values");
    u64 n = uleb_len(t->enum_values.values[i].value.value);
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
    const UnionMember *m = &t->union_members.members[i];
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
    WireSize field = codegen_wire_size(g, t->struct_fields.fields[i].type);
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
