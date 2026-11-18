/* GIO before Qt: its headers use the name `signals`. */
#include <gio/gdesktopappinfo.h>
#include <gio/gio.h>
#include <sys/utsname.h>
#include <unistd.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>

#include "apps.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QDBusVariant>
#include <QFileInfo>
#include <QSet>
#include <QStandardPaths>

namespace {

const QString kAccounts = QStringLiteral("org.freedesktop.Accounts");
const QString kAccountsRoot = QStringLiteral("/org/freedesktop/Accounts");
const QString kProperties = QStringLiteral("org.freedesktop.DBus.Properties");
const QString kMct = QStringLiteral("com.endlessm.ParentalControls.AppFilter");

QDBusConnection bus() {
	return qEnvironmentVariable("ZACOS9_ACCOUNTS_BUS") == QStringLiteral("session")
		? QDBusConnection::sessionBus() : QDBusConnection::systemBus();
}

QString userPath(const QString &user) {
	QDBusMessage message = QDBusMessage::createMethodCall(kAccounts, kAccountsRoot, kAccounts,
		user.isEmpty() ? QStringLiteral("FindUserById") : QStringLiteral("FindUserByName"));
	if (user.isEmpty()) {
		message << static_cast<qlonglong>(getuid());
	} else {
		message << user;
	}
	const QDBusMessage reply = bus().call(message, QDBus::Block, 2000);
	if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty()) {
		return QString();
	}
	return reply.arguments().first().value<QDBusObjectPath>().path();
}

QString iconNameFor(GDesktopAppInfo *app) {
	GIcon *icon = g_app_info_get_icon(G_APP_INFO(app));
	if (!icon) {
		return QString();
	}
	if (G_IS_THEMED_ICON(icon)) {
		const gchar *const *names = g_themed_icon_get_names(G_THEMED_ICON(icon));
		for (int i = 0; names && names[i]; i++) {
			if (*names[i]) {
				return QString::fromUtf8(names[i]);
			}
		}
	}
	if (G_IS_FILE_ICON(icon)) {
		GFile *file = g_file_icon_get_file(G_FILE_ICON(icon));
		char *path = g_file_get_path(file);
		const QString name = path ? QString::fromUtf8(path) : QString();
		g_free(path);
		return name;
	}
	return QString();
}

QString programOf(GDesktopAppInfo *app) {
	const char *exec = g_app_info_get_commandline(G_APP_INFO(app));
	char **argv = nullptr;
	if (!exec || !g_shell_parse_argv(exec, nullptr, &argv, nullptr)) {
		return QString();
	}
	int i = 0;
	if (argv[0] && g_strcmp0(argv[0], "env") == 0) {
		for (i = 1; argv[i] && (strchr(argv[i], '=') || argv[i][0] == '-'); i++) {
		}
	}
	char *path = argv[i] ? g_find_program_in_path(argv[i]) : nullptr;
	g_strfreev(argv);
	if (!path) {
		return QString();
	}
	char *real = realpath(path, nullptr);
	const QString out = real ? QString::fromUtf8(real) : QString::fromUtf8(path);
	free(real);
	g_free(path);
	return out;
}

QString entryFor(GDesktopAppInfo *app) {
	char *flatpak = g_desktop_app_info_get_string(app, "X-Flatpak");
	if (flatpak && *flatpak) {
		struct utsname u;
		uname(&u);
		const char *arch = strcmp(u.machine, "aarch64") == 0 ? "aarch64" : "x86_64";
		const QString entry = QStringLiteral("app/%1/%2/stable")
			.arg(QString::fromUtf8(flatpak), QString::fromUtf8(arch));
		g_free(flatpak);
		return entry;
	}
	g_free(flatpak);
	return programOf(app);
}

QStringList commandFor(GDesktopAppInfo *app) {
	const char *exec = g_app_info_get_commandline(G_APP_INFO(app));
	char **argv = nullptr;
	if (!exec || !g_shell_parse_argv(exec, nullptr, &argv, nullptr)) {
		return {};
	}
	const QString appName = QString::fromUtf8(g_app_info_get_name(G_APP_INFO(app)));
	const QString icon = iconNameFor(app);
	const char *desktop = g_desktop_app_info_get_filename(app);
	const QString desktopFile = desktop ? QString::fromUtf8(desktop) : QString();
	QStringList out;
	for (int i = 0; argv[i]; i++) {
		QString a = QString::fromUtf8(argv[i]);
		if (a == QStringLiteral("%f") || a == QStringLiteral("%F") ||
				a == QStringLiteral("%u") || a == QStringLiteral("%U")) {
			continue;
		}
		if (a == QStringLiteral("%i")) {
			if (!icon.isEmpty()) {
				out << QStringLiteral("--icon") << icon;
			}
			continue;
		}
		a.replace(QStringLiteral("%%"), QStringLiteral("\x01"));
		a.replace(QStringLiteral("%i"), icon);
		a.replace(QStringLiteral("%c"), appName);
		a.replace(QStringLiteral("%k"), desktopFile);
		a.remove(QStringLiteral("%f"));
		a.remove(QStringLiteral("%F"));
		a.remove(QStringLiteral("%u"));
		a.remove(QStringLiteral("%U"));
		a.replace(QChar('\x01'), QChar('%'));
		if (!a.isEmpty()) {
			out << a;
		}
	}
	g_strfreev(argv);
	if (out.isEmpty()) {
		return out;
	}
	if (g_desktop_app_info_get_boolean(app, "Terminal")) {
		for (const char *candidate : { "x-terminal-emulator", "qterminal", "foot", "xterm" }) {
			const QString terminal = QStandardPaths::findExecutable(QString::fromLatin1(candidate));
			if (!terminal.isEmpty()) {
				out.prepend(QStringLiteral("-e"));
				out.prepend(terminal);
				break;
			}
		}
	}
	return out;
}

bool listed(const QString &entry, const QStringList &entries) {
	if (entries.contains(entry)) {
		return true;
	}
	/* Flatpak entries: the same app ID, whatever the architecture/branch. */
	if (!entry.startsWith(QStringLiteral("app/"))) {
		return false;
	}
	const QString id = entry.section('/', 1, 1);
	for (const QString &e : entries) {
		if (e.startsWith(QStringLiteral("app/")) && e.section('/', 1, 1) == id) {
			return true;
		}
	}
	return false;
}

std::vector<DeskApp> filterList(const std::vector<DeskApp> &apps, const QString &user) {
	bool allowlist = false;
	QStringList entries;
	if (!AppCatalog::filterForUser(user, &allowlist, &entries)) {
		return apps;
	}
	std::vector<DeskApp> out;
	for (const DeskApp &app : apps) {
		if (app.entry.isEmpty()) {
			if (!allowlist) {
				out.push_back(app);
			}
			continue;
		}
		if (listed(app.entry, entries) == allowlist) {
			out.push_back(app);
		}
	}
	return out;
}

} // namespace

namespace AppCatalog {

std::vector<DeskApp> installed() {
	std::vector<DeskApp> apps;
	QSet<QString> seen;
	GList *all = g_app_info_get_all();
	for (GList *l = all; l; l = l->next) {
		auto *info = static_cast<GAppInfo *>(l->data);
		if (!G_IS_DESKTOP_APP_INFO(info) || !g_app_info_should_show(info)) {
			continue;
		}
		auto *desktop = G_DESKTOP_APP_INFO(info);
		const QString entry = entryFor(desktop);
		if (entry.isEmpty() || seen.contains(entry)) {
			continue;
		}
		const QString base = QFileInfo(entry).fileName();
		/* ZacOS's user programs belong on the panel (Calculator, Jigsaw,
		 * Notepad and the rest); the shell, control panels, installers and
		 * helpers do not. */
		static const char *const system_programs[] = {
			"zacos9-controlpanel", "zacos9-collarpanel", "zacos9-finder",
			"zacos9-frontdesk", "zacos9-frontdesk-setup", "zacos9-menubar",
			"zacos9-collar", "zacos9-session", "zacos9-greeter", "zacos9-lock",
			"zacos9-screensaver", "zacos9-keyboard", "zacos9-inhibit",
			"zacos9-setup", "zacos9-installer", "zacos9-update", "zacos9-store",
			"zacos9-disksetup", "zacos9-themeinstaller", "zacos9-wininstall",
			"zacos9-classic", "zacos9-snapshot", "zacos9-appinstall",
			"zacos9-gtkmenu", "zacos9-web-sharing", "zacos9-disk-helper",
			"zacos9-accounts-helper", "zacos9-appstore-helper",
		};
		bool system = false;
		for (const char *name : system_programs) {
			if (base == QString::fromLatin1(name)) {
				system = true;
				break;
			}
		}
		if (system) {
			continue;
		}
		const QString name = QString::fromUtf8(g_app_info_get_name(info));
		const QStringList command = commandFor(desktop);
		if (name.isEmpty() || command.isEmpty()) {
			continue;
		}
		const char *file = g_desktop_app_info_get_filename(desktop);
		char *working = g_desktop_app_info_get_string(desktop, "Path");
		DeskApp app;
		app.name = name;
		app.desktopFile = file ? QString::fromUtf8(file) : QString();
		app.iconName = iconNameFor(desktop);
		app.entry = entry;
		app.workingDir = working ? QString::fromUtf8(working) : QString();
		app.command = command;
		app.terminal = g_desktop_app_info_get_boolean(desktop, "Terminal");
		g_free(working);
		seen.insert(entry);
		apps.push_back(app);
	}
	g_list_free_full(all, g_object_unref);
	std::sort(apps.begin(), apps.end(), [](const DeskApp &a, const DeskApp &b) {
		return a.name.compare(b.name, Qt::CaseInsensitive) < 0;
	});
	return apps;
}

bool filterForUser(const QString &user, bool *allowlist, QStringList *entries) {
	const QString path = userPath(user);
	if (path.isEmpty()) {
		return false;
	}
	QDBusMessage message = QDBusMessage::createMethodCall(kAccounts, path, kProperties,
		QStringLiteral("Get"));
	message << kMct << QStringLiteral("AppFilter");
	const QDBusMessage reply = bus().call(message, QDBus::Block, 2000);
	if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty()) {
		return false;
	}
	const QVariant value = reply.arguments().first().value<QDBusVariant>().variant();
	if (!value.canConvert<QDBusArgument>()) {
		return false;
	}
	const QDBusArgument arg = value.value<QDBusArgument>();
	bool allow = false;
	QStringList list;
	arg.beginStructure();
	arg >> allow >> list;
	arg.endStructure();
	if (allowlist) {
		*allowlist = allow;
	}
	if (entries) {
		*entries = list;
	}
	return true;
}

std::vector<DeskApp> forUser(const QString &user) {
	return filterList(installed(), user);
}

std::vector<DeskApp> forCurrentUser() {
	return forUser(QString());
}

void setFilterForUser(const QString &user, bool limited, const QStringList &entries,
		QObject *context, std::function<void(const QString &)> done) {
	const QString path = userPath(user);
	if (path.isEmpty()) {
		if (done) {
			done(QStringLiteral("That account isn't known to accountsservice."));
		}
		return;
	}
	QDBusArgument arg;
	arg.beginStructure();
	arg << limited << (limited ? entries : QStringList());
	arg.endStructure();
	QDBusMessage message = QDBusMessage::createMethodCall(kAccounts, path, kProperties,
		QStringLiteral("Set"));
	message << kMct << QStringLiteral("AppFilter")
		<< QVariant::fromValue(QDBusVariant(QVariant::fromValue(arg)));
	message.setInteractiveAuthorizationAllowed(true);
	auto *watcher = new QDBusPendingCallWatcher(bus().asyncCall(message, 10 * 60 * 1000),
		context);
	QObject::connect(watcher, &QDBusPendingCallWatcher::finished, context,
		[watcher, done](QDBusPendingCallWatcher *self) {
			const QDBusMessage reply = self->reply();
			QString error;
			if (reply.type() == QDBusMessage::ErrorMessage) {
				if (reply.errorName() == QStringLiteral("org.freedesktop.DBus.Error.UnknownInterface") ||
						reply.errorName() == QStringLiteral("org.freedesktop.DBus.Error.UnknownProperty")) {
					error = QStringLiteral("Limited accounts need malcontent, which isn't installed.");
				} else if (reply.errorName().endsWith(QStringLiteral("PermissionDenied")) ||
						reply.errorName().endsWith(QStringLiteral("AccessDenied")) ||
						reply.errorName().endsWith(QStringLiteral("NotAuthorized"))) {
					error = QStringLiteral("You need an administrator's name and password for that.");
				} else {
					error = reply.errorMessage().isEmpty() ? reply.errorName() : reply.errorMessage();
				}
			}
			self->deleteLater();
			if (done) {
				done(error);
			}
		});
}

QIcon iconFor(const DeskApp &app) {
	if (!app.iconName.isEmpty()) {
		QIcon icon = QIcon::fromTheme(app.iconName);
		if (!icon.isNull()) {
			return icon;
		}
		if (QFileInfo(app.iconName).isFile()) {
			icon = QIcon(app.iconName);
			if (!icon.isNull()) {
				return icon;
			}
		}
	}
	QIcon icon = QIcon::fromTheme(QStringLiteral("application-x-executable"));
	return icon;
}

} // namespace AppCatalog
