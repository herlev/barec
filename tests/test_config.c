#include "backend/config.h"
#include "util/diag.h"
#include "util/types.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static Config load_ok(const char *text) {
  Config cfg = config_default();
  Diag diag = {};
  assert(config_load_text(&cfg, text, strlen(text), &diag));
  return cfg;
}

static Diag load_fail(const char *text) {
  Config cfg = config_default();
  Diag diag = {};
  assert(!config_load_text(&cfg, text, strlen(text), &diag));
  return diag;
}

static void test_defaults(void) {
  Config cfg = config_default();
  assert(cfg.type_case == CaseStyle_PASCAL);
  assert(cfg.field_case == CaseStyle_SNAKE);
  assert(cfg.function_case == CaseStyle_SNAKE);
  assert(cfg.enum_variant_style == EnumVariantStyle_TYPE_UPPER);
  assert(cfg.prefix.len == 0);
  assert(cfg.str_cap == 64);
  assert(cfg.data_cap == 64);
  assert(cfg.list_cap == 8);
  assert(cfg.map_cap == 8);
  assert(cfg.overrides_len == 0);
}

static void test_empty(void) {
  Config cfg = load_ok("");
  assert(cfg.str_cap == 64);
  config_free(&cfg);
}

static void test_full(void) {
  Config cfg = load_ok("# codegen settings\n"
                       "[naming]\n"
                       "type_case = pascal\n"
                       "field_case = camel\n"
                       "function_case = snake\n"
                       "enum_variant = UPPER\n"
                       "prefix = acme\n"
                       "\n"
                       "[caps]\n"
                       "str = 128\n"
                       "list = 16 # inline comment\n"
                       "\n"
                       "[caps.overrides]\n"
                       "Customer.orders = 32\n"
                       "Employee.metadata.key = 4\n");
  assert(cfg.type_case == CaseStyle_PASCAL);
  assert(cfg.field_case == CaseStyle_CAMEL);
  assert(cfg.enum_variant_style == EnumVariantStyle_UPPER);
  assert(str_eq(cfg.prefix, STR("acme")));
  assert(cfg.str_cap == 128);
  assert(cfg.list_cap == 16);
  assert(cfg.data_cap == 64);
  assert(cfg.map_cap == 8);
  assert(cfg.overrides_len == 2);
  assert(str_eq(cfg.overrides[0].path, STR("Customer.orders")));
  assert(cfg.overrides[0].cap == 32);
  assert(str_eq(cfg.overrides[1].path, STR("Employee.metadata.key")));
  assert(cfg.overrides[1].cap == 4);
  config_free(&cfg);
}

static void test_section_switching(void) {
  Config cfg = load_ok("[caps]\n"
                       "str = 32\n"
                       "[naming]\n"
                       "field_case = screaming\n"
                       "[caps]\n"
                       "map = 2\n");
  assert(cfg.str_cap == 32);
  assert(cfg.map_cap == 2);
  assert(cfg.field_case == CaseStyle_SCREAMING);
  config_free(&cfg);
}

static void test_errors(void) {
  Diag diag = load_fail("[bogus]\n");
  assert(strstr(diag.message, "unknown section") != nullptr);
  assert(diag.loc.line == 1);

  diag = load_fail("str = 64\n");
  assert(strstr(diag.message, "outside of a section") != nullptr);

  diag = load_fail("[naming]\nbogus = 1\n");
  assert(strstr(diag.message, "unknown key 'bogus'") != nullptr);
  assert(diag.loc.line == 2);

  diag = load_fail("[naming]\ntype_case = kebab\n");
  assert(strstr(diag.message, "invalid case style 'kebab'") != nullptr);

  diag = load_fail("[naming]\nenum_variant = Kebab\n");
  assert(strstr(diag.message, "invalid enum variant style") != nullptr);

  diag = load_fail("[naming]\nprefix = 9lives\n");
  assert(strstr(diag.message, "invalid prefix") != nullptr);

  diag = load_fail("[caps]\nstr = 0\n");
  assert(strstr(diag.message, "at least one") != nullptr);

  diag = load_fail("[caps]\nstr = 4294967296\n");
  assert(strstr(diag.message, "32 bits") != nullptr);

  diag = load_fail("[caps]\nstr = abc\n");
  assert(strstr(diag.message, "invalid integer") != nullptr);

  diag = load_fail("[caps]\nstr\n");
  assert(strstr(diag.message, "expected 'key = value'") != nullptr);

  diag = load_fail("[caps.overrides]\nCustomer.orders = 1\nCustomer.orders = 2\n");
  assert(strstr(diag.message, "duplicate override") != nullptr);
  assert(diag.loc.line == 3);

  diag = load_fail("[caps.overrides]\nbad path = 1\n");
  assert(strstr(diag.message, "invalid override path") != nullptr);
}

static void test_load_file(void) {
  const char *path = "test_config_tmp.conf";
  FILE *f = fopen(path, "w");
  assert(f != nullptr);
  (void)fputs("[caps]\nstr = 99\n", f);
  (void)fclose(f);

  Config cfg = config_default();
  Diag diag = {};
  assert(config_load(&cfg, path, &diag));
  assert(cfg.str_cap == 99);
  config_free(&cfg);
  (void)remove(path);

  assert(!config_load(&cfg, "does_not_exist.conf", &diag));
  assert(strstr(diag.message, "cannot open") != nullptr);
}

int main(void) {
  test_defaults();
  test_empty();
  test_full();
  test_section_switching();
  test_errors();
  test_load_file();
  return 0;
}
