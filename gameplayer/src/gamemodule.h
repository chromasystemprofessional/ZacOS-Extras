#pragma once

#include <QImage>
#include <QProcess>
#include <QTimer>
#include <QUdpSocket>
#include <functional>
#include <QObject>

#include "collarextension.h"
#include "gameplayer.h"

/*
 * The Collar module that appears while a game runs: the game's name, a
 * Fullscreen toggle, and a Quit Game item.
 */
class GameModule : public QObject {
public:
	using Done = std::function<void(const QString &)>;

	GameModule(CollarExtensionService *service, const QString &game,
		gameplayer::System system, bool fullscreen, QProcess *engine,
		QObject *parent = nullptr);

	std::function<void(bool)> fullscreenChanged;

	CollarModule module() const;
	void activate(const QString &item, Done done);

	bool fullscreen() const { return m_fullscreen; }

	/* Set by activate when the engine must be restarted (non-RetroArch). */
	bool restartRequested() const { return m_restartRequested; }
	bool quitRequested() const { return m_quitRequested; }
	void clearRestartRequested() { m_restartRequested = false; }
	void clearQuitRequested() { m_quitRequested = false; }

private:
	bool isRetroArch() const;

	CollarExtensionService *m_service;
	QString m_game;
	gameplayer::System m_system;
	bool m_fullscreen;
	QProcess *m_engine;
	QTimer m_kill;
	bool m_restartRequested = false;
	bool m_quitRequested = false;
};