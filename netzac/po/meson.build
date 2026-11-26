# i18n.gettext() installs with the fixed 'i18n' tag, but this repository's
# package builder installs only the 'netzac' tag. Compile the .mo files at
# install time instead, so they travel with the rest of the package and have
# gettext's expected name (zacos9-netzac.mo) in every locale directory.
msgfmt = find_program('msgfmt')
meson.add_install_script(find_program('install-translations.sh'), msgfmt,
  meson.current_source_dir(),
  install_tag: 'netzac',
)
