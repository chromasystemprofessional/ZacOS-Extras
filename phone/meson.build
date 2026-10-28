phone_exe = executable('zacos9-extra-phone',
  'src/main.cpp',
  dependencies: qt6,
  install: true,
  install_tag: 'phone',
)

install_data('data/zacos9-extra-phone.desktop',
  install_dir: get_option('sysconfdir') / 'xdg' / 'autostart',
  install_tag: 'phone',
)
