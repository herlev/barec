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
