#!/bin/sh
# Compile Netzac's gettext catalogs into the staged install tree. Meson
# runs this only when the 'netzac' install tag is selected.
set -eu
msgfmt=$1
src=$2
dest=${MESON_INSTALL_DESTDIR_PREFIX:?}
textdomain=zacos9-netzac

for language in $(cat "$src/LINGUAS"); do
	case $language in
	'' | '#'*) continue ;;
	esac
	mkdir -p "$dest/share/locale/$language/LC_MESSAGES"
	"$msgfmt" -o "$dest/share/locale/$language/LC_MESSAGES/$textdomain.mo" \
		"$src/$language.po"
done
