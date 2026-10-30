#pragma once

#include <QImage>
#include <QProcess>
#include <QTimer>
#include <functional>
#include <QObject>

#include "collarextension.h"
#include "gameplayer.h"

#include <QTimer>
#include <functional>
#include <QObject>

/*
 * The Collar module that appears while a game runs.
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
	bool restartRequested() const { return m_restartRequested; }
	bool quitRequested() const { return m_quitRequested; }
	void clearRestartRequested() { m_restartRequested = false; }
	void clearQuitRequested() { m_quitRequested = false; }

private:
	CollarExtensionService *m_service;
	QString m_game;
	gameplayer::System m_system;
	bool m_fullscreen;
	QProcess *m_engine;
	QTimer m_kill;
	QTimer m_refresh;
	QStringList m_controllers;
	bool m_restartRequested = false;
	bool m_quitRequested = false;
	void refreshControllers();
};