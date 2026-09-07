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
    'src/backend/codegen/funcs.c',
    'src/backend/codegen/generate.c',
    'src/backend/codegen/ident.c',
    'src/backend/codegen/read.c',
    'src/backend/codegen/scan.c',
    'src/backend/codegen/write.c',
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
  [
    'src/cli/cli.c',
    'src/cli/cmd/check.c',
    'src/cli/cmd/common.c',
    'src/cli/cmd/config.c',
    'src/cli/cmd/generate.c',
    'src/main.c',
  ],
  include_directories: src_inc,
  link_with: barec_lib,
  c_args: [
    '-DBAREC_VERSION="@0@"'.format(meson.project_version()),
    '--embed-dir=' + meson.project_source_root() / 'runtime',
  ],
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

test('cli', find_program('tests/cli.sh'), args: [barec])

example_gen = custom_target(
  'example-gen',
  input: 'tests/example.bare',
  output: ['example.h', 'example.c'],
  command: [barec, 'generate', '@INPUT@', '-o', '@OUTDIR@', '-n', 'example'],
)

test_e2e = executable(
  'test_e2e',
  ['tests/test_e2e.c', example_gen[1]],
  include_directories: runtime_inc,
  link_with: bare_runtime,
)
test('e2e', test_e2e)

example99_gen = custom_target(
  'example99-gen',
  input: 'tests/example.bare',
  output: ['example99.h', 'example99.c'],
  command: [barec, 'generate', '@INPUT@', '-o', '@OUTDIR@', '-n', 'example99', '--std', 'c99'],
)

example99_lib = static_library(
  'example99',
  example99_gen[1],
  include_directories: runtime_inc,
  override_options: ['c_std=c99'],
)

protocol_gen = custom_target(
  'protocol-gen',
  input: 'examples/protocol.bare',
  output: ['protocol.h', 'protocol.c'],
  command: [barec, 'generate', '@INPUT@', '-o', '@OUTDIR@', '-n', 'protocol'],
)

example_protocol = executable(
  'example-protocol',
  ['examples/main.c', protocol_gen[1]],
  include_directories: runtime_inc,
  link_with: bare_runtime,
)
test('example', example_protocol)
