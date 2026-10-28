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
gameplayer)
	program=zacos9-gameplayer
	desktop=gameplayer/data/zacos9-gameplayer.desktop
	built=build/gameplayer/$program
	scripts/build.sh >/dev/null
	bin="$HOME/.local/bin"
	install -Dm755 "$built" "$bin/$program"
	mkdir -p "$HOME/.local/share/applications"
	install -Dm644 "$desktop" "$HOME/.local/share/applications/$(basename "$desktop")"
	echo "Installed $program to $bin and its desktop entry to ~/.local/share/applications."
	echo "Double-click a game file to play it, or run: $bin/$program --play <game>"
	exit 0
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
