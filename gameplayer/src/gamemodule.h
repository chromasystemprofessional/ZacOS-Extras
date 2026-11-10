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
		gameplayer::System system, bool fullscreen, int volume, QProcess *engine,
		QObject *parent = nullptr);

	std::function<void(bool)> fullscreenChanged;
	std::function<void(int)> volumeChanged;

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
	int m_volume;
	QProcess *m_engine;
	QTimer m_kill;
	QTimer m_refresh;
	QTimer m_volumeTimer;
	int m_volumeAttempts = 0;
	QStringList m_controllers;
	bool m_restartRequested = false;
	bool m_quitRequested = false;
	void refreshControllers();
	void applyVolume();
};