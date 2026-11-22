#pragma once

#include <QString>

/* One account's Front Desk presentation and privileges. The identity is the
 * Unix account; this file only says how Front Desk behaves for it. */
struct FrontDeskSettings {
	QString user;
	QString displayName;
	QString home;
	QString documents;
	QString menus = QStringLiteral("restricted");
	QString save = QStringLiteral("user");
	bool saveRemovable = false;
	bool finder = false;
	bool speech = false;
	bool capture = false;
	/* The menu bar's Zac menu, with every item: Control Panels, Software,
	 * Old Dog and the rest. Off by default: it is a way out of the panel. */
	bool zacMenu = false;
	/* The settings file exists: Front Desk starts for this account. */
	bool frontDesk = false;

	/* The current account, or another one for the Setup window. */
	static FrontDeskSettings load(const QString &user = QString());
	static QString currentUserName();
	static QString configDir();
};
