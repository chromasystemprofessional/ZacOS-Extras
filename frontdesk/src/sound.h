#pragma once

#include <QString>

/* Front Desk's short interface sounds. They come from ZacOS's original
 * sound files (the same set the desktop uses), never from Apple's: the
 * woodblock click for tabs and pages, the chirp for opening an item, and
 * the Platinum chime for an action that couldn't be done.
 *
 * The files are looked up in ZACOS9_DATA, the user's data folder, then the
 * installed data folder. Playback is detached and follows the Sound
 * control panel's interface volume; a missing file or player is silent. */
class FrontDeskSound {
public:
	FrontDeskSound();

	void click();
	void open();
	void alert();

	bool available() const { return !m_dir.isEmpty(); }
	QString directory() const { return m_dir; }

private:
	void play(const QString &file);

	QString m_dir;
	QString m_player;
};
