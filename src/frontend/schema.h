#pragma once

#include "util/diag.h"
#include "util/optional.h"
#include "util/types.h"
#include "util/vec.h"

#include <stddef.h>

typedef enum : u8 {
  TypeKind_UINT,
  TypeKind_U8,
  TypeKind_U16,
  TypeKind_U32,
  TypeKind_U64,
  TypeKind_INT,
  TypeKind_I8,
  TypeKind_I16,
  TypeKind_I32,
  TypeKind_I64,
  TypeKind_F32,
  TypeKind_F64,
  TypeKind_BOOL,
  TypeKind_STR,
  TypeKind_DATA,
  TypeKind_VOID,
  TypeKind_ENUM,
  TypeKind_OPTIONAL,
  TypeKind_LIST,
  TypeKind_MAP,
  TypeKind_UNION,
  TypeKind_STRUCT,
  TypeKind_USER,
} TypeKind;

typedef struct Type Type;

/// Comments attached to a declaration: the block on the lines directly
/// above it and the trailing comment on its line, if any. The lines view
/// the schema source buffer, the above array is owned.
typedef struct {
  SLICE(Str) above;
  OPTIONAL(Str) trailing;
} Doc;

typedef struct {
  Str name;
  OPTIONAL(u64) value;
  SrcLoc loc;
  Doc doc;
} EnumValue;

typedef struct {
  Type *type;
  OPTIONAL(u64) tag;
} UnionMember;

typedef struct {
  Str name;
  Type *type;
  SrcLoc loc;
  Doc doc;
} StructField;

/// The tagged tree for a single BARE type expression. All names are views
/// into the schema source buffer. Enum values and union tags left implicit
/// in the schema text hold their auto-assigned value after check_schema has
/// run, and user.resolved points at the referenced definition's type. data
/// and list carry a length that is present only for their fixed-length forms
/// data[n] and list<T>[n].
struct Type {
  TypeKind kind;
  union {
    struct {
      OPTIONAL(u64) length;
    } data;
    SLICE(EnumValue) enum_values;
    struct {
      Type *inner;
    } optional;
    struct {
      Type *elem;
      OPTIONAL(u64) length;
    } list;
    struct {
      Type *key;
      Type *value;
    } map;
    SLICE(UnionMember) union_members;
    SLICE(StructField) struct_fields;
    struct {
      Str name;
      Type *resolved;
    } user;
  };
  SrcLoc loc;
};

typedef struct {
  Str name;
  Type *type;
  SrcLoc loc;
  Doc doc;
} UserType;

typedef SLICE(UserType) Schema;

/// Follows user-type references to the concrete type. Valid only after
/// check_schema has resolved the tree.
const Type *type_underlying(const Type *type);

/// FNV1a-64 hash identifying the type's declared schema, for peers to
/// compare as a protocol identity. It covers everything the author wrote,
/// names included, and ignores only formatting, comments, and schema
/// position, so reordering or adding unrelated declarations changes no
/// hash while any rename does.
///
/// The canonical byte stream is built from two primitives, a single tag
/// byte and a u64 fed as 8 little-endian bytes. A name is its u64 length
/// followed by its ASCII bytes. The stream is one form version byte
/// (currently 1), then the root encoded as NAMED, name, definition. A
/// user-type reference encodes the same way, so alias chains feed every
/// name on the path. A struct is its tag, the u64 field count, then each
/// field's name and type. An enum is its tag, the count, then each
/// value's name and u64 value. A union is its tag, the count, then each
/// member's u64 tag value and type. Fixed data and lists use distinct
/// tags followed by the u64 length, and lists then feed their element
/// type. Scalars are a single tag byte. The
/// form is frozen once published, since peers compare hashes across
/// builds and implementations. Valid only after check_schema.
///
/// For example, given
///
///     type Celcius u32
///     type Reading struct {
///       temperature: Celcius
///       ok: bool
///     }
///
/// the hash of Reading digests the bytes
///
///     01                     form version
///     19 07 00·· "Reading"   NAMED, the root's own name
///     18 02 00··             STRUCT, 2 fields
///     0b 00·· "temperature"  field name
///     19 07 00·· "Celcius"   the reference feeds its name
///     04                     then Celcius's definition, U32
///     02 00·· "ok"           field name
///     0d                     BOOL
///
/// and the hash of Celcius digests 01, then NAMED "Celcius", then 04,
/// exactly the stream a reference to it embeds.
u64 type_wire_hash(const UserType *type);

void type_free(Type *type);
void schema_free(Schema *schema);
