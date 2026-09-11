#pragma once

#include "cli/cli.h"

/// One entry point per CLI command, each returning the process exit code.
int cmd_generate(const Cli *cli);
int cmd_check(const Cli *cli);
int cmd_config(void);
int cmd_hash(const Cli *cli);
