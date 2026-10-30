project('zacos9-extras', 'cpp',
  version: '0.1.0',
  default_options: ['cpp_std=c++17', 'warning_level=2', 'buildtype=debug'],
  meson_version: '>=1.0',
)

qt6 = dependency('qt6', modules: ['Core', 'Gui', 'Network', 'DBus', 'Widgets'])

# Shared by every extra that adds modules to The Collar.
collarextension = static_library('collarextension', 'common/collarextension.cpp',
  dependencies: qt6,
)
collarextension_dep = declare_dependency(
  link_with: collarextension,
  include_directories: include_directories('common'),
  dependencies: qt6,
)

subdir('hue')
subdir('turntable')
subdir('phone')
subdir('nightmode')
subdir('gameplayer')
subdir('scrcpy')
