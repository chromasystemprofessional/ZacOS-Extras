#pragma once

/*
 * Game Player: recognise a console game file's system and build the command
 * that runs it. The recognition mirrors the Sniffer's in ~/ZacOS
 * (shell/files/items.cpp, romIconKind), so a file the Sniffer draws as a
 * cartridge or disc is exactly the one Game Player can play.
 *
 * Engines (Debian's): RetroArch for NES/Super NES/Sega/Game Boy/Game Boy
 * Advance, mednafen for PlayStation, mupen64plus for Nintendo 64.
 */

#include <QString>
#include <QStringList>

namespace gameplayer {

enum class System { None, Nes, Snes, Ps1, Ps2, N64, Sega, GameBoy, Gba };

/* The user's games folder, ~/Games. ZACOS9_GAMEPLAYER_GAMES overrides it,
 * for testing. */
QString gamesFolder();

/* Create ~/Games and ~/Games/Saved Games when they don't exist, and give
 * ~/Games Game Player's folder icon if it has no custom icon yet. */
bool ensureGamesFolder(QString *error = nullptr);

/* Move a PlayStation BIOS dropped in the games folder (scph5500.bin,
 * scph5501.bin or scph5502.bin) into mednafen's firmware folder, and a
 * 4 MB PlayStation 2 BIOS .bin into PCSX2's BIOS folders. Returns the files
 * it put in place. The dropped file is removed from ~/Games once it is
 * installed. */
QStringList installDroppedFirmware(const QString &configDir);

/* Which system a file belongs to, or None. An extension alone is not enough
 * where it is shared: a .md Markdown file is not a Sega cartridge and a bare
 * .bin without a .cue is not a disc. */
System detectSystem(const QString &path);

/* The file to hand the engine. A PlayStation disc is its .cue sheet. */
QString launchFile(System system, const QString &path);

/* A human name for a system: "NES", "Super NES", "PlayStation", ... */
QString systemName(System system);

/* What to tell the user when a system needs a BIOS or firmware that isn't
 * present; empty when the game runs without extra files (or the files are
 * there). The caller shows this instead of launching. */
QString biosInstructions(System system);

/* One engine run: a program and its arguments. */
struct Launch {
	QString program;
	QStringList args;
	QStringList env; /* NAME=value added to the engine's environment */
};

/* The engine run for a game file. `fullscreen` controls whether the engine
 * starts full-screen. An empty program means unknown system or missing engine;
 * `error` says why. */
Launch buildLaunch(System system, const QString &file, const QString &configDir,
		bool fullscreen, QString *error = nullptr);

/* Write the engine config files (RetroArch, mednafen, mupen64plus). */
bool ensureConfigs(const QString &configDir, bool fullscreen);

/* Game controllers (joysticks/gamepads) currently connected, auto-detected.
 * Names come from SDL2; ZACOS9_GAMEPLAYER_CONTROLLERS (comma-separated)
 * overrides the list, for testing and for hand-picking. */
QStringList detectedControllers();

/* SDL game-controller mapping strings for the detected controllers, so the
 * engines receive a button layout instead of a bare joystick. Empty when a
 * controller isn't in SDL's database (it then needs manual configuration). */
QStringList controllerMappings();
/* The RetroArch config written on first run, with absolute save paths. */
QString retroarchConfig(const QString &saveDir, bool fullscreen);

/* Mednafen config (windowed/fullscreen, autosave). */
QString mednafenConfig(bool fullscreen);

} // namespace gameplayer