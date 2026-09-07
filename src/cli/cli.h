#pragma once

#include "util/types.h"

typedef enum : u8 {
  CliCommand_GENERATE,
  CliCommand_CHECK,
} CliCommand;

/// Arguments for barec. GENERATE emits <name>.h/<name>.c for a schema into
/// out_dir, CHECK only parses and validates. config_path is empty when no
/// config file was given, name defaults to the schema file's stem.
typedef struct {
  CliCommand command;
  union {
    struct {
      const char *schema_path;
      const char *out_dir;
      const char *config_path;
      const char *name;
    } generate;
    struct {
      const char *schema_path;
    } check;
  };
} Cli;

/// Parses argv, or prints a usage message and exits on invalid arguments or
/// --help.
Cli cli_parse_args(int argc, char **argv);
