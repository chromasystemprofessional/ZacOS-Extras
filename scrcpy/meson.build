scrcpy_exe = executable('zacos9-scrcpy',
  'src/main.cpp', 'src/scrcpymodule.cpp',
  dependencies: collarextension_dep,
  install: true,
  install_tag: 'scrcpy',
)

install_data('data/zacos9-scrcpy.desktop',
  install_dir: get_option('datadir') / 'applications',
  install_tag: 'scrcpy',
)

test('scrcpy', find_program('dbus-run-session'),
  args: ['--', find_program('python3').full_path(), files('tests/test_scrcpy.py'), scrcpy_exe],
  env: ['PYTHONDONTWRITEBYTECODE=1'],
  timeout: 120,
)