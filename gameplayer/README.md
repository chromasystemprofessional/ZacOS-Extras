# Game Player

`zacos9-gameplayer` runs console games in ZacOS 9. The Sniffer recognises
cartridge and disc files (NES, Super NES, Sega, Game Boy, Game Boy Advance,
PlayStation, PlayStation 2 and Nintendo 64) and shows them with their own
icons; double-click one and Game Player starts the right engine for it, in a
Platinum-framed window. While a game runs, a **Game Player** module appears
in The Collar with the game's name and a **Quit Game** item.

Engines: RetroArch (NES, Super NES, Sega, Game Boy, Game Boy Advance),
mednafen (PlayStation), PCSX2 (PlayStation 2) and mupen64plus (Nintendo 64).
RetroArch, mednafen and mupen64plus come from Debian's archive; installing
the extra pulls them in. PCSX2 is i386-only in Debian (see below).

## Setting up

1. Install it with `scripts/install-user.sh gameplayer` (or from Software ▸
   Extras). It installs the program and its desktop entry.
2. Put games in `~/Games` (or anywhere). The Sniffer shows them with their
   own icons; double-click one to play it.
3. **PlayStation** needs a Sony BIOS. Put `scph5501.bin`, `scph5502.bin`
   and `scph5503.bin` in `~/.config/zacos9/gameplayer/mednafen/firmware/`.
   Without them Game Player says what to do, and the game won't run.
4. **PlayStation 2** needs PCSX2 and a PS2 BIOS. Install PCSX2 first (Debian's
   is i386 — enable `dpkg --add-architecture i386` and install `pcsx2`, or
   use the Flatpak `org.pcsx2.PCSX2`), then copy the BIOS from your own PS2
   into `~/.config/PCSX2/bios/`. Without the BIOS, Game Player says what to do.
5. **Nintendo 64** uses mupen64plus's default settings; its config goes in
   `~/.config/zacos9/gameplayer/mupen64plus/`.
6. **Leaving full screen:** press **F12** (a ZacOS shortcut that works for every
   engine) to drop out of full screen and reach The Collar. RetroArch also
   answers **F11**; mednafen **Alt+Enter**; mupen64plus **F11**.
7. **Save states** (RetroArch systems) are kept in `~/Games/Saved Games` and
   are saved automatically when a game is quit.

While a game runs, the **Game Player** module appears in The Collar. Its
**Quit Game** stops the engine and returns to the desktop. The engine's own
window also has a close box, but The Collar's item is the reliable way out.

## Details

- Engines: RetroArch 1.20 with the nestopia, snes9x, Genesis Plus GX,
  gambatte and mGBA cores; mednafen 1.32 for PlayStation; PCSX2 for
  PlayStation 2; mupen64plus for Nintendo 64.
- Save files and states: `~/Games/Saved Games` for RetroArch systems.
- RetroArch runs windowed with auto save/load of states; its own menu (F1)
  stays available for power users.
- mednafen runs windowed (`video.fs 0` in its config, written once).
- PCSX2 runs with `--nogui`, windowed or full screen; its BIOS lives in
  `~/.config/PCSX2/bios/` (or `~/.local/share/PCSX2/bios/` on PCSX2 2.x).
- mupen64plus uses glide64mk2 video, SDL audio and input, HLE RSP.
- `~/.config/zacos9/gameplayer/` holds the generated config files; the engine
  binaries come from Debian, not from this extra.
- D-Bus: while a game runs it owns `org.zacos9.CollarExtension.GamePlayer`.
  See "Extensions" in `~/ZacOS/docs/collar.md`.

## Limitations

- PlayStation and Nintendo 64 are detected and launched through mednafen and
  mupen64plus, which have their own keyboard layouts; their quit key may
  differ. The Collar's **Quit Game** works for all of them.
- An `.iso` disc is treated as a PlayStation 2 game; a PlayStation 1 disc is
  recognised by its `.cue` sheet instead.
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
- the engine and core chosen for each system;
- a bare `.bin` and a `.md` document being rejected;
- the Game Player module appearing while a game runs, with Quit Game;
- Quit Game stopping the engine and the program.