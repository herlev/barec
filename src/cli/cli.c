#include "cli/cli.h"

#include "backend/config.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *prog_name = "barec";

static void print_help(FILE *stream) {
  (void)fprintf(stream, "%s - BARE schema compiler for C\n\n", prog_name);
  (void)fprintf(stream,
                "Usage:\n"
                "  %s generate <schema.bare> [options]\n"
                "  %s check <schema.bare>\n"
                "  %s config\n\n",
                prog_name, prog_name, prog_name);
  (void)fputs(
      "Commands:\n"
      "  generate  Generate C types and (de)serialization code from a schema\n"
      "  check     Parse and validate a schema, printing nothing on success\n"
      "  config    Print an annotated default config file to stdout\n"
      "\n"
      "Options:\n"
      "  -o, --out-dir <dir>   Directory for generated files  [default: schema's directory]\n"
      "  -n, --name <name>     Basename of the generated .h/.c pair  [default: schema stem]\n"
      "  -c, --config <file>   Config file  [default: barec.conf next to the schema, if "
      "present]\n"
      "      --std <c99|c23>   C standard of the generated code  [default: c23]\n"
      "      --prefix <name>   Prefix for all generated identifiers\n"
      "      --runtime         Also write the runtime (bare.h, bare.c) into the output "
      "directory\n"
      "  -h, --help            Print this help\n"
      "  -V, --version         Print version\n"
      "\n"
      "Settings apply in order: defaults, config file, flags.\n",
      stream);
  (void)fprintf(stream, "A schema path alone implies generate: %s device.bare\n", prog_name);
}

const char *cli_prog_name(void) { return prog_name; }

static void set_prog_name(int argc, char **argv) {
  if (argc < 1 || argv[0] == nullptr || argv[0][0] == '\0') {
    return;
  }
  const char *slash = strrchr(argv[0], '/');
  prog_name = slash != nullptr ? slash + 1 : argv[0];
}

[[noreturn]] [[gnu::format(printf, 1, 2)]] static void usage_error(const char *fmt, ...) {
  (void)fprintf(stderr, "%s: ", prog_name);
  va_list args;
  va_start(args, fmt);
  (void)vfprintf(stderr, fmt, args);
  va_end(args);
  (void)fprintf(stderr, "\ntry '%s --help'\n", prog_name);
  exit(2);
}

static bool opt_matches(const char *arg, const char *short_opt, const char *long_opt,
                        const char **inline_value) {
  *inline_value = nullptr;
  if (short_opt != nullptr && strcmp(arg, short_opt) == 0) {
    return true;
  }
  size_t n = strlen(long_opt);
  if (strncmp(arg, long_opt, n) == 0) {
    if (arg[n] == '\0') {
      return true;
    }
    if (arg[n] == '=') {
      *inline_value = arg + n + 1;
      return true;
    }
  }
  return false;
}

static const char *opt_value(int argc, char **argv, int *i, const char *inline_value,
                             const char *opt_name) {
  if (inline_value != nullptr) {
    return inline_value;
  }
  *i += 1;
  if (*i >= argc) {
    usage_error("option '%s' requires a value", opt_name);
  }
  return argv[*i];
}

static CStd parse_std(const char *value) {
  if (strcmp(value, "c99") == 0) {
    return CStd_C99;
  }
  if (strcmp(value, "c23") == 0) {
    return CStd_C23;
  }
  usage_error("invalid --std '%s', expected c99 or c23", value);
}

static void validate_prefix(const char *prefix) {
  for (size_t i = 0; prefix[i] != '\0'; i++) {
    char c = prefix[i];
    bool alpha = (bool)((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_');
    bool digit = (bool)(c >= '0' && c <= '9');
    if (!alpha && !(digit && i > 0)) {
      usage_error("invalid --prefix '%s', expected letters, digits, and underscores not starting "
                  "with a digit",
                  prefix);
    }
  }
}

static void parse_generate_option(Cli *cli, int argc, char **argv, int *i) {
  const char *arg = argv[*i];
  const char *inline_value = nullptr;
  if (opt_matches(arg, "-o", "--out-dir", &inline_value)) {
    cli->generate.out_dir = opt_value(argc, argv, i, inline_value, "--out-dir");
  } else if (opt_matches(arg, "-n", "--name", &inline_value)) {
    cli->generate.name = opt_value(argc, argv, i, inline_value, "--name");
  } else if (opt_matches(arg, "-c", "--config", &inline_value)) {
    cli->generate.config_path = opt_value(argc, argv, i, inline_value, "--config");
  } else if (opt_matches(arg, nullptr, "--std", &inline_value)) {
    cli->generate.std.value = parse_std(opt_value(argc, argv, i, inline_value, "--std"));
    cli->generate.std.has_value = true;
  } else if (opt_matches(arg, nullptr, "--prefix", &inline_value)) {
    const char *prefix = opt_value(argc, argv, i, inline_value, "--prefix");
    validate_prefix(prefix);
    cli->generate.prefix = prefix;
  } else if (strcmp(arg, "--runtime") == 0) {
    cli->generate.runtime = true;
  } else {
    usage_error("unknown option '%s'", arg);
  }
}

static void handle_help_version(int argc, char **argv) {
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
      print_help(stdout);
      exit(0);
    }
    if (strcmp(argv[i], "-V") == 0 || strcmp(argv[i], "--version") == 0) {
      printf("%s %s\n", prog_name, BAREC_VERSION);
      exit(0);
    }
  }
}

static void parse_command(Cli *cli, const char *arg) {
  if (strcmp(arg, "generate") == 0) {
    cli->command = CliCommand_GENERATE;
  } else if (strcmp(arg, "check") == 0) {
    cli->command = CliCommand_CHECK;
  } else if (strcmp(arg, "config") == 0) {
    cli->command = CliCommand_CONFIG;
  } else if (arg[0] != '-') {
    cli->command = CliCommand_GENERATE;
    cli->generate.schema_path = arg;
  } else {
    usage_error("expected a command or schema file, got '%s'", arg);
  }
}

Cli cli_parse_args(int argc, char **argv) {
  set_prog_name(argc, argv);
  handle_help_version(argc, argv);
  if (argc < 2) {
    print_help(stderr);
    exit(2);
  }
  Cli cli = {};
  parse_command(&cli, argv[1]);
  for (int i = 2; i < argc; i++) {
    const char *arg = argv[i];
    if (arg[0] != '-') {
      const char **slot = &cli.generate.schema_path;
      if (cli.command == CliCommand_CHECK) {
        slot = &cli.check.schema_path;
      } else if (cli.command == CliCommand_CONFIG) {
        usage_error("unexpected argument '%s'", arg);
      }
      if (*slot != nullptr) {
        usage_error("unexpected argument '%s'", arg);
      }
      *slot = arg;
      continue;
    }
    if (cli.command != CliCommand_GENERATE) {
      usage_error("unknown option '%s'", arg);
    }
    parse_generate_option(&cli, argc, argv, &i);
  }
  if (cli.command == CliCommand_GENERATE && cli.generate.schema_path == nullptr) {
    usage_error("missing schema file");
  }
  if (cli.command == CliCommand_CHECK && cli.check.schema_path == nullptr) {
    usage_error("missing schema file");
  }
  return cli;
}
