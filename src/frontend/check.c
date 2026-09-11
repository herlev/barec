#include "frontend/check.h"

#include "frontend/schema.h"
#include "util/diag.h"
#include "util/optional.h"
#include "util/types.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
  const Schema *schema;
  size_t defined;
  Diag *diag;
} Checker;

static bool is_ultimately_void(const Type *type) {
  return type_underlying(type)->kind == TypeKind_VOID;
}

static bool type_equal(const Type *a, const Type *b);

static bool length_equal(OPTIONAL(u64) a, OPTIONAL(u64) b) {
  return (bool)(a.has_value == b.has_value && (!a.has_value || a.value == b.value));
}

static bool enum_values_equal(const Type *a, const Type *b) {
  if (a->enum_values.len != b->enum_values.len) {
    return false;
  }
  for (size_t i = 0; i < a->enum_values.len; i++) {
    const EnumValue *va = &a->enum_values.values[i];
    const EnumValue *vb = &b->enum_values.values[i];
    if (!str_eq(va->name, vb->name) || va->value.value != vb->value.value) {
      return false;
    }
  }
  return true;
}

static bool union_members_equal(const Type *a, const Type *b) {
  if (a->union_members.len != b->union_members.len) {
    return false;
  }
  for (size_t i = 0; i < a->union_members.len; i++) {
    const UnionMember *ma = &a->union_members.members[i];
    const UnionMember *mb = &b->union_members.members[i];
    assert(ma->tag.has_value && mb->tag.has_value &&
           "members are checked before the duplicate comparison");
    if (ma->tag.value != mb->tag.value || !type_equal(ma->type, mb->type)) {
      return false;
    }
  }
  return true;
}

static bool struct_fields_equal(const Type *a, const Type *b) {
  if (a->struct_fields.len != b->struct_fields.len) {
    return false;
  }
  for (size_t i = 0; i < a->struct_fields.len; i++) {
    const StructField *fa = &a->struct_fields.fields[i];
    const StructField *fb = &b->struct_fields.fields[i];
    if (!str_eq(fa->name, fb->name) || !type_equal(fa->type, fb->type)) {
      return false;
    }
  }
  return true;
}

static bool type_equal(const Type *a, const Type *b) {
  if (a->kind != b->kind) {
    return false;
  }
  switch (a->kind) {
  case TypeKind_USER:
    return str_eq(a->user.name, b->user.name);
  case TypeKind_DATA:
    return length_equal(a->data.length, b->data.length);
  case TypeKind_OPTIONAL:
    return type_equal(a->optional.inner, b->optional.inner);
  case TypeKind_LIST:
    return (bool)(length_equal(a->list.length, b->list.length) &&
                  type_equal(a->list.elem, b->list.elem));
  case TypeKind_MAP:
    return (bool)(type_equal(a->map.key, b->map.key) && type_equal(a->map.value, b->map.value));
  case TypeKind_ENUM:
    return enum_values_equal(a, b);
  case TypeKind_UNION:
    return union_members_equal(a, b);
  case TypeKind_STRUCT:
    return struct_fields_equal(a, b);
  default:
    return true;
  }
}

[[nodiscard]] static bool assign_next(Checker *ctx, SrcLoc loc, OPTIONAL(u64) * slot, bool *first,
                                      u64 *prev, const char *what) {
  if (!slot->has_value) {
    if (*first) {
      slot->value = 0;
    } else if (*prev == UINT64_MAX) {
      diag_set(ctx->diag, loc, "implicit %s overflows 64 bits", what);
      return false;
    } else {
      slot->value = *prev + 1;
    }
    slot->has_value = true;
  } else if (!*first && slot->value <= *prev) {
    diag_set(ctx->diag, loc, "%ss must be in ascending order", what);
    return false;
  }
  *prev = slot->value;
  *first = false;
  return true;
}

[[nodiscard]] static bool check_type(Checker *ctx, Type *type);

[[nodiscard]] static bool resolve_user(Checker *ctx, Type *type) {
  Str name = type->user.name;
  for (size_t i = 0; i < ctx->defined; i++) {
    if (str_eq(ctx->schema->types[i].name, name)) {
      type->user.resolved = ctx->schema->types[i].type;
      return true;
    }
  }
  for (size_t i = ctx->defined; i < ctx->schema->len; i++) {
    if (str_eq(ctx->schema->types[i].name, name)) {
      if (i == ctx->defined) {
        diag_set(ctx->diag, type->loc, "recursive type definitions are not allowed");
      } else {
        diag_set(ctx->diag, type->loc, "type '%.*s' is used before its definition", (int)name.len,
                 name.data);
      }
      return false;
    }
  }
  diag_set(ctx->diag, type->loc, "unknown type '%.*s'", (int)name.len, name.data);
  return false;
}

[[nodiscard]] static bool check_enum(Checker *ctx, Type *type) {
  EnumValue *values = type->enum_values.values;
  bool first = true;
  u64 prev = 0;
  for (size_t i = 0; i < type->enum_values.len; i++) {
    for (size_t j = 0; j < i; j++) {
      if (str_eq(values[j].name, values[i].name)) {
        diag_set(ctx->diag, values[i].loc, "duplicate enum value name '%.*s'",
                 (int)values[i].name.len, values[i].name.data);
        return false;
      }
    }
    if (!assign_next(ctx, values[i].loc, &values[i].value, &first, &prev, "enum value")) {
      return false;
    }
  }
  return true;
}

[[nodiscard]] static bool check_union(Checker *ctx, Type *type) {
  UnionMember *members = type->union_members.members;
  bool first = true;
  u64 prev = 0;
  for (size_t i = 0; i < type->union_members.len; i++) {
    if (!check_type(ctx, members[i].type)) {
      return false;
    }
    if (!assign_next(ctx, members[i].type->loc, &members[i].tag, &first, &prev, "union tag")) {
      return false;
    }
  }
  for (size_t i = 0; i < type->union_members.len; i++) {
    for (size_t j = 0; j < i; j++) {
      if (type_equal(members[j].type, members[i].type)) {
        diag_set(ctx->diag, members[i].type->loc, "duplicate union member type");
        return false;
      }
    }
  }
  return true;
}

[[nodiscard]] static bool check_struct(Checker *ctx, Type *type) {
  StructField *fields = type->struct_fields.fields;
  for (size_t i = 0; i < type->struct_fields.len; i++) {
    for (size_t j = 0; j < i; j++) {
      if (str_eq(fields[j].name, fields[i].name)) {
        diag_set(ctx->diag, fields[i].loc, "duplicate field name '%.*s'", (int)fields[i].name.len,
                 fields[i].name.data);
        return false;
      }
    }
    if (!check_type(ctx, fields[i].type)) {
      return false;
    }
    if (is_ultimately_void(fields[i].type)) {
      diag_set(ctx->diag, fields[i].type->loc, "void is not allowed as a struct field type");
      return false;
    }
  }
  return true;
}

[[nodiscard]] static bool check_map_key(Checker *ctx, const Type *key) {
  switch (type_underlying(key)->kind) {
  case TypeKind_F32:
  case TypeKind_F64:
    diag_set(ctx->diag, key->loc, "floating-point types are not allowed as map keys");
    return false;
  case TypeKind_VOID:
    diag_set(ctx->diag, key->loc, "void is not allowed as a map key");
    return false;
  case TypeKind_OPTIONAL:
  case TypeKind_LIST:
  case TypeKind_MAP:
  case TypeKind_UNION:
  case TypeKind_STRUCT:
    diag_set(ctx->diag, key->loc, "map keys must be a primitive type");
    return false;
  default:
    return true;
  }
}

[[nodiscard]] static bool check_fixed_length(Checker *ctx, SrcLoc loc, OPTIONAL(u64) length) {
  if (length.has_value && length.value == 0) {
    diag_set(ctx->diag, loc, "fixed length must be at least one");
    return false;
  }
  return true;
}

[[nodiscard]] static bool check_type(Checker *ctx, Type *type) {
  switch (type->kind) {
  case TypeKind_USER:
    return resolve_user(ctx, type);
  case TypeKind_DATA:
    return check_fixed_length(ctx, type->loc, type->data.length);
  case TypeKind_OPTIONAL:
    if (!check_type(ctx, type->optional.inner)) {
      return false;
    }
    if (is_ultimately_void(type->optional.inner)) {
      diag_set(ctx->diag, type->optional.inner->loc, "void is not allowed as an optional type");
      return false;
    }
    return true;
  case TypeKind_LIST:
    if (!check_type(ctx, type->list.elem)) {
      return false;
    }
    if (is_ultimately_void(type->list.elem)) {
      diag_set(ctx->diag, type->list.elem->loc, "void is not allowed as a list element type");
      return false;
    }
    return check_fixed_length(ctx, type->loc, type->list.length);
  case TypeKind_MAP:
    if (!check_type(ctx, type->map.key) || !check_type(ctx, type->map.value)) {
      return false;
    }
    if (!check_map_key(ctx, type->map.key)) {
      return false;
    }
    if (is_ultimately_void(type->map.value)) {
      diag_set(ctx->diag, type->map.value->loc, "void is not allowed as a map value type");
      return false;
    }
    return true;
  case TypeKind_ENUM:
    return check_enum(ctx, type);
  case TypeKind_UNION:
    return check_union(ctx, type);
  case TypeKind_STRUCT:
    return check_struct(ctx, type);
  default:
    return true;
  }
}

bool check_schema(Schema *schema, Diag *diag) {
  Checker ctx = {.schema = schema, .diag = diag};
  for (size_t i = 0; i < schema->len; i++) {
    for (size_t j = 0; j < i; j++) {
      if (str_eq(schema->types[j].name, schema->types[i].name)) {
        diag_set(diag, schema->types[i].loc, "redefinition of type '%.*s'",
                 (int)schema->types[i].name.len, schema->types[i].name.data);
        return false;
      }
    }
    ctx.defined = i;
    if (!check_type(&ctx, schema->types[i].type)) {
      return false;
    }
  }
  return true;
}
