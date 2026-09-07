#pragma once

#include "backend/config.h"
#include "util/optional.h"
#include "util/types.h"

typedef enum : u8 {
  CliCommand_GENERATE,
  CliCommand_CHECK,
  CliCommand_CONFIG,
} CliCommand;

/// Parsed command line. GENERATE fields left null fall back to: out_dir the
/// schema's directory, config_path auto-discovery of barec.conf next to the
/// schema, name the schema filename stem. std and prefix override the
/// config file when given. runtime also writes bare.h/bare.c to out_dir.
typedef struct {
  CliCommand command;
  union {
    struct {
      const char *schema_path;
      const char *_Nullable out_dir;
      const char *_Nullable config_path;
      const char *_Nullable name;
      const char *_Nullable prefix;
      OPTIONAL(CStd) std;
      bool runtime;
    } generate;
    struct {
      const char *schema_path;
    } check;
  };
} Cli;

/// Parses argv. Prints help or version and exits 0 on -h/-V, prints a
/// usage error and exits 2 on invalid arguments.
Cli cli_parse_args(int argc, char **argv);

/// The program name for messages, the basename of argv[0] once
/// cli_parse_args has run.
const char *cli_prog_name(void);
