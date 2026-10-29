# Game Player

`zacos9-gameplayer` runs console games in ZacOS 9. The Sniffer recognises
cartridge and disc files (NES, Super NES, Sega, Game Boy, Game Boy Advance,
PlayStation and Nintendo 64) and shows them with their own icons; double-click
one and Game Player starts the right engine for it, in a Platinum-framed
window. While a game runs, a **Game Player** module appears in The Collar
with the game's name and a **Quit Game** item.

Engines: RetroArch (NES, Super NES, Sega, Game Boy, Game Boy Advance),
mednafen (PlayStation) and mupen64plus (Nintendo 64). They come from
Debian's archive; installing the extra pulls them all in.

## Setting up

1. Install it with `scripts/install-user.sh gameplayer` (or from Software ▸
   Extras). It installs the program and its desktop entry.
2. Put games in `~/Games` (or anywhere). The Sniffer shows them with their
   own icons; double-click one to play it.
3. **PlayStation** needs a Sony BIOS. Put `scph5501.bin`, `scph5502.bin`
   and `scph5503.bin` in `~/.config/zacos9/gameplayer/mednafen/firmware/`.
   Without them mednafen says so and the game won't run.
4. **Nintendo 64** uses mupen64plus's default settings; its config goes in
   `~/.config/zacos9/gameplayer/mupen64plus/`.
5. **Leaving full screen:** press **F12** (a ZacOS shortcut that works for every
   engine) to drop out of full screen and reach The Collar. RetroArch also
   answers **F11**; mednafen **Alt+Enter**; mupen64plus **F11**.
6. **Save states** (RetroArch systems) are kept in `~/Games/Saved Games` and
   are saved automatically when a game is quit.

While a game runs, the **Game Player** module appears in The Collar. Its
**Quit Game** stops the engine and returns to the desktop. The engine's own
window also has a close box, but The Collar's item is the reliable way out.

## Details

- Engines: RetroArch 1.20 with the nestopia, snes9x, Genesis Plus GX,
  gambatte and mGBA cores; mednafen 1.32 for PlayStation; mupen64plus for
  Nintendo 64.
- Save files and states: `~/Games/Saved Games` for RetroArch systems.
- RetroArch runs windowed with auto save/load of states; its own menu (F1)
  stays available for power users.
- mednafen runs windowed (`video.fs 0` in its config, written once).
- mupen64plus uses glide64mk2 video, SDL audio and input, HLE RSP.
- `~/.config/zacos9/gameplayer/` holds the generated config files; the engine
  binaries come from Debian, not from this extra.
- D-Bus: while a game runs it owns `org.zacos9.CollarExtension.GamePlayer`.
  See "Extensions" in `~/ZacOS/docs/collar.md`.

## Limitations

- PlayStation and Nintendo 64 are detected and launched through mednafen and
  mupen64plus, which have their own keyboard layouts; their quit key may
  differ. The Collar's **Quit Game** works for all of them.
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