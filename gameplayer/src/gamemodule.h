#pragma once

#include <QImage>
#include <QProcess>
#include <QTimer>
#include <functional>
#include <QObject>

#include "collarextension.h"

/*
 * The Collar module that appears while a game runs: the game's name and a
 * Quit Game item. Quitting terminates the engine; the program exits when the
 * engine finishes.
 */
class GameModule : public QObject {
public:
	using Done = std::function<void(const QString &)>;

	GameModule(CollarExtensionService *service, const QString &game,
		const QString &system, QProcess *engine, QObject *parent = nullptr);

	CollarModule module() const;
	void activate(const QString &item, Done done);

private:
	CollarExtensionService *m_service;
	QString m_game;
	QString m_system;
	QProcess *m_engine;
	QTimer m_kill;
};