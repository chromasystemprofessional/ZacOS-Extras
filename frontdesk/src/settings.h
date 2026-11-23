#pragma once

#include <QString>

/* One account's Front Desk presentation and privileges. The identity is the
 * Unix account; this file only says how Front Desk behaves for it. */
struct FrontDeskSettings {
	QString user;
	QString displayName;
	QString home;
	QString menus = QStringLiteral("restricted");
	QString save = QStringLiteral("user");
	bool saveRemovable = false;
	bool finder = false;
	bool speech = false;
	bool capture = false;
	/* The menu bar's Zac menu, with every item: Control Panels, Software,
	 * Old Dog and the rest. Off by default: it is a way out of the panel. */
	bool zacMenu = false;
	/* One Front Desk panel per screen, each on its own front program. Off:
	 * one panel and one front program, on the main display. */
	bool eachScreen = false;
	/* The settings file exists: Front Desk starts for this account. */
	bool frontDesk = false;

	/* The current account, or another one for the Setup window. */
	static FrontDeskSettings load(const QString &user = QString());
	static QString currentUserName();
	static QString configDir();
};

/* The folders that get their own page on the user panel. The user's own
 * list, in ~/.config/zacos9/frontdesk-folders; a missing file means the
 * home folder alone, and an existing empty one means no folder pages.
 * ZACOS9_FRONTDESK_FOLDERS replaces the file, for tests. */
QString frontDeskFoldersFile();
QStringList frontDeskFolders();
bool setFrontDeskFolders(const QStringList &paths);
