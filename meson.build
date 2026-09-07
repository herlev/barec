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
    'src/frontend/check.c',
    'src/frontend/lexer.c',
    'src/frontend/parser.c',
    'src/frontend/schema.c',
    'src/util/diag.c',
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
