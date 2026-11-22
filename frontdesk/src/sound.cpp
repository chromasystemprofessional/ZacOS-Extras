#include "sound.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QStringList>

namespace {

QString configPath() {
	const QByteArray configured = qgetenv("XDG_CONFIG_HOME");
	const QString base = configured.isEmpty() ? QDir::homePath() + QStringLiteral("/.config")
		: QString::fromLocal8Bit(configured);
	return base + QStringLiteral("/zacos9/desktop.conf");
}

/* The Sound control panel's interface volume, 0 to 7; the same default as
 * the desktop (5). Front Desk's own Sound menu is on top of this. */
int interfaceVolume() {
	QFile file(configPath());
	if (!file.open(QIODevice::ReadOnly)) {
		return 5;
	}
	while (!file.atEnd()) {
		const QString line = QString::fromUtf8(file.readLine()).trimmed();
		const int eq = line.indexOf(QLatin1Char('='));
		if (eq <= 0 || line.left(eq) != QStringLiteral("interface-volume")) {
			continue;
		}
		bool ok = false;
		const int value = line.mid(eq + 1).trimmed().toInt(&ok);
		return ok ? qBound(0, value, 7) : 5;
	}
	return 5;
}

QString soundDirectory() {
	QStringList roots;
	const QByteArray data = qgetenv("ZACOS9_DATA");
	if (!data.isEmpty()) {
		roots << QString::fromLocal8Bit(data);
	}
	const QString dataHome = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
	if (!dataHome.isEmpty()) {
		roots << dataHome + QStringLiteral("/zacos9");
	}
	roots << QStringLiteral("/usr/local/share/zacos9")
		<< QStringLiteral("/usr/share/zacos9");
	for (const QString &root : roots) {
		const QString dir = root + QStringLiteral("/sounds");
		if (QFileInfo(dir + QStringLiteral("/woodblock.wav")).isFile()) {
			return dir;
		}
	}
	return {};
}

QString soundPlayer() {
	for (const char *name : { "paplay", "pw-play", "aplay" }) {
		const QString path = QStandardPaths::findExecutable(QString::fromLatin1(name));
		if (!path.isEmpty()) {
			return path;
		}
	}
	return {};
}

} // namespace

FrontDeskSound::FrontDeskSound() : m_dir(soundDirectory()), m_player(soundPlayer()) {}

void FrontDeskSound::click() {
	play(QStringLiteral("woodblock.wav"));
}

void FrontDeskSound::open() {
	play(QStringLiteral("chirp.wav"));
}

void FrontDeskSound::alert() {
	play(QStringLiteral("platinum.wav"));
}

void FrontDeskSound::play(const QString &file) {
	if (m_dir.isEmpty() || m_player.isEmpty()) {
		return;
	}
	const int level = interfaceVolume();
	if (level <= 0) {
		return;
	}
	const QString path = m_dir + QLatin1Char('/') + file;
	if (!QFileInfo(path).isFile()) {
		return;
	}
	QStringList arguments;
	if (m_player.endsWith(QStringLiteral("paplay"))) {
		arguments << QStringLiteral("--volume=%1").arg(65536 * level / 7);
	} else if (m_player.endsWith(QStringLiteral("pw-play"))) {
		arguments << QStringLiteral("--volume")
			<< QString::number(level / 7.0, 'f', 3);
	} else {
		arguments << QStringLiteral("-q");
	}
	arguments << path;
	QProcess::startDetached(m_player, arguments);
}
