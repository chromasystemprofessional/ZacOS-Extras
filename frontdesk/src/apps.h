#pragma once

#include <QIcon>
#include <QString>
#include <QStringList>

#include <functional>
#include <vector>

class QObject;

/* One program the panel can offer. `entry` is malcontent's name for it: an
 * absolute program path or a Flatpak ref. */
struct DeskApp {
	QString name;
	QString desktopFile;
	QString iconName;
	QString entry;
	QString workingDir;
	QStringList command;
	bool terminal = false;
};

namespace AppCatalog {

/* Installed programs the Applications folder would show, minus ZacOS's own
 * programs and control panels. */
std::vector<DeskApp> installed();

/* The same list, filtered by the account's malcontent app filter. No filter
 * means every program. */
std::vector<DeskApp> forUser(const QString &user = QString());
std::vector<DeskApp> forCurrentUser();

/* Whether the account has a filter, and its allowlist entries. False means
 * no filter (or accountsservice/malcontent isn't there). */
bool filterForUser(const QString &user, bool *allowlist, QStringList *entries);

/* Write the filter through accountsservice. The call may ask for an
 * administrator's password; `done` gets an empty string on success. */
void setFilterForUser(const QString &user, bool limited, const QStringList &entries,
	QObject *context, std::function<void(const QString &)> done);

QIcon iconFor(const DeskApp &app);

} // namespace AppCatalog
