#include "frontend/schema.h"

#include <stddef.h>
#include <stdlib.h>

const Type *type_underlying(const Type *type) {
  while (type->kind == TypeKind_USER) {
    type = type->user.resolved;
  }
  return type;
}

void type_free(Type *type) {
  if (type == nullptr) {
    return;
  }
  switch (type->kind) {
  case TypeKind_ENUM:
    free(type->enum_values.values);
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
      type_free(type->union_members.members[i].type);
    }
    free(type->union_members.members);
    break;
  case TypeKind_STRUCT:
    for (size_t i = 0; i < type->struct_fields.len; i++) {
      type_free(type->struct_fields.fields[i].type);
    }
    free(type->struct_fields.fields);
    break;
  default:
    break;
  }
  free(type);
}

void schema_free(Schema *schema) {
  for (size_t i = 0; i < schema->len; i++) {
    type_free(schema->types[i].type);
  }
  free(schema->types);
  *schema = (Schema){};
}
