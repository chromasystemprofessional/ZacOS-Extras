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

enum class System { None, Nes, Snes, Ps1, N64, Sega, GameBoy, Gba };

/* Which system a file belongs to, or None. An extension alone is not enough
 * where it is shared: a .md Markdown file is not a Sega cartridge and a bare
 * .bin without a .cue is not a disc. */
System detectSystem(const QString &path);

/* The file to hand the engine. A PlayStation disc is its .cue sheet. */
QString launchFile(System system, const QString &path);

/* A human name for a system: "NES", "Super NES", "PlayStation", ... */
QString systemName(System system);

/* One engine run: a program and its arguments. */
struct Launch {
	QString program;
	QStringList args;
	QStringList env; /* NAME=value added to the engine's environment */
};

/* The engine run for a game file, or an empty program when it is unknown or
 * the engine is not installed. Config files are written by ensureConfigs(). */
Launch buildLaunch(System system, const QString &file, const QString &configDir,
		QString *error = nullptr);

/* Write the engine config files (RetroArch, mednafen, mupen64plus). */
bool ensureConfigs(const QString &configDir);

/* The RetroArch config written on first run, with absolute save paths. */
QString retroarchConfig(const QString &saveDir);

} // namespace gameplayer