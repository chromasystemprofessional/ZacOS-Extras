# ZacOS 9 Extras

Add-ons and tools for [ZacOS 9](../ZacOS) that live outside the main tree.
They are built and installed separately, and they talk to ZacOS only through
its public interfaces (D-Bus, settings files, XDG autostart).

| Extra | What | Docs |
|---|---|---|
| `hue/` | **Hue lights in The Collar**: the Philips Hue rooms and zones you choose as Collar modules (on/off, brightness, scenes) | [hue/README.md](hue/README.md) |
| `turntable/` | **A turntable in The Collar**: plays a USB turntable through the sound output, switched on and off from a Collar module | [turntable/README.md](turntable/README.md) |
| `phone/` | **Phone**: KDE Connect and Syncthing for your phone: notifications, file sharing, clipboard and photo sync | [phone/README.md](phone/README.md) |
| `gameplayer/` | **Game Player**: plays NES, Super NES, Sega, Game Boy, Game Boy Advance, PlayStation and Nintendo 64 cartridges and discs; double-click a game file, or launch from the Collar module while one runs | [gameplayer/README.md](gameplayer/README.md) |
| `steam/` | **Steam games in The Collar**: lists the games installed by a native or Flatpak Steam and launches them through that Steam | — |
| `frontdesk/` | **Front Desk**: a shared-computer session shell with a tabbed app/document panel, a Front Desk login and an At Ease-style setup; uses the real accounts and the existing program allowlist | [frontdesk/README.md](frontdesk/README.md) |
| `netzac/` | **Netzac Navigator**: a Navigator 4.0-style browser with ZacOS 9's Platinum look, tabs, Calm Mode and an adblocker; opens from Applications and the **Browse the Web** Desktop alias | [netzac/README.md](netzac/README.md) |
| `common/` | `CollarExtensionService`, for any extra that adds Collar modules | [common/collarextension.h](common/collarextension.h) |

## Installing

Open **Software** (Apple menu > New Tricks) and choose **Extras**. It
installs an extra's newest release from this repository's GitHub releases,
and Remove takes it away again.

## Build, test, install

```sh
scripts/build.sh              # meson + ninja into build/
meson test -C build           # every extra's tests (private D-Bus, fake devices)
scripts/install-user.sh hue   # install one extra for this user and start it
```

`install-user.sh` puts binaries in `~/.local/bin` and autostart entries in
`~/.config/autostart`, which `zacos9-autostart` runs at login. It needs no sudo.
It's for trying changes: remove it again before installing the same extra
from Software, or the two copies compete for the same D-Bus name.

## Releasing

```sh
scripts/build-deb.sh hue 0.1.0                       # just the package, into build/packages
scripts/release.sh hue 0.1.0 "What's new, in a sentence or two."
```

Each extra has an `EXTRA/package.conf` (package name, description,
dependencies, homepage) and installs with its own meson `install_tag`. Most
packages hold `/usr/bin/PROGRAM` and `/etc/xdg/autostart/PROGRAM.desktop`;
removing one stops the program. An application extra such as Netzac has no
autostart entry: its package holds the program, desktop entry, icon and
translations, and Software adds its Desktop alias after installing it.

`release.sh` runs on a clean `main` that matches `origin/main`. It builds and
tests, tags `EXTRA-vVERSION`, builds the package from that commit, pushes
the tag and creates a GitHub release with the `.deb` attached. Software
picks an extra's newest release by its tag prefix.

## Requirements

- Qt 6 (Core, Gui, Network, DBus, Widgets), Meson, Ninja. These are the same packages ZacOS builds with.
- `qt6-webengine-dev` for Netzac (`sudo apt install qt6-webengine-dev`).
- Collar extensions need a ZacOS build that includes The Collar's extension
  client (`shell/collar/extensions.cpp`; see "Extensions" in
  `../ZacOS/docs/collar.md`).
