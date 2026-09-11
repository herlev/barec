#include "cli/cmd.h"

#include "cli/cli.h"
#include "cli/cmd/common.h"
#include "frontend/schema.h"
#include "util/types.h"

#include <inttypes.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

int cmd_hash(const Cli *cli) {
  char *schema_text = nullptr;
  Schema schema = {};
  if (!cmd_load_schema(cli->hash.schema_path, &schema_text, &schema)) {
    return 1;
  }
  for (size_t i = 0; i < schema.len; i++) {
    const UserType *ut = &schema.ptr[i];
    printf("%.*s 0x%016" PRIx64 "\n", (int)ut->name.len, ut->name.data, type_wire_hash(ut));
  }
  schema_free(&schema);
  free(schema_text);
  return 0;
}
