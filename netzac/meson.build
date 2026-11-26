# Netzac Navigator: a browser, not a Collar extra. The package installs
# the program, desktop entry, icon and translations; nothing starts at
# login.
netzac_qt6_dep = dependency('qt6',
  modules: ['Core', 'Gui', 'Network', 'Widgets', 'WebEngineCore', 'WebEngineWidgets'],
)

textdomain = 'zacos9-netzac'
localedir = get_option('prefix') / get_option('localedir')

subdir('shell/netzac')
subdir('po')

install_data('share/applications/netzac.desktop',
  install_dir: get_option('datadir') / 'applications',
  install_tag: 'netzac',
)

install_data('share/icons/hicolor/scalable/apps/netzac.svg',
  install_dir: get_option('datadir') / 'icons' / 'hicolor' / 'scalable' / 'apps',
  install_tag: 'netzac',
)

test('netzac',
  find_program('python3'),
  args: [
    files('tests/test_netzac.py'),
    files('share/applications/netzac.desktop'),
    files('share/icons/hicolor/scalable/apps/netzac.svg'),
    files('package.conf'),
    files('po/LINGUAS'),
  ],
  env: ['PYTHONDONTWRITEBYTECODE=1'],
  timeout: 60,
)
