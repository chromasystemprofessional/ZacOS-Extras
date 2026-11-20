frontdesk_gio = dependency('gio-unix-2.0')

frontdesk_exe = executable('zacos9-frontdesk',
  'src/main.cpp',
  'src/frontdesk.cpp',
  'src/apps.cpp',
  'src/settings.cpp',
  'src/art.cpp',
  dependencies: [qt6, frontdesk_gio],
  install: true,
  install_tag: 'frontdesk',
)

frontdesk_setup_exe = executable('zacos9-frontdesk-setup',
  'src/setup_main.cpp',
  'src/apps.cpp',
  'src/settings.cpp',
  dependencies: [qt6, frontdesk_gio],
  install: true,
  install_tag: 'frontdesk',
)

install_data('data/zacos9-frontdesk.desktop',
  install_dir: get_option('datadir') / 'applications',
  install_tag: 'frontdesk',
)

install_data('data/zacos9-frontdesk-setup.desktop',
  install_dir: get_option('datadir') / 'applications',
  install_tag: 'frontdesk',
)

test('frontdesk',
  find_program('python3'),
  args: [files('tests/test_frontdesk.py'), frontdesk_exe],
  env: [
    'PYTHONDONTWRITEBYTECODE=1',
    'QT_QPA_PLATFORM=offscreen',
  ],
  timeout: 60,
)

test('frontdesk-setup',
  find_program('python3'),
  args: [files('tests/test_setup.py'), frontdesk_setup_exe],
  env: [
    'PYTHONDONTWRITEBYTECODE=1',
    'QT_QPA_PLATFORM=offscreen',
  ],
  timeout: 60,
)

test('frontdesk-apps',
  executable('test-frontdesk-apps', 'tests/test_apps.cpp', 'src/apps.cpp',
    include_directories: include_directories('src'),
    dependencies: [qt6, frontdesk_gio],
    build_by_default: false,
  ),
  timeout: 60,
)
