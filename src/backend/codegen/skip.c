#include "backend/codegen/internal.h"

#include "frontend/schema.h"
#include "util/macros.h"
#include "util/strbuf.h"
#include "util/types.h"

#include <assert.h>
#include <stddef.h>
#include <stdlib.h>

static void emit_skip_step(const Gen *g, const Type *t, int indent, int depth);

/// Whether the wire size is the same for every byte sequence, not merely
/// for every valid value. Varints are self-describing, so an enum or union
/// whose valid encodings coincide in length must still be walked.
static bool skip_is_fixed(const Type *t) {
  switch (t->kind) {
  case TypeKind_U8:
  case TypeKind_U16:
  case TypeKind_U32:
  case TypeKind_U64:
  case TypeKind_I8:
  case TypeKind_I16:
  case TypeKind_I32:
  case TypeKind_I64:
  case TypeKind_F32:
  case TypeKind_F64:
  case TypeKind_BOOL:
  case TypeKind_VOID:
    return true;
  case TypeKind_DATA:
    return t->data.length.has_value;
  case TypeKind_LIST:
    return (bool)(t->list.length.has_value && skip_is_fixed(t->list.elem));
  case TypeKind_STRUCT:
    for (size_t i = 0; i < t->struct_fields.len; i++) {
      if (!skip_is_fixed(t->struct_fields.fields[i].type)) {
        return false;
      }
    }
    return true;
  case TypeKind_USER:
    assert(t->user.resolved != nullptr && "check_schema resolves user references");
    return skip_is_fixed(t->user.resolved);
  default:
    return false;
  }
}

static void emit_fixed_skip(const Gen *g, u64 n, int indent) {
  if (n == 0) {
    return;
  }
  codegen_indent(g->out, indent);
  strbuf_appendf(g->out, "BARE_TRY(bare_reader_skip(r, %s));\n", codegen_u64_lit(n).text);
}

static void emit_skip_call(const Gen *g, const char *cname, int indent) {
  char *fn = codegen_type_fn_name(g, cname, "skip");
  codegen_indent(g->out, indent);
  strbuf_appendf(g->out, "BARE_TRY(%s(r));\n", fn);
  free(fn);
}

static void emit_skip_prefixed(const Gen *g, const Type *elem, int indent, int depth) {
  StrBuf *out = g->out;
  codegen_indent(out, indent);
  strbuf_append(out, "{\n");
  codegen_indent(out, indent + 2);
  strbuf_appendf(out, "uint64_t n%d;\n", depth);
  codegen_indent(out, indent + 2);
  strbuf_appendf(out, "BARE_TRY(bare_read_uint(r, &n%d));\n", depth);
  if (elem == nullptr) {
    codegen_indent(out, indent + 2);
    strbuf_appendf(out, "BARE_TRY(bare_reader_skip(r, n%d));\n", depth);
  } else {
    codegen_indent(out, indent + 2);
    strbuf_appendf(out, "for (uint64_t i%d = 0; i%d < n%d; i%d++) {\n", depth, depth, depth, depth);
    emit_skip_step(g, elem, indent + 4, depth + 1);
    codegen_indent(out, indent + 2);
    strbuf_append(out, "}\n");
  }
  codegen_indent(out, indent);
  strbuf_append(out, "}\n");
}

static void emit_skip_step(const Gen *g, const Type *t, int indent, int depth) {
  StrBuf *out = g->out;
  if (skip_is_fixed(t)) {
    emit_fixed_skip(g, codegen_wire_size(g, t).max, indent);
    return;
  }
  switch (t->kind) {
  case TypeKind_UINT:
  case TypeKind_INT:
  case TypeKind_ENUM:
    codegen_indent(out, indent);
    strbuf_append(out, "BARE_TRY(bare_skip_uint(r));\n");
    break;
  case TypeKind_STR:
  case TypeKind_DATA:
    assert(t->kind == TypeKind_STR ||
           (!t->data.length.has_value && "fixed data collapses to a fixed skip"));
    emit_skip_prefixed(g, nullptr, indent, depth);
    break;
  case TypeKind_USER: {
    char *cname = codegen_render_type_name(g->cfg, t->user.name);
    emit_skip_call(g, cname, indent);
    free(cname);
    break;
  }
  case TypeKind_UNION:
    emit_skip_call(g, codegen_name_of(g, t), indent);
    break;
  case TypeKind_OPTIONAL:
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
    strbuf_appendf(out, "if (p%d == 1) {\n", depth);
    emit_skip_step(g, t->optional.inner, indent + 4, depth + 1);
    codegen_indent(out, indent + 2);
    strbuf_append(out, "}\n");
    codegen_indent(out, indent);
    strbuf_append(out, "}\n");
    break;
  case TypeKind_STRUCT:
    for (size_t i = 0; i < t->struct_fields.len; i++) {
      emit_skip_step(g, t->struct_fields.fields[i].type, indent, depth);
    }
    break;
  case TypeKind_LIST:
    if (t->list.length.has_value) {
      codegen_indent(out, indent);
      strbuf_appendf(out, "for (uint64_t i%d = 0; i%d < %s; i%d++) {\n", depth, depth,
                     codegen_u64_lit(t->list.length.value).text, depth);
      emit_skip_step(g, t->list.elem, indent + 2, depth + 1);
      codegen_indent(out, indent);
      strbuf_append(out, "}\n");
    } else {
      emit_skip_prefixed(g, t->list.elem, indent, depth);
    }
    break;
  case TypeKind_MAP:
    codegen_indent(out, indent);
    strbuf_append(out, "{\n");
    codegen_indent(out, indent + 2);
    strbuf_appendf(out, "uint64_t n%d;\n", depth);
    codegen_indent(out, indent + 2);
    strbuf_appendf(out, "BARE_TRY(bare_read_uint(r, &n%d));\n", depth);
    codegen_indent(out, indent + 2);
    strbuf_appendf(out, "for (uint64_t i%d = 0; i%d < n%d; i%d++) {\n", depth, depth, depth, depth);
    emit_skip_step(g, t->map.key, indent + 4, depth + 1);
    emit_skip_step(g, t->map.value, indent + 4, depth + 1);
    codegen_indent(out, indent + 2);
    strbuf_append(out, "}\n");
    codegen_indent(out, indent);
    strbuf_append(out, "}\n");
    break;
  case TypeKind_VOID:
  default:
    UNREACHABLE();
  }
}

static void emit_union_skip_body(const Gen *g, const Type *t) {
  StrBuf *out = g->out;
  strbuf_append(out, "  uint64_t tag;\n  BARE_TRY(bare_read_uint(r, &tag));\n  switch (tag) {\n");
  for (size_t i = 0; i < t->union_members.len; i++) {
    const UnionMember *m = &t->union_members.members[i];
    assert(m->tag.has_value && "check_schema assigns implicit union tags");
    strbuf_appendf(out, "  case %s:\n", codegen_u64_lit(m->tag.value).text);
    emit_skip_step(g, m->type, 4, 0);
    strbuf_append(out, "    break;\n");
  }
  strbuf_append(out, "  default:\n    return BareStatus_INVALID_TAG;\n  }\n"
                     "  return BareStatus_OK;\n");
}

void codegen_emit_skip_fn(const Gen *g, const Type *t, const char *cname, bool is_public) {
  StrBuf *out = g->out;
  char *fn = codegen_type_fn_name(g, cname, "skip");
  if (!is_public) {
    strbuf_append(out, "static ");
  }
  strbuf_appendf(out, "BareStatus %s(BareReader *r) {\n", fn);
  free(fn);
  if (t->kind == TypeKind_USER) {
    char *target = codegen_render_type_name(g->cfg, t->user.name);
    char *target_fn = codegen_type_fn_name(g, target, "skip");
    strbuf_appendf(out, "  return %s(r);\n", target_fn);
    free(target_fn);
    free(target);
  } else if (t->kind == TypeKind_UNION) {
    emit_union_skip_body(g, t);
  } else {
    emit_skip_step(g, t, 2, 0);
    strbuf_append(out, "  return BareStatus_OK;\n");
  }
  strbuf_append(out, "}\n\n");
}
