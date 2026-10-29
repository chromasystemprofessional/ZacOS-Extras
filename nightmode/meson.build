nightmode_exe = executable('zacos9-collar-nightmode',
  'src/main.cpp', 'src/nightmodule.cpp',
  dependencies: collarextension_dep,
  install: true,
  install_tag: 'nightmode',
)

install_data('data/zacos9-collar-nightmode.desktop',
  install_dir: get_option('sysconfdir') / 'xdg' / 'autostart',
  install_tag: 'nightmode',
)

test('nightmode', find_program('true'))