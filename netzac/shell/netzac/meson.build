qt6_module = import('qt6')

moc_files = qt6_module.compile_moc(
  headers: ['adblocker.h', 'calmmode.h', 'widgets.h', 'dialogs.h', 'webview.h', 'netzacwindow.h'],
  dependencies: netzac_qt6_dep,
)

executable('zacos9-netzac',
  [
    'main.cpp',
    'adblocker.cpp',
    'bookmarks.cpp',
    'calmmode.cpp',
    'history.cpp',
    'platinum.cpp',
    'preferences.cpp',
    'widgets.cpp',
    'dialogs.cpp',
    'webview.cpp',
    'netzacwindow.cpp',
    moc_files,
  ],
  dependencies: [netzac_qt6_dep],
  cpp_args: [
    '-DZACOS9_TEXTDOMAIN="' + textdomain + '"',
    '-DZACOS9_LOCALEDIR_DEFAULT="' + localedir + '"',
  ],
  install: true,
  install_tag: 'netzac',
)
