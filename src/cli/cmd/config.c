#include "cli/cmd.h"

#include <stdio.h>

static const char CONFIG_TEMPLATE[] =
    "# barec configuration. Every key is optional, defaults shown.\n"
    "\n"
    "[naming]\n"
    "# snake | camel | pascal | screaming\n"
    "type_case = pascal\n"
    "field_case = snake\n"
    "function_case = snake\n"
    "# Type_UPPER | UPPER | Type_Pascal\n"
    "enum_variant = Type_UPPER\n"
    "# Prepended to every generated identifier\n"
    "prefix =\n"
    "\n"
    "[codegen]\n"
    "# c99 | c23\n"
    "std = c23\n"
    "\n"
    "# Fixed capacities for variable-length values, in elements (str and\n"
    "# data in octets)\n"
    "[caps]\n"
    "str = 64\n"
    "data = 64\n"
    "list = 8\n"
    "map = 8\n"
    "\n"
    "# Per-element capacity overrides, addressed as Type.field with .item\n"
    "# for list elements and .key/.value for map sides\n"
    "[caps.overrides]\n"
    "# Customer.orders = 16\n"
    "# Employee.metadata.key = 32\n";

int cmd_config(void) {
  fputs(CONFIG_TEMPLATE, stdout);
  return 0;
}
