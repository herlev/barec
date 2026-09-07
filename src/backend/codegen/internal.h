#pragma once

#include "backend/config.h"
#include "frontend/schema.h"
#include "util/diag.h"
#include "util/strbuf.h"
#include "util/types.h"
#include "util/vec.h"

#include <stddef.h>

typedef char *GenName;
typedef const char *GenNameView;

typedef struct {
  const Type *type;
  char *name;
} TypeName;

typedef struct {
  const Type *type;
  u32 cap;
} SiteCap;

typedef struct {
  const Type *type;
  VEC(GenName) bases;
} UnionBases;

/// Shared state across the codegen passes. The scan pass fills the side
/// tables (names for aggregates, tag names and member base names for
/// unions, resolved caps per site), the emit pass reads them.
typedef struct {
  const Schema *schema;
  const Config *cfg;
  Diag *diag;
  StrBuf *out;
  VEC(TypeName) names;
  VEC(TypeName) tag_names;
  VEC(UnionBases) union_bases;
  VEC(SiteCap) caps;
  VEC(u32) str_caps;
  VEC(u32) data_caps;
  bool *override_used;
} Gen;

static inline bool codegen_is_aggregate(TypeKind kind) {
  return (bool)(kind == TypeKind_ENUM || kind == TypeKind_STRUCT || kind == TypeKind_UNION);
}

/// Renders a schema name or dotted path into a C identifier: optional
/// config prefix, dots to underscores, re-cased via name_render, C keywords
/// escaped with a trailing underscore. Returns an owned string.
char *codegen_render_ident(const Config *cfg, Str raw, CaseStyle style, bool with_prefix);
char *codegen_render_ident_cstr(const Config *cfg, const char *raw, CaseStyle style,
                                bool with_prefix);

/// Walks a user type assigning derived names and resolving caps, keyed by
/// the dotted override path rooted at the user type's name.
void codegen_scan_type(Gen *g, const Type *t, StrBuf *path);

const char *codegen_name_of(const Gen *g, const Type *t);
const char *codegen_tag_name_of(const Gen *g, const Type *t);
u32 codegen_cap_of(const Gen *g, const Type *t);
const VEC(GenName) * codegen_bases_of(const Gen *g, const Type *t);

/// Emits one struct member declaration for a type, recursing through
/// anonymous aggregates. dims accumulates array suffixes from enclosing
/// fixed-length lists.
void codegen_emit_member(Gen *g, const Type *t, const char *name, const char *dims, int indent);

/// Emits named definitions for anonymous aggregates in post-order so inner
/// types precede their users. The root definition itself is skipped.
void codegen_emit_derived_defs(Gen *g, const Type *t, bool is_root);

void codegen_emit_root_def(Gen *g, const UserType *ut, const char *cname);
void codegen_emit_shared_typedefs(Gen *g);
