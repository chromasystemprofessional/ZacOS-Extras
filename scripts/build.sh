#!/bin/sh
# Configure (first run) and build every extra.
set -eu
cd "$(dirname "$0")/.."

if [ ! -f build/build.ninja ]; then
	meson setup build
fi
ninja -C build "$@"
