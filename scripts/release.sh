#!/bin/sh
# Publish one extra, which is what ZacOS's Software window offers under Extras:
#
#   scripts/release.sh hue 0.1.0 "What's new, one or more sentences."
#
# On a clean main branch that matches origin/main, it tags EXTRA-vVERSION
# (hue-v0.1.0), builds the extra's package from that commit, pushes the tag,
# and creates the GitHub release with the .deb attached. Software finds an
# extra's newest release by that tag prefix. Needs `gh auth login`.
set -eu
self=$(realpath "$0")
cd "$(dirname "$self")/.."

die() {
	echo "release.sh: $1" >&2
	exit 1
}

[ "$#" -ge 2 ] || die "usage: scripts/release.sh EXTRA VERSION [NOTES]"
extra=$1
version=$2
case "$extra" in
	*[!a-z0-9-]* | "") die "'$extra' isn't an extra's directory name." ;;
esac
[ -f "$extra/package.conf" ] || die "$extra/package.conf doesn't exist."
. "./$extra/package.conf"
notes=${3:-"$name $version."}
tag=$extra-v$version

command -v gh >/dev/null || die "gh isn't installed (sudo apt install gh)."
gh auth status >/dev/null 2>&1 || die "not logged in to GitHub: run gh auth login."
[ "$(git rev-parse --abbrev-ref HEAD)" = main ] || die "releases are made from main."
[ -z "$(git status --porcelain)" ] || die "commit or stash your changes first."
git fetch -q origin
[ "$(git rev-parse HEAD)" = "$(git rev-parse origin/main)" ] || die "main isn't the same as origin/main; pull or push first."
git rev-parse -q --verify "refs/tags/$tag" >/dev/null && die "$tag already exists."
last=$(git tag -l "$extra-v*" | sed "s/^$extra-v//" | sort -V | tail -1)
if [ -n "$last" ]; then
	dpkg --compare-versions "$version" gt "$last" || die "$version isn't newer than $last."
fi

scripts/build.sh >/dev/null
meson test -C build "$extra" >/dev/null || die "$extra's tests fail (meson test -C build $extra)."
deb=$(scripts/build-deb.sh "$extra" "$version")

git tag -a "$tag" -m "$name $version"
git push -q origin "$tag"
gh release create "$tag" "$deb" --title "$name $version" --notes "$notes" --latest=false
echo "Released $name $version: ZacOS's Software window will offer it."
