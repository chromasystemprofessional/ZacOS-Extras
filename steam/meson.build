steam_exe = executable('zacos9-steam',
  'src/main.cpp', 'src/steammodule.cpp',
  dependencies: collarextension_dep,
  install: true,
  install_tag: 'steam',
)

install_data('data/zacos9-steam.desktop',
  install_dir: get_option('datadir') / 'applications',
  install_tag: 'steam',
)

test('steam', find_program('dbus-run-session'),
  args: ['--', find_program('python3').full_path(), files('tests/test_steam.py'), steam_exe],
  env: ['PYTHONDONTWRITEBYTECODE=1'],
  timeout: 120,
)