project(
  'barec',
  'c',
  version: '0.1.0',
  default_options: ['c_std=gnu23', 'warning_level=3', 'werror=true'],
)

add_project_arguments('-Werror=vla', language: 'c')

src_inc = include_directories('src')
runtime_inc = include_directories('runtime')

bare_runtime = static_library('bare', 'runtime/bare.c')

barec_lib = static_library(
  'barec_lib',
  [
    'src/backend/codegen/emit.c',
    'src/backend/codegen/generate.c',
    'src/backend/codegen/ident.c',
    'src/backend/codegen/scan.c',
    'src/backend/config.c',
    'src/backend/names.c',
    'src/frontend/check.c',
    'src/frontend/lexer.c',
    'src/frontend/parser.c',
    'src/frontend/schema.c',
    'src/util/diag.c',
    'src/util/strbuf.c',
  ],
  include_directories: src_inc,
)

barec = executable(
  'barec',
  'src/main.c',
  include_directories: src_inc,
  link_with: barec_lib,
)

test_runtime = executable(
  'test_runtime',
  'tests/test_runtime.c',
  include_directories: runtime_inc,
  link_with: bare_runtime,
)
test('runtime', test_runtime)

test_lexer = executable(
  'test_lexer',
  'tests/test_lexer.c',
  include_directories: src_inc,
  link_with: barec_lib,
)
test('lexer', test_lexer)

test_parser = executable(
  'test_parser',
  'tests/test_parser.c',
  include_directories: src_inc,
  link_with: barec_lib,
)
test('parser', test_parser)

test_check = executable(
  'test_check',
  'tests/test_check.c',
  include_directories: src_inc,
  link_with: barec_lib,
)
test('check', test_check)

test_config = executable(
  'test_config',
  'tests/test_config.c',
  include_directories: src_inc,
  link_with: barec_lib,
)
test('config', test_config)

test_names = executable(
  'test_names',
  'tests/test_names.c',
  include_directories: src_inc,
  link_with: barec_lib,
)
test('names', test_names)

test_codegen = executable(
  'test_codegen',
  'tests/test_codegen.c',
  include_directories: src_inc,
  link_with: barec_lib,
)
test('codegen', test_codegen)
