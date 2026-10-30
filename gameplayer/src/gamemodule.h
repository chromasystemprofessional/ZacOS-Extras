#pragma once

#include <QImage>
#include <QProcess>
#include <QTimer>
#include <functional>
#include <QObject>

#include "collarextension.h"
#include "gameplayer.h"

#include <QTimer>

/*
 * The Collar module that appears while a game runs: the game's name and a
 * Quit Game item. Quitting terminates the engine; the program exits when the
 * engine finishes. It also reports the game controllers auto-detected on this
 * machine, so they can be configured from The Collar.
 */
class GameModule : public QObject {
public:
	using Done = std::function<void(const QString &)>;
 
	GameModule(CollarExtensionService *service, const QString &game,
		gameplayer::System system, QProcess *engine, QObject *parent = nullptr);

	CollarModule module() const;
	void activate(const QString &item, Done done);

private:
	CollarExtensionService *m_service;
	QString m_game;
	gameplayer::System m_system;
	QProcess *m_engine;
	QTimer m_kill;
	QTimer m_refresh;
	QStringList m_controllers;
	void refreshControllers();
};