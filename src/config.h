#pragma once

#include "diag.h"
#include "types.h"

#include <stddef.h>

typedef enum : u8 {
  CaseStyle_SNAKE,
  CaseStyle_CAMEL,
  CaseStyle_PASCAL,
} CaseStyle;

typedef enum : u8 {
  EnumVariantStyle_TYPE_UPPER,
  EnumVariantStyle_UPPER,
  EnumVariantStyle_TYPE_PASCAL,
} EnumVariantStyle;

/// path addresses a schema element as Type.field, with .key/.value for map
/// sides and .item for list elements, e.g. "Customer.orders" or
/// "Employee.metadata.key".
typedef struct {
  Str path;
  u32 cap;
} CapOverride;

typedef struct {
  CaseStyle type_case;
  CaseStyle field_case;
  CaseStyle function_case;
  EnumVariantStyle enum_variant_style;
  Str prefix;
  u32 str_cap;
  u32 data_cap;
  u32 list_cap;
  u32 map_cap;
  CapOverride *overrides;
  size_t overrides_len;
} Config;

/// Style-guide defaults: pascal types, snake fields and functions,
/// Type_UPPER enum variants, no prefix, caps str/data 64 and list/map 8.
Config config_default(void);

/// Loads an INI-style config file over *cfg, which must already hold
/// defaults. Unset keys keep their current value.
[[nodiscard]] bool config_load(Config *cfg, const char *path, Diag *diag);
void config_free(Config *cfg);
