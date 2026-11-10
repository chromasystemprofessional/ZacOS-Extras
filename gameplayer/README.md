# Game Player

`zacos9-gameplayer` runs console games in ZacOS 9. The Sniffer recognises
cartridge and disc files (NES, Super NES, Sega, Game Boy, Game Boy Advance,
PlayStation, PlayStation 2 and Nintendo 64) and PlayStation 3 game folders,
and shows them with their own icons; double-click one and Game Player starts
the right engine for it, in a Platinum-framed window. While a game runs, a
**Game Player** module appears in The Collar with the game's name and a
**Quit Game** item.

Engines: RetroArch (NES, Super NES, Sega, Game Boy, Game Boy Advance),
mednafen (PlayStation), PCSX2 (PlayStation 2), mupen64plus (Nintendo 64) and
RPCS3 (PlayStation 3). They come from Debian's archive; installing the extra
pulls them all in. PCSX2 is Debian's i386 build, and ZacOS already enables
i386 for Wine, so installing Game Player installs PCSX2 too. RPCS3 is not in
Debian; install its AppImage or Flatpak (see below).

## Setting up

1. Install it with `scripts/install-user.sh gameplayer` (or from Software ▸
   Extras). It installs the program and its desktop entry.
2. Put games in `~/Games` (or anywhere). Game Player makes `~/Games` the
   first time it starts, with its own folder icon. The Sniffer shows game
   files with their own icons; double-click one to play it. A PlayStation 3
   game is a folder, not a file: put it in `~/Games` as it came, e.g.
   `~/Games/Demon's Souls/PS3_GAME/…`.
3. **PlayStation** needs a Sony BIOS. Drop the BIOS `.bin` in `~/Games`:
   `scph5500.bin` (Japan), `scph5501.bin` (North America) or
   `scph5502.bin` (Europe). Game Player recognises those images by their
   contents even when the file has another name. It moves the BIOS into
   mednafen's firmware folder when it next starts, so you never need to visit
   that folder and the file doesn't stay in `~/Games`. Without a BIOS Game
   Player says what to drop in and the game won't run.
4. **PlayStation 2** needs a PS2 BIOS. Game Player installs PCSX2 with the
   extra (Debian's i386 build; ZacOS enables i386 for Wine). Drop the 4 MB
   PS2 BIOS `.bin` from your own PS2 in `~/Games`; Game Player moves it into
   PCSX2's BIOS folder and removes it from `~/Games`. Without the BIOS, Game
   Player says what to do.
5. **PlayStation 3** needs RPCS3 and its firmware. Install RPCS3 from
   [rpcs3.net](https://rpcs3.net/) (AppImage) or the Flatpak
   `net.rpcs3.RPCS3`, run it once, and choose **File ▸ Install Firmware**
   with the official `PS3UPDAT.PUP` from Sony. Game Player finds `rpcs3` on
   `PATH`, an `rpcs3*.AppImage` in `~/Applications`, `~/Downloads` or
   `~/.local/bin`, or the Flatpak; set `ZACOS9_RPCS3` to another executable.
   Only folder dumps are launched this way; install `.pkg` games in RPCS3.
6. **Nintendo 64** uses mupen64plus's default settings; its config goes in
   `~/.config/zacos9/gameplayer/mupen64plus/`.
7. **Leaving full screen:** press **F12** (a ZacOS shortcut that works for every
   engine) to drop out of full screen and reach The Collar. RetroArch also
   answers **F11**; mednafen **Alt+Enter**; mupen64plus **F11**; RPCS3 keeps
   its own fullscreen setting and hotkey.
8. **Save states** (RetroArch systems) are kept in `~/Games/Saved Games` and
   are saved automatically when a game is quit.

While a game runs, the **Game Player** module appears in The Collar. Its
**Quit Game** stops the engine and returns to the desktop. The engine's own
window also has a close box, but The Collar's item is the reliable way out.

## Details

- Engines: RetroArch 1.20 with the nestopia, snes9x, Genesis Plus GX,
  gambatte and mGBA cores; mednafen 1.32 for PlayStation; PCSX2 for
  PlayStation 2; mupen64plus for Nintendo 64; RPCS3 for PlayStation 3.
- Save files and states: `~/Games/Saved Games` for RetroArch systems.
- `~/Games` holds the games and any PlayStation or PlayStation 2 BIOS `.bin`
  you drop in. Game Player moves a PS1 BIOS into mednafen's firmware folder
  (`~/.config/zacos9/gameplayer/mednafen/firmware/`) and a 4 MB PS2 BIOS into
  PCSX2's BIOS folders automatically, removing it from `~/Games`.
- RetroArch runs windowed with auto save/load of states; its own menu (F1)
  stays available for power users.
- mednafen runs windowed (`video.fs 0` in its config, written once).
- PCSX2 runs with `--nogui`, windowed or full screen; its BIOS lives in
  `~/.config/PCSX2/bios/` (or `~/.local/share/PCSX2/bios/` on PCSX2 2.x).
  Debian's program is `/usr/games/PCSX2`; Game Player looks for `PCSX2` or
  `pcsx2` on `PATH`. PCSX2 1.6's GS plugin needs an X11 window, so Game
  Player starts it with `GDK_BACKEND=x11` under ZacOS's Wayland session.
- mupen64plus uses glide64mk2 video, SDL audio and input, HLE RSP.
- RPCS3 runs with `--no-gui` and its own Vulkan window; its firmware and
  settings live in `~/.config/rpcs3/` (a Flatpak's in
  `~/.var/app/net.rpcs3.RPCS3/config/rpcs3/`). Fullscreen is RPCS3's own
  setting, so The Collar leaves the item out for PlayStation 3.
- `~/.config/zacos9/gameplayer/` holds the generated config files; the engine
  binaries come from Debian (RPCS3 from its own download), not from this extra.
- D-Bus: while a game runs it owns `org.zacos9.CollarExtension.GamePlayer`.
  See "Extensions" in `~/ZacOS/docs/collar.md`.

## Limitations

- PlayStation and Nintendo 64 are detected and launched through mednafen and
  mupen64plus, which have their own keyboard layouts; their quit key may
  differ. The Collar's **Quit Game** works for all of them.
- An `.iso` disc is treated as a PlayStation 2 game; a PlayStation 1 disc is
  recognised by its `.cue` sheet instead.
- PlayStation 3 games are launched as folders only. `.pkg` games install in
  RPCS3 itself, and RPCS3 compatibility is per-game: check its list before
  expecting a title to run. It needs a Vulkan GPU and a fast CPU.
- A Genesis cartridge named `*.md` (Markdown's extension) is recognised by
  its `SEGA` header, but shared-mime-info has no such type, so double-click
  routing works for `*.gen`, `*.smd` and `*.sms` only. Rename or use one of
  those if needed.
- Save/load/pause through The Collar are not built yet; Quit Game saves
  RetroArch's state automatically.

## Tests

`meson test -C build gameplayer` runs `tests/test_gameplayer.py` on a
private session bus. It uses fake engine binaries on `PATH` (no real
emulators needed) and covers:

- system detection from file bytes and extensions;
- PlayStation 3 game folders, both disc dumps and installed games;
- the engine and core chosen for each system, including PCSX2 under its
  Debian `PCSX2` program name;
- a bare `.bin` and a `.md` document being rejected;
- `~/Games` being created with its custom folder icon, and an existing icon
  being left alone;
- a PlayStation BIOS `.bin` dropped in `~/Games` being moved to mednafen,
  and a 4 MB PS2 BIOS `.bin` to PCSX2, without touching an ordinary `.bin`;
- those BIOS files being removed from `~/Games` once they are in place;
- PlayStation 2 running PCSX2 as an X11 client, so its GS plugin can open
  under ZacOS's Wayland session;
- the Game Player module appearing while a game runs, with Quit Game;
- Quit Game stopping the engine and the program.