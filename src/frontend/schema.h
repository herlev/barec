#pragma once

#include "util/diag.h"
#include "util/optional.h"
#include "util/types.h"

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

typedef struct {
  Str name;
  OPTIONAL(u64) value;
  SrcLoc loc;
} EnumValue;

typedef struct {
  Type *type;
  OPTIONAL(u64) tag;
} UnionMember;

typedef struct {
  Str name;
  Type *type;
  SrcLoc loc;
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
    struct {
      EnumValue *values;
      size_t len;
    } enum_values;
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
    struct {
      UnionMember *members;
      size_t len;
    } union_members;
    struct {
      StructField *fields;
      size_t len;
    } struct_fields;
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
} UserType;

typedef struct {
  UserType *types;
  size_t len;
} Schema;

/// Follows user-type references to the concrete type. Valid only after
/// check_schema has resolved the tree.
const Type *type_underlying(const Type *type);

void type_free(Type *type);
void schema_free(Schema *schema);
