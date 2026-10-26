#!/bin/sh
# Build one extra's Debian package from the committed tree:
#
#   scripts/build-deb.sh hue 0.1.0     # build/packages/zacos9-extra-hue_0.1.0_amd64.deb
#
# What goes in is EXTRA/package.conf plus the files meson installs with the
# extra's install tag (to /usr and /etc).
set -eu
self=$(realpath "$0")
cd "$(dirname "$self")/.."
root=$(pwd)

die() {
	echo "build-deb.sh: $1" >&2
	exit 1
}

[ "$#" -eq 2 ] || die "usage: scripts/build-deb.sh EXTRA VERSION"
extra=$1
version=$2
case "$extra" in
	*[!a-z0-9-]* | "") die "'$extra' isn't an extra's directory name." ;;
esac
[ -f "$extra/package.conf" ] || die "$extra/package.conf doesn't exist."
case "$version" in
	[0-9]*) ;;
	*) die "'$version' must start with a digit, e.g. 0.1.0." ;;
esac
case "$version" in
	*[!A-Za-z0-9.+~]*) die "'$version' isn't a Debian version (letters, digits, . + ~)." ;;
esac
. "./$extra/package.conf"

work=$(mktemp -d /tmp/zacos9-extra-deb.XXXXXX)
trap 'rm -rf "$work"' EXIT
git archive HEAD | tar -x -C "$work" --one-top-level=src
meson setup "$work/build" "$work/src" --prefix=/usr --sysconfdir=/etc --buildtype=release >"$work/setup.log" 2>&1 ||
	{ cat "$work/setup.log" >&2; die "meson setup failed."; }
ninja -C "$work/build" >"$work/ninja.log" 2>&1 || { cat "$work/ninja.log" >&2; die "the build failed."; }
stage=$work/stage
meson install -C "$work/build" --tags "$extra" --destdir "$stage" --strip >/dev/null
[ -n "$(find "$stage" -type f)" ] || die "nothing is installed with the '$extra' tag."

# Library dependencies, the way dh_shlibdeps finds them.
mkdir -p "$work/shlibs/debian"
printf 'Source: %s\n\nPackage: %s\nArchitecture: any\n' "$package" "$package" >"$work/shlibs/debian/control"
exes=$(find "$stage" -type f -perm -u+x)
shlibs=$(cd "$work/shlibs" && dpkg-shlibdeps -O $(printf '%s\n' "$exes" | sed 's|^|-e|') 2>/dev/null |
	sed -n 's/^shlibs:Depends=//p')

arch=$(dpkg --print-architecture)
mkdir -p "$stage/DEBIAN"
{
	printf 'Package: %s\nVersion: %s\nArchitecture: %s\n' "$package" "$version" "$arch"
	printf 'Maintainer: %s <%s>\n' "$(git config user.name)" "$(git config user.email)"
	printf 'Section: x11\nPriority: optional\n'
	printf 'Installed-Size: %s\n' "$(du -sk --exclude=DEBIAN "$stage" | cut -f1)"
	printf 'Depends: %s\n' "${shlibs:+$shlibs, }$depends"
	[ -z "${recommends:-}" ] || printf 'Recommends: %s\n' "$recommends"
	printf 'Homepage: https://github.com/chromasystemprofessional/ZacOS-Extras\n'
	printf 'Description: %s\n' "$summary"
	printf '%s\n' "$description" | sed 's/^/ /'
} >"$stage/DEBIAN/control"
find "$stage/etc" -type f 2>/dev/null | sed "s|^$stage||" >"$stage/DEBIAN/conffiles"
[ -s "$stage/DEBIAN/conffiles" ] || rm "$stage/DEBIAN/conffiles"
if [ -n "${programs:-}" ]; then
	{
		printf '#!/bin/sh\nset -e\n'
		printf 'if [ "$1" = remove ]; then\n'
		# By command line: `pkill -x NAME` sees only a name's first 15 characters,
		# so it never matches zacos9-collar-hue.
		for p in $programs; do
			printf '\tpkill -x -f "(/usr/bin/)?%s" || true\n' "$p"
		done
		printf 'fi\n'
	} >"$stage/DEBIAN/prerm"
	chmod 755 "$stage/DEBIAN/prerm"
fi

mkdir -p "$root/build/packages"
deb=$root/build/packages/${package}_${version}_$arch.deb
dpkg-deb --root-owner-group -Zxz --build "$stage" "$deb" >/dev/null
echo "$deb"
