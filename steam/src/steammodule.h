#pragma once

#include "collarextension.h"

#include <QProcess>
#include <QObject>
#include <functional>
#include <QTimer>

class SteamModule : public QObject {
public:
	using Done = std::function<void(const QString &)>;
	SteamModule(CollarExtensionService *service, QObject *parent = nullptr);
	CollarModule module() const;
	void activate(const QString &item, Done done);
private:
	CollarExtensionService *m_service;
	QTimer m_refresh;
	struct Game {
		QString id, name;
		bool flatpak = false;
	};
	QList<Game> m_games;
	bool m_native = false;  /* a `steam` command of its own */
	bool m_flatpak = false; /* com.valvesoftware.Steam from Flathub */
	bool m_steamInstalled = false;
	void refresh();
};
