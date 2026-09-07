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

barec = executable(
  'barec',
  'src/main.c',
  include_directories: src_inc,
)

test_smoke = executable(
  'test_smoke',
  'tests/test_smoke.c',
  include_directories: [src_inc, runtime_inc],
  link_with: bare_runtime,
)
test('smoke', test_smoke)
