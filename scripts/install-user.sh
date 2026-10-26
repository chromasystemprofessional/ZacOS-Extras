#!/bin/sh
# Install an extra for this user (~/.local/bin, ~/.config/autostart) and (re)start it.
#   scripts/install-user.sh hue
set -eu
cd "$(dirname "$0")/.."

case "${1:-}" in
hue)
	program=zacos9-collar-hue
	desktop=hue/data/zacos9-collar-hue.desktop
	built=build/hue/$program
	;;
turntable)
	program=zacos9-collar-turntable
	desktop=turntable/data/zacos9-collar-turntable.desktop
	built=build/turntable/$program
	;;
*)
	echo "usage: $0 hue|turntable" >&2
	exit 2
	;;
esac

scripts/build.sh >/dev/null
bin="$HOME/.local/bin"
autostart="${XDG_CONFIG_HOME:-$HOME/.config}/autostart"
install -Dm755 "$built" "$bin/$program"
mkdir -p "$autostart"
# Autostart runs with a minimal PATH, so name the program by its full path.
sed "s|^Exec=.*|Exec=$bin/$program|" "$desktop" >"$autostart/$(basename "$desktop")"

pkill -x -f "$bin/$program" 2>/dev/null || true
setsid -f "$bin/$program" >/dev/null 2>&1 </dev/null
echo "Installed $program to $bin and started it; it starts at every login."
