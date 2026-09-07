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
  ['src/check.c', 'src/diag.c', 'src/lexer.c', 'src/parser.c', 'src/schema.c'],
  include_directories: src_inc,
)

barec = executable(
  'barec',
  'src/main.c',
  include_directories: src_inc,
  link_with: barec_lib,
)

test_smoke = executable(
  'test_smoke',
  'tests/test_smoke.c',
  include_directories: [src_inc, runtime_inc],
  link_with: bare_runtime,
)
test('smoke', test_smoke)

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
