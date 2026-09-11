#include "frontend/schema.h"

#include "util/macros.h"
#include "util/types.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

const Type *type_underlying(const Type *type) {
  while (type->kind == TypeKind_USER) {
    type = type->user.resolved;
  }
  return type;
}

/// Version byte fed before anything else, so a future revision of the
/// canonical form can never collide with hashes published under this one.
enum : u8 { WIRE_HASH_FORM = 1 };

/// Canonical wire-hash tags, deliberately independent of TypeKind so enum
/// reorders can never silently change published hashes.
enum : u8 {
  WireHash_UINT = 1,
  WireHash_U8 = 2,
  WireHash_U16 = 3,
  WireHash_U32 = 4,
  WireHash_U64 = 5,
  WireHash_INT = 6,
  WireHash_I8 = 7,
  WireHash_I16 = 8,
  WireHash_I32 = 9,
  WireHash_I64 = 10,
  WireHash_F32 = 11,
  WireHash_F64 = 12,
  WireHash_BOOL = 13,
  WireHash_STR = 14,
  WireHash_DATA = 15,
  WireHash_DATA_FIXED = 16,
  WireHash_VOID = 17,
  WireHash_ENUM = 18,
  WireHash_OPTIONAL = 19,
  WireHash_LIST = 20,
  WireHash_LIST_FIXED = 21,
  WireHash_MAP = 22,
  WireHash_UNION = 23,
  WireHash_STRUCT = 24,
  WireHash_NAMED = 25,
};

enum : u64 {
  FNV_OFFSET_BASIS = UINT64_C(0xcbf29ce484222325),
  FNV_PRIME = UINT64_C(0x100000001b3),
};

static u64 fnv1a_bytes(u64 hash, const void *data, size_t len) {
  const u8 *bytes = data;
  for (size_t i = 0; i < len; i++) {
    hash ^= bytes[i];
    hash *= FNV_PRIME;
  }
  return hash;
}

static u64 fnv1a_tag(u64 hash, u8 tag) { return fnv1a_bytes(hash, &tag, sizeof(tag)); }

static u64 fnv1a_u64(u64 hash, u64 value) {
  u8 bytes[8];
  for (size_t i = 0; i < sizeof(bytes); i++) {
    bytes[i] = (value >> (8 * i)) & 0xff;
  }
  return fnv1a_bytes(hash, bytes, sizeof(bytes));
}

static u64 fnv1a_name(u64 hash, Str name) {
  hash = fnv1a_u64(hash, name.len);
  return fnv1a_bytes(hash, name.data, name.len);
}

static u64 hash_type(u64 hash, const Type *type) {
  static const u8 PRIMITIVE_TAG[] = {
      [TypeKind_UINT] = WireHash_UINT, [TypeKind_U8] = WireHash_U8,
      [TypeKind_U16] = WireHash_U16,   [TypeKind_U32] = WireHash_U32,
      [TypeKind_U64] = WireHash_U64,   [TypeKind_INT] = WireHash_INT,
      [TypeKind_I8] = WireHash_I8,     [TypeKind_I16] = WireHash_I16,
      [TypeKind_I32] = WireHash_I32,   [TypeKind_I64] = WireHash_I64,
      [TypeKind_F32] = WireHash_F32,   [TypeKind_F64] = WireHash_F64,
      [TypeKind_BOOL] = WireHash_BOOL, [TypeKind_STR] = WireHash_STR,
      [TypeKind_VOID] = WireHash_VOID,
  };
  switch (type->kind) {
  case TypeKind_USER:
    assert(type->user.resolved != nullptr && "check_schema resolves user references");
    hash = fnv1a_tag(hash, WireHash_NAMED);
    hash = fnv1a_name(hash, type->user.name);
    return hash_type(hash, type->user.resolved);
  case TypeKind_DATA:
    if (type->data.length.has_value) {
      hash = fnv1a_tag(hash, WireHash_DATA_FIXED);
      return fnv1a_u64(hash, type->data.length.value);
    }
    return fnv1a_tag(hash, WireHash_DATA);
  case TypeKind_ENUM:
    hash = fnv1a_tag(hash, WireHash_ENUM);
    hash = fnv1a_u64(hash, type->enum_values.len);
    for (size_t i = 0; i < type->enum_values.len; i++) {
      assert(type->enum_values.ptr[i].value.has_value &&
             "check_schema assigns implicit enum values");
      hash = fnv1a_name(hash, type->enum_values.ptr[i].name);
      hash = fnv1a_u64(hash, type->enum_values.ptr[i].value.value);
    }
    return hash;
  case TypeKind_OPTIONAL:
    hash = fnv1a_tag(hash, WireHash_OPTIONAL);
    return hash_type(hash, type->optional.inner);
  case TypeKind_LIST:
    if (type->list.length.has_value) {
      hash = fnv1a_tag(hash, WireHash_LIST_FIXED);
      hash = fnv1a_u64(hash, type->list.length.value);
    } else {
      hash = fnv1a_tag(hash, WireHash_LIST);
    }
    return hash_type(hash, type->list.elem);
  case TypeKind_MAP:
    hash = fnv1a_tag(hash, WireHash_MAP);
    hash = hash_type(hash, type->map.key);
    return hash_type(hash, type->map.value);
  case TypeKind_UNION:
    hash = fnv1a_tag(hash, WireHash_UNION);
    hash = fnv1a_u64(hash, type->union_members.len);
    for (size_t i = 0; i < type->union_members.len; i++) {
      assert(type->union_members.ptr[i].tag.has_value &&
             "check_schema assigns implicit union tags");
      hash = fnv1a_u64(hash, type->union_members.ptr[i].tag.value);
      hash = hash_type(hash, type->union_members.ptr[i].type);
    }
    return hash;
  case TypeKind_STRUCT:
    hash = fnv1a_tag(hash, WireHash_STRUCT);
    hash = fnv1a_u64(hash, type->struct_fields.len);
    for (size_t i = 0; i < type->struct_fields.len; i++) {
      hash = fnv1a_name(hash, type->struct_fields.ptr[i].name);
      hash = hash_type(hash, type->struct_fields.ptr[i].type);
    }
    return hash;
  case TypeKind_UINT:
  case TypeKind_U8:
  case TypeKind_U16:
  case TypeKind_U32:
  case TypeKind_U64:
  case TypeKind_INT:
  case TypeKind_I8:
  case TypeKind_I16:
  case TypeKind_I32:
  case TypeKind_I64:
  case TypeKind_F32:
  case TypeKind_F64:
  case TypeKind_BOOL:
  case TypeKind_STR:
  case TypeKind_VOID:
    return fnv1a_tag(hash, PRIMITIVE_TAG[type->kind]);
  }
  UNREACHABLE();
}

u64 type_wire_hash(const UserType *type) {
  u64 hash = fnv1a_tag(FNV_OFFSET_BASIS, WIRE_HASH_FORM);
  hash = fnv1a_tag(hash, WireHash_NAMED);
  hash = fnv1a_name(hash, type->name);
  return hash_type(hash, type->type);
}

void type_free(Type *type) {
  if (type == nullptr) {
    return;
  }
  switch (type->kind) {
  case TypeKind_ENUM:
    for (size_t i = 0; i < type->enum_values.len; i++) {
      free(type->enum_values.ptr[i].doc.above.ptr);
    }
    free(type->enum_values.ptr);
    break;
  case TypeKind_OPTIONAL:
    type_free(type->optional.inner);
    break;
  case TypeKind_LIST:
    type_free(type->list.elem);
    break;
  case TypeKind_MAP:
    type_free(type->map.key);
    type_free(type->map.value);
    break;
  case TypeKind_UNION:
    for (size_t i = 0; i < type->union_members.len; i++) {
      type_free(type->union_members.ptr[i].type);
    }
    free(type->union_members.ptr);
    break;
  case TypeKind_STRUCT:
    for (size_t i = 0; i < type->struct_fields.len; i++) {
      type_free(type->struct_fields.ptr[i].type);
      free(type->struct_fields.ptr[i].doc.above.ptr);
    }
    free(type->struct_fields.ptr);
    break;
  default:
    break;
  }
  free(type);
}

void schema_free(Schema *schema) {
  for (size_t i = 0; i < schema->len; i++) {
    type_free(schema->ptr[i].type);
    free(schema->ptr[i].doc.above.ptr);
  }
  free(schema->ptr);
  *schema = (Schema){};
}
