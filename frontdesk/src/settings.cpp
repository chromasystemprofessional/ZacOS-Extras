#include "settings.h"

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTextStream>

#include <pwd.h>
#include <unistd.h>

QString FrontDeskSettings::configDir() {
	const QByteArray configured = qgetenv("ZACOS9_ACCOUNTS_CONF");
	return configured.isEmpty() ? QStringLiteral("/etc/zacos9") : QString::fromUtf8(configured);
}

QString FrontDeskSettings::currentUserName() {
	if (const struct passwd *pw = getpwuid(getuid())) {
		return QString::fromLocal8Bit(pw->pw_name);
	}
	return QString();
}

FrontDeskSettings FrontDeskSettings::load(const QString &user) {
	FrontDeskSettings s;
	s.user = user.isEmpty() ? currentUserName() : user;
	const QByteArray name = s.user.toLocal8Bit();
	struct passwd *pw = getpwnam(name.constData());
	if (!pw && s.user == currentUserName()) {
		pw = getpwuid(getuid());
	}
	if (pw) {
		s.user = QString::fromLocal8Bit(pw->pw_name);
		s.home = QString::fromLocal8Bit(pw->pw_dir);
		const QString gecos = QString::fromLocal8Bit(pw->pw_gecos).section(',', 0, 0).trimmed();
		s.displayName = gecos.isEmpty() ? s.user : gecos;
	} else {
		s.displayName = s.user;
	}
	if (s.home.isEmpty()) {
		s.home = QDir::homePath();
	}

	QFile file(QDir(configDir()).filePath(QStringLiteral("frontdesk/") + s.user));
	if (!file.open(QIODevice::ReadOnly)) {
		return s;
	}
	s.frontDesk = true;
	while (!file.atEnd()) {
		const QString line = QString::fromUtf8(file.readLine()).trimmed();
		const int eq = line.indexOf('=');
		if (eq <= 0) {
			continue;
		}
		const QString key = line.left(eq);
		const QString value = line.mid(eq + 1).trimmed();
		if (key == QStringLiteral("menus")) {
			if (value == QStringLiteral("minimal") || value == QStringLiteral("restricted") ||
					value == QStringLiteral("all")) {
				s.menus = value;
			}
		} else if (key == QStringLiteral("save")) {
			if (value == QStringLiteral("anywhere") || value == QStringLiteral("user")) {
				s.save = value;
			}
		} else if (key == QStringLiteral("save-removable")) {
			s.saveRemovable = value == QStringLiteral("on");
		} else if (key == QStringLiteral("finder")) {
			s.finder = value == QStringLiteral("on");
		} else if (key == QStringLiteral("speech")) {
			s.speech = value == QStringLiteral("on");
		} else if (key == QStringLiteral("capture")) {
			s.capture = value == QStringLiteral("on");
		} else if (key == QStringLiteral("zac-menu")) {
			s.zacMenu = value == QStringLiteral("on");
		} else if (key == QStringLiteral("screens")) {
			s.eachScreen = value == QStringLiteral("each");
		}
	}
	return s;
}

QString frontDeskFoldersFile() {
	const QByteArray override = qgetenv("ZACOS9_FRONTDESK_FOLDERS");
	if (!override.isEmpty()) {
		return QString::fromUtf8(override);
	}
	return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) +
		QStringLiteral("/zacos9/frontdesk-folders");
}

QStringList frontDeskFolders() {
	const QString file = frontDeskFoldersFile();
	if (!QFile::exists(file)) {
		return { QDir::homePath() };
	}
	QStringList paths;
	QFile input(file);
	if (!input.open(QIODevice::ReadOnly | QIODevice::Text)) {
		return paths;
	}
	while (!input.atEnd()) {
		QString line = QString::fromUtf8(input.readLine()).trimmed();
		if (line.isEmpty() || line.startsWith('#')) {
			continue;
		}
		if (line.startsWith(QStringLiteral("~/"))) {
			line = QDir::home().filePath(line.mid(2));
		}
		const QString path = QFileInfo(line).absoluteFilePath();
		if (!path.isEmpty() && !paths.contains(path)) {
			paths << path;
		}
	}
	return paths;
}

bool setFrontDeskFolders(const QStringList &paths) {
	const QFileInfo file(frontDeskFoldersFile());
	if (!QDir().mkpath(file.absolutePath())) {
		return false;
	}
	QSaveFile output(file.absoluteFilePath());
	if (!output.open(QIODevice::WriteOnly | QIODevice::Text)) {
		return false;
	}
	QTextStream stream(&output);
	for (const QString &path : paths) {
		stream << QDir::cleanPath(path) << '\n';
	}
	return output.commit();
}
