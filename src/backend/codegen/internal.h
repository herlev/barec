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
  /// Invented member names rendered per field_case. The shared BareStrN
  /// and BareDataN typedefs keep literal data/len regardless, since the
  /// BARE_STR helper macros spell those members out.
  struct {
    char *has_value;
    char *value;
    char *items;
    char *len;
    char *entries;
    char *key;
    char *tag;
    char *data;
  } members;
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

/// C name for a user or derived type: prefix, type_case, then the
/// configured type suffix. Returns an owned string.
char *codegen_render_type_name(const Config *cfg, Str raw);

/// Length of cname without the configured type suffix. Function and enum
/// variant names derive from this base so the suffix stays on types only.
size_t codegen_type_base_len(const Config *cfg, const char *cname);

/// Function name for a generated type, derived from its C name so prefix
/// and casing carry over, e.g. ("AcmeCustomer", "read") -> acme_customer_read.
char *codegen_type_fn_name(const Gen *g, const char *cname, const char *op);

/// Enum variant name for a value raw name under the configured style.
char *codegen_render_variant(const Gen *g, const char *type_cname, Str raw);

/// The BARE spelling of a type kind (u8, str, list, ...), used for runtime
/// function suffixes and union member base names.
const char *codegen_bare_type_name(TypeKind kind);

typedef struct {
  char text[32];
} U64Lit;

/// Renders a value as a C integer constant, wrapped in UINT64_C when it
/// exceeds INT64_MAX since larger unsuffixed literals are not valid C.
U64Lit codegen_u64_lit(u64 value);

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
void codegen_emit_member(const Gen *g, const Type *t, const char *name, const char *dims,
                         int indent);

/// Emits named definitions for anonymous aggregates in post-order so inner
/// types precede their users. The root definition itself is skipped.
void codegen_emit_derived_defs(const Gen *g, const Type *t, bool is_root);

void codegen_emit_root_def(const Gen *g, const UserType *ut, const char *cname);
void codegen_emit_shared_typedefs(const Gen *g);
void codegen_indent(StrBuf *out, int indent);

/// Emit a complete read/write function definition for a named type. Derived
/// helper functions are static, root functions match the header decls.
void codegen_emit_read_fn(const Gen *g, const Type *t, const char *cname, bool is_public);
void codegen_emit_write_fn(const Gen *g, const Type *t, const char *cname, bool is_public);

/// Emits the whole generated source file into g->out: derived helpers in
/// post-order, then read/write/decode/encode per user type.
void codegen_emit_source_content(const Gen *g, char *const root_names[], const char *basename);
