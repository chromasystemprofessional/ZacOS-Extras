hue_exe = executable('zacos9-collar-hue',
  'src/main.cpp', 'src/huebridge.cpp', 'src/huecolor.cpp', 'src/huemodules.cpp',
  dependencies: collarextension_dep,
  install: true,
  install_tag: 'hue',
)

install_data('data/zacos9-collar-hue.desktop',
  install_dir: get_option('sysconfdir') / 'xdg' / 'autostart',
  install_tag: 'hue',
)

test('hue', find_program('dbus-run-session'),
  args: ['--', find_program('python3').full_path(), files('tests/test_hue.py'), hue_exe],
  env: ['PYTHONDONTWRITEBYTECODE=1'],
  timeout: 60,
)
