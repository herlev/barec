#include "cli/cmd.h"

#include "backend/codegen.h"
#include "backend/config.h"
#include "cli/cli.h"
#include "cli/cmd/common.h"
#include "frontend/schema.h"
#include "util/diag.h"
#include "util/strbuf.h"
#include "util/types.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static const char RUNTIME_HEADER[] = {
#embed "bare.h"
};

static const char RUNTIME_SOURCE[] = {
#embed "bare.c"
};

static bool file_exists(const char *path) {
  FILE *file = fopen(path, "rb");
  if (file == nullptr) {
    return false;
  }
  fclose(file);
  return true;
}

static void dir_of(const char *path, StrBuf *out) {
  const char *slash = strrchr(path, '/');
  if (slash == nullptr) {
    strbuf_append(out, ".");
  } else if (slash == path) {
    strbuf_append(out, "/");
  } else {
    strbuf_append_str(out, (Str){.data = path, .len = (size_t)(slash - path)});
  }
}

static void stem_of(const char *path, StrBuf *out) {
  const char *slash = strrchr(path, '/');
  const char *base = slash != nullptr ? slash + 1 : path;
  const char *dot = strrchr(base, '.');
  size_t len = (dot != nullptr && dot != base) ? (size_t)(dot - base) : strlen(base);
  strbuf_append_str(out, (Str){.data = base, .len = len});
}

[[nodiscard]] static bool make_dirs(const char *path) {
  StrBuf partial = {};
  size_t n = strlen(path);
  bool ok = true;
  for (size_t i = 0; i <= n && ok; i++) {
    if ((i == n || path[i] == '/') && partial.len > 0) {
      if (mkdir(partial.data, 0755) != 0 && errno != EEXIST) {
        fprintf(stderr, "%s: error: cannot create directory '%s': %s\n", cli_prog_name(),
                partial.data, strerror(errno));
        ok = false;
      }
    }
    if (i < n) {
      strbuf_append_char(&partial, path[i]);
    }
  }
  strbuf_free(&partial);
  return ok;
}

[[nodiscard]] static bool write_output(const char *path, const char *data, size_t len) {
  FILE *file = fopen(path, "wb");
  if (file == nullptr) {
    fprintf(stderr, "%s: error: cannot write '%s': %s\n", cli_prog_name(), path, strerror(errno));
    return false;
  }
  bool ok = fwrite(data, 1, len, file) == len;
  int write_errno = errno;
  if (fclose(file) != 0) {
    write_errno = errno;
    ok = false;
  }
  if (!ok) {
    fprintf(stderr, "%s: error: cannot write '%s': %s\n", cli_prog_name(), path,
            strerror(write_errno));
  }
  return ok;
}

[[nodiscard]] static bool write_generated(const Cli *cli, const char *out_dir, const char *name,
                                          const StrBuf *header, const StrBuf *source) {
  if (!make_dirs(out_dir)) {
    return false;
  }
  StrBuf path = {};
  strbuf_appendf(&path, "%s/%s.h", out_dir, name);
  bool ok = write_output(path.data, header->data, header->len);
  path.len = 0;
  strbuf_appendf(&path, "%s/%s.c", out_dir, name);
  ok = (bool)(ok && write_output(path.data, source->data, source->len));
  if (ok && cli->generate.runtime) {
    path.len = 0;
    strbuf_appendf(&path, "%s/bare.h", out_dir);
    ok = write_output(path.data, RUNTIME_HEADER, sizeof(RUNTIME_HEADER));
    path.len = 0;
    strbuf_appendf(&path, "%s/bare.c", out_dir);
    ok = (bool)(ok && write_output(path.data, RUNTIME_SOURCE, sizeof(RUNTIME_SOURCE)));
  }
  strbuf_free(&path);
  return ok;
}

int cmd_generate(const Cli *cli) {
  char *schema_text = nullptr;
  Schema schema = {};
  if (!cmd_load_schema(cli->generate.schema_path, &schema_text, &schema)) {
    return 1;
  }
  int result = 1;
  Config cfg = config_default();
  Diag diag = {};
  StrBuf discovered = {};
  StrBuf name = {};
  StrBuf out_dir = {};
  StrBuf header = {};
  StrBuf source = {};

  const char *config_path = cli->generate.config_path;
  if (config_path == nullptr) {
    dir_of(cli->generate.schema_path, &discovered);
    strbuf_append(&discovered, "/barec.conf");
    if (file_exists(discovered.data)) {
      config_path = discovered.data;
      fprintf(stderr, "%s: using config '%s'\n", cli_prog_name(), config_path);
    }
  }
  if (config_path != nullptr && !config_load(&cfg, config_path, &diag)) {
    cmd_print_diag(config_path, &diag);
    goto done;
  }
  if (cli->generate.std.has_value) {
    cfg.std = cli->generate.std.value;
  }
  if (cli->generate.prefix != nullptr) {
    cfg.prefix = (Str){.data = cli->generate.prefix, .len = strlen(cli->generate.prefix)};
  }

  if (cli->generate.name != nullptr) {
    strbuf_append(&name, cli->generate.name);
  } else {
    stem_of(cli->generate.schema_path, &name);
    if (name.len == 0 || name.data[0] == '.') {
      fprintf(stderr, "%s: error: cannot derive an output name from '%s', pass --name\n",
              cli_prog_name(), cli->generate.schema_path);
      goto done;
    }
  }
  if (cli->generate.out_dir != nullptr) {
    strbuf_append(&out_dir, cli->generate.out_dir);
  } else {
    dir_of(cli->generate.schema_path, &out_dir);
  }

  if (!codegen_generate(&schema, &cfg, name.data, &header, &source, &diag)) {
    cmd_print_diag(cli->generate.schema_path, &diag);
    goto done;
  }
  if (write_generated(cli, out_dir.data, name.data, &header, &source)) {
    result = 0;
  }

done:
  strbuf_free(&discovered);
  strbuf_free(&name);
  strbuf_free(&out_dir);
  strbuf_free(&header);
  strbuf_free(&source);
  config_free(&cfg);
  schema_free(&schema);
  free(schema_text);
  return result;
}
