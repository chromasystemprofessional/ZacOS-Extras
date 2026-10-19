# ZacOS 9 Extras

Add-ons and tools for [ZacOS 9](../ZacOS) that live outside the main tree.
They are built and installed separately, and they talk to ZacOS only through
its public interfaces (D-Bus, settings files, XDG autostart).

| Extra | What | Docs |
|---|---|---|
| `hue/` | **Hue lights in The Collar**: the Philips Hue rooms and zones you choose as Collar modules (on/off, brightness, scenes) | [hue/README.md](hue/README.md) |
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
dependencies) and installs with its own meson `install_tag`. Its package
holds `/usr/bin/PROGRAM` and `/etc/xdg/autostart/PROGRAM.desktop`; removing
it stops the program.

`release.sh` runs on a clean `main` that matches `origin/main`. It builds and
tests, tags `EXTRA-vVERSION`, builds the package from that commit, pushes
the tag and creates a GitHub release with the `.deb` attached. Software
picks an extra's newest release by its tag prefix.

## Requirements

- Qt 6 (Core, Gui, Network, DBus), Meson, Ninja. These are the same packages ZacOS builds with.
- Collar extensions need a ZacOS build that includes The Collar's extension
  client (`shell/collar/extensions.cpp`; see "Extensions" in
  `../ZacOS/docs/collar.md`).
