turntable_exe = executable('zacos9-collar-turntable',
  'src/main.cpp', 'src/turntableaudio.cpp', 'src/turntablemodule.cpp',
  dependencies: collarextension_dep,
  install: true,
  install_tag: 'turntable',
)

install_data('data/zacos9-collar-turntable.desktop',
  install_dir: get_option('sysconfdir') / 'xdg' / 'autostart',
  install_tag: 'turntable',
)

test('turntable', find_program('dbus-run-session'),
  args: ['--', find_program('python3').full_path(), files('tests/test_turntable.py'), turntable_exe],
  env: ['PYTHONDONTWRITEBYTECODE=1'],
  timeout: 90,
)
