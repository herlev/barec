#include "cli/cli.h"
#include "cli/cmd.h"

int main(int argc, char **argv) {
  Cli cli = cli_parse_args(argc, argv);
  switch (cli.command) {
  case CliCommand_GENERATE:
    return cmd_generate(&cli);
  case CliCommand_CHECK:
    return cmd_check(&cli);
  case CliCommand_CONFIG:
    return cmd_config();
  }
}
