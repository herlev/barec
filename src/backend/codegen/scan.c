#include "backend/codegen/internal.h"

#include "backend/config.h"
#include "frontend/schema.h"
#include "util/macros.h"
#include "util/strbuf.h"
#include "util/types.h"
#include "util/vec.h"

#include <assert.h>
#include <inttypes.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

static const char *const BARE_TYPE_NAME[] = {
    [TypeKind_UINT] = "uint",     [TypeKind_U8] = "u8",     [TypeKind_U16] = "u16",
    [TypeKind_U32] = "u32",       [TypeKind_U64] = "u64",   [TypeKind_INT] = "int",
    [TypeKind_I8] = "i8",         [TypeKind_I16] = "i16",   [TypeKind_I32] = "i32",
    [TypeKind_I64] = "i64",       [TypeKind_F32] = "f32",   [TypeKind_F64] = "f64",
    [TypeKind_BOOL] = "bool",     [TypeKind_STR] = "str",   [TypeKind_DATA] = "data",
    [TypeKind_VOID] = "void",     [TypeKind_ENUM] = "enum", [TypeKind_OPTIONAL] = "optional",
    [TypeKind_LIST] = "list",     [TypeKind_MAP] = "map",   [TypeKind_UNION] = "union",
    [TypeKind_STRUCT] = "struct", [TypeKind_USER] = "user",
};

const char *codegen_bare_type_name(TypeKind kind) { return BARE_TYPE_NAME[kind]; }

const char *codegen_name_of(const Gen *g, const Type *t) {
  for (size_t i = 0; i < g->names.len; i++) {
    if (g->names.ptr[i].type == t) {
      return g->names.ptr[i].name;
    }
  }
  UNREACHABLE();
}

const char *codegen_tag_name_of(const Gen *g, const Type *t) {
  for (size_t i = 0; i < g->tag_names.len; i++) {
    if (g->tag_names.ptr[i].type == t) {
      return g->tag_names.ptr[i].name;
    }
  }
  UNREACHABLE();
}

u32 codegen_cap_of(const Gen *g, const Type *t) {
  for (size_t i = 0; i < g->caps.len; i++) {
    if (g->caps.ptr[i].type == t) {
      return g->caps.ptr[i].cap;
    }
  }
  UNREACHABLE();
}

const VEC(GenName) * codegen_bases_of(const Gen *g, const Type *t) {
  for (size_t i = 0; i < g->union_bases.len; i++) {
    if (g->union_bases.ptr[i].type == t) {
      return &g->union_bases.ptr[i].bases;
    }
  }
  UNREACHABLE();
}

static void add_cap_unique(VEC(u32) * set, u32 cap) {
  for (size_t i = 0; i < set->len; i++) {
    if (set->ptr[i] == cap) {
      return;
    }
  }
  VEC_PUSH(set, cap);
}

static u32 resolve_cap(Gen *g, const StrBuf *path, u32 fallback) {
  Str p = {.data = path->data, .len = path->len};
  for (size_t i = 0; i < g->cfg->overrides_len; i++) {
    if (str_eq(g->cfg->overrides[i].path, p)) {
      g->override_used[i] = true;
      return g->cfg->overrides[i].cap;
    }
  }
  return fallback;
}

static size_t path_push(StrBuf *path, Str seg) {
  size_t saved = path->len;
  strbuf_append_char(path, '.');
  strbuf_append_str(path, seg);
  return saved;
}

static void path_pop(StrBuf *path, size_t saved) {
  path->len = saved;
  path->data[saved] = '\0';
}

static void union_member_bases(const Type *t, VEC(GenName) * out) {
  VEC(GenName) bases = {};
  for (size_t i = 0; i < t->union_members.len; i++) {
    const UnionMember *m = &t->union_members.members[i];
    assert(m->tag.has_value && "check_schema assigns implicit union tags");
    StrBuf b = {};
    if (m->type->kind == TypeKind_USER) {
      strbuf_append_str(&b, m->type->user.name);
    } else if (codegen_is_aggregate(m->type->kind)) {
      strbuf_appendf(&b, "member%" PRIu64, m->tag.value);
    } else {
      strbuf_append(&b, BARE_TYPE_NAME[m->type->kind]);
    }
    VEC_PUSH(&bases, b.data);
  }
  for (size_t i = 0; i < bases.len; i++) {
    bool dup = false;
    for (size_t j = 0; j < i; j++) {
      if (strcmp(bases.ptr[j], bases.ptr[i]) == 0) {
        dup = true;
        break;
      }
    }
    if (dup) {
      StrBuf b = {};
      strbuf_appendf(&b, "%s%" PRIu64, bases.ptr[i], t->union_members.members[i].tag.value);
      free(bases.ptr[i]);
      bases.ptr[i] = b.data;
    }
  }
  *out = bases;
}

static char *render_path_type_name(const Gen *g, const StrBuf *path) {
  return codegen_render_type_name(g->cfg, (Str){.data = path->data, .len = path->len});
}

void codegen_scan_type(Gen *g, const Type *t, StrBuf *path) {
  switch (t->kind) {
  case TypeKind_STR: {
    u32 cap = resolve_cap(g, path, g->cfg->str_cap);
    VEC_PUSH(&g->caps, ((SiteCap){.type = t, .cap = cap}));
    add_cap_unique(&g->str_caps, cap);
    break;
  }
  case TypeKind_DATA:
    if (!t->data.length.has_value) {
      u32 cap = resolve_cap(g, path, g->cfg->data_cap);
      VEC_PUSH(&g->caps, ((SiteCap){.type = t, .cap = cap}));
      add_cap_unique(&g->data_caps, cap);
    }
    break;
  case TypeKind_OPTIONAL:
    codegen_scan_type(g, t->optional.inner, path);
    break;
  case TypeKind_LIST: {
    if (!t->list.length.has_value) {
      u32 cap = resolve_cap(g, path, g->cfg->list_cap);
      VEC_PUSH(&g->caps, ((SiteCap){.type = t, .cap = cap}));
    }
    size_t saved = path_push(path, STR("item"));
    codegen_scan_type(g, t->list.elem, path);
    path_pop(path, saved);
    break;
  }
  case TypeKind_MAP: {
    u32 cap = resolve_cap(g, path, g->cfg->map_cap);
    VEC_PUSH(&g->caps, ((SiteCap){.type = t, .cap = cap}));
    size_t saved = path_push(path, STR("key"));
    codegen_scan_type(g, t->map.key, path);
    path_pop(path, saved);
    saved = path_push(path, STR("value"));
    codegen_scan_type(g, t->map.value, path);
    path_pop(path, saved);
    break;
  }
  case TypeKind_ENUM:
    VEC_PUSH(&g->names, ((TypeName){.type = t, .name = render_path_type_name(g, path)}));
    break;
  case TypeKind_STRUCT: {
    for (size_t i = 0; i < t->struct_fields.len; i++) {
      size_t saved = path_push(path, t->struct_fields.fields[i].name);
      codegen_scan_type(g, t->struct_fields.fields[i].type, path);
      path_pop(path, saved);
    }
    break;
  }
  case TypeKind_UNION: {
    VEC_PUSH(&g->names, ((TypeName){.type = t, .name = render_path_type_name(g, path)}));
    size_t tag_saved = path_push(path, STR("tag"));
    char *tag_name = render_path_type_name(g, path);
    path_pop(path, tag_saved);
    VEC_PUSH(&g->tag_names, ((TypeName){.type = t, .name = tag_name}));
    VEC(GenName) bases = {};
    union_member_bases(t, &bases);
    VEC_PUSH(&g->union_bases, ((UnionBases){.type = t, .bases = bases}));
    for (size_t i = 0; i < t->union_members.len; i++) {
      size_t saved = path_push(path, (Str){.data = bases.ptr[i], .len = strlen(bases.ptr[i])});
      codegen_scan_type(g, t->union_members.members[i].type, path);
      path_pop(path, saved);
    }
    break;
  }
  default:
    break;
  }
}
