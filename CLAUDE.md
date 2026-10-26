# CLAUDE.md: ZacOS 9 Extras

Extras and tools for ZacOS 9, kept out of the main tree at `~/ZacOS` (which
has its own CLAUDE.md; read it before touching anything there).

## Map

| Component | Path | Language |
|---|---|---|
| Collar extension helper (serves `org.zacos9.CollarExtension1`) | `common/collarextension.*` | C++ / Qt6 DBus |
| Hue lights for The Collar (`zacos9-collar-hue`) | `hue/src/` | C++ / Qt6 |
| └ Bridge client: discovery, pairing, CLIP v2 resources, event stream, commands | `hue/src/huebridge.*` | |
| └ Modules, menus, bulb icon | `hue/src/huemodules.*` | |
| └ xy / mired / sRGB conversions, colour presets | `hue/src/huecolor.*` | |
| Hue tests (fake bridge over HTTP + private session bus) | `hue/tests/` | Python (dbus-python) |
| USB turntable for The Collar (`zacos9-collar-turntable`) | `turntable/src/` | C++ / Qt6 |
| └ Sound server through `pactl`: USB inputs, loopbacks, output, `subscribe` | `turntable/src/turntableaudio.*` | |
| └ Module, menu, record icon, playing again when the turntable appears | `turntable/src/turntablemodule.*` | |
| Turntable tests (stand-in `pactl` + private session bus) | `turntable/tests/` | Python (dbus-python) |
| Build and install helpers | `scripts/` | sh |

## Rules

- Extras use only ZacOS's public interfaces. If an extra needs a new hook
  in ZacOS, add it there as a generic interface with its own test and
  documentation, as The Collar's extension protocol was done
  (`~/ZacOS/shell/collar/extensions.*`, `~/ZacOS/docs/collar.md`, "Extensions").
- Match ZacOS's style: tabs, no `Q_OBJECT`/moc (lambdas and
  `QDBusVirtualObject`), and never `QProcess::waitForFinished()`.
- No Apple or Philips/Signify artwork. Icons are drawn as our own pixel art.
- Build: `scripts/build.sh`. Test: `meson test -C build`. Redirect long output.
- Never put a test bridge's address in `~/.config/zacos9/hue.conf`.
  `$ZACOS9_HUE_BRIDGE` and a temporary `XDG_CONFIG_HOME` keep tests isolated.
