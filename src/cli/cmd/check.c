#include "cli/cmd.h"

#include "cli/cli.h"
#include "cli/cmd/common.h"
#include "frontend/schema.h"

#include <stdlib.h>

int cmd_check(const Cli *cli) {
  char *schema_text = nullptr;
  Schema schema = {};
  if (!cmd_load_schema(cli->check.schema_path, &schema_text, &schema)) {
    return 1;
  }
  schema_free(&schema);
  free(schema_text);
  return 0;
}
