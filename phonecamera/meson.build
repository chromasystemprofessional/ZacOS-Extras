phonecamera_exe = executable('zacos9-extra-phonecamera',
  'src/main.cpp',
  dependencies: qt6,
  install: true,
  install_tag: 'phonecamera',
)

install_data('data/zacos9-extra-phonecamera.desktop',
  install_dir: get_option('sysconfdir') / 'xdg' / 'autostart',
  install_tag: 'phonecamera',
)

test('phonecamera', find_program('true'))
