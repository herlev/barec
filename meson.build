project(
  'barec',
  'c',
  version: '0.1.0',
  default_options: [
    'c_std=gnu23',
    'warning_level=3',
    'werror=true',
    'b_sanitize=address,undefined',
  ],
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
  install: true,
)

test(
  'runtime',
  executable(
    'test_runtime',
    'tests/test_runtime.c',
    include_directories: runtime_inc,
    link_with: bare_runtime,
  ),
)

foreach name : ['lexer', 'parser', 'check', 'config', 'names', 'codegen']
  test(
    name,
    executable(
      'test_' + name,
      'tests/test_' + name + '.c',
      include_directories: src_inc,
      link_with: barec_lib,
    ),
  )
endforeach

test('cli', find_program('tests/cli.sh'), args: [barec])

test('snapshots', find_program('tests/snapshot.sh'), args: [barec])

example_gen = custom_target(
  'example-gen',
  input: 'tests/example.bare',
  output: ['example.h', 'example.c'],
  command: [barec, 'generate', '@INPUT@', '-o', '@OUTDIR@', '-n', 'example'],
)

test(
  'e2e',
  executable(
    'test_e2e',
    ['tests/test_e2e.c', example_gen],
    include_directories: runtime_inc,
    link_with: bare_runtime,
  ),
)

sink_gen = custom_target(
  'sink-gen',
  input: 'tests/sink.bare',
  output: ['sink.h', 'sink.c'],
  command: [barec, 'generate', '@INPUT@', '-o', '@OUTDIR@', '-n', 'sink'],
)

test(
  'sink',
  executable(
    'test_sink',
    ['tests/test_sink.c', sink_gen],
    include_directories: runtime_inc,
    link_with: bare_runtime,
  ),
)

example99_gen = custom_target(
  'example99-gen',
  input: 'tests/example.bare',
  output: ['example99.h', 'example99.c'],
  command: [
    barec,
    'generate',
    '@INPUT@',
    '-o', '@OUTDIR@',
    '-n', 'example99',
    '--std', 'c99',
  ],
)

static_library(
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

test(
  'example',
  executable(
    'example-protocol',
    ['examples/main.c', protocol_gen],
    include_directories: runtime_inc,
    link_with: bare_runtime,
  ),
)
