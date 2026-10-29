#pragma once

#include <QImage>
#include <QProcess>
#include <QTimer>
#include <functional>
#include <QObject>

#include "collarextension.h"
#include "gameplayer.h"

/*
 * The Collar module that appears while a game runs: the game's name, a
 * Fullscreen toggle, and a Quit Game item. Quitting (or toggling fullscreen,
 * which restarts the engine) terminates the engine; the program exits when the
 * engine finishes, unless a restart was requested.
 */
class GameModule : public QObject {
public:
	using Done = std::function<void(const QString &)>;

	GameModule(CollarExtensionService *service, const QString &game,
		gameplayer::System system, bool fullscreen, QProcess *engine,
		QObject *parent = nullptr);

	CollarModule module() const;
	void activate(const QString &item, Done done);

	/* Flipped by the Fullscreen item; the program reads this on relaunch. */
	bool fullscreen() const { return m_fullscreen; }

	/* Set by activate("fullscreen"); the program restarts the engine when set. */
	bool restartRequested() const { return m_restartRequested; }
	void clearRestartRequested() { m_restartRequested = false; }

private:
	CollarExtensionService *m_service;
	QString m_game;
	gameplayer::System m_system;
	bool m_fullscreen;
	QProcess *m_engine;
	QTimer m_kill;
	bool m_restartRequested = false;
};