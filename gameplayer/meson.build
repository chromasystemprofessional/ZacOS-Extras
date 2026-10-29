gameplayer_exe = executable('zacos9-gameplayer',
  'src/main.cpp', 'src/gameplayer.cpp', 'src/gamemodule.cpp',
  dependencies: collarextension_dep,
  install: true,
  install_tag: 'gameplayer',
)

install_data('data/zacos9-gameplayer.desktop',
  install_dir: get_option('datadir') / 'applications',
  install_tag: 'gameplayer',
)

install_data('data/zacos9-mimeapps.list',
  install_dir: get_option('sysconfdir') / 'xdg',
  install_tag: 'gameplayer',
)

test('gameplayer', find_program('dbus-run-session'),
  args: ['--', find_program('python3').full_path(), files('tests/test_gameplayer.py'), gameplayer_exe],
  env: ['PYTHONDONTWRITEBYTECODE=1'],
  timeout: 120,
)