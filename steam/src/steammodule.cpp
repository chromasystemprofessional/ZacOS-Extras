#include "steammodule.h"

#include <QImage>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <QStandardPaths>
#include <QTextStream>
#include <QProcess>

#include <algorithm>

namespace {

QImage steamIcon() {
	QImage image(16, 16, QImage::Format_ARGB32);
	image.fill(Qt::transparent);
	for (int y = 0; y < 16; ++y)
		for (int x = 0; x < 16; ++x)
			if ((x-8)*(x-8)+(y-8)*(y-8) < 49)
				image.setPixel(x, y, qRgb(0x1b, 0x28, 0x38));
	return image;
}

/* Flatpak keeps an app's active deployment here, user first (the way
 * ZacOS's Software installs apps) and then system-wide. */
QString flatpakApp(const QString &id) {
	QStringList roots;
	const QByteArray user = qgetenv("FLATPAK_USER_DIR");
	roots << (user.isEmpty() ? QDir::homePath() + "/.local/share/flatpak" : QString::fromLocal8Bit(user));
	const QByteArray system = qgetenv("FLATPAK_SYSTEM_DIR");
	roots << (system.isEmpty() ? QStringLiteral("/var/lib/flatpak") : QString::fromLocal8Bit(system));
	for (const QString &root : roots) {
		const QString app = root + "/app/" + id + "/current/active";
		if (QFileInfo(app).exists()) {
			return app;
		}
	}
	return QString();
}

/* Steam's own folders: a native install keeps games under ~/.local/share/Steam
 * (or ~/.steam), the Flatpak under its per-app home. */
QStringList steamApps(bool flatpak) {
	QStringList roots;
	if (flatpak) {
		roots << QDir::homePath() + "/.var/app/com.valvesoftware.Steam/.local/share/Steam";
	} else {
		roots << QDir::homePath() + "/.local/share/Steam"
			<< QDir::homePath() + "/.steam/steam"
			<< QDir::homePath() + "/.steam/debian-installation";
	}
	QStringList out;
	QSet<QString> seen;
	for (const QString &root : roots) {
		const QString apps = QFileInfo(root + "/steamapps").canonicalFilePath();
		if (!apps.isEmpty() && !seen.contains(apps)) {
			seen.insert(apps);
			out << apps;
		}
	}
	return out;
}

/* Extra libraries from libraryfolders.vdf: the current format names each
 * library's "path", the old one used "1" "/path". */
QStringList moreLibraries(const QString &steamapps) {
	QStringList out;
	QFile f(steamapps + "/libraryfolders.vdf");
	if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
		return out;
	}
	QTextStream in(&f);
	QString line;
	while (in.readLineInto(&line)) {
		line = line.trimmed();
		const QString key = line.section('"', 1, 1);
		const QString value = line.section('"', 3, 3);
		if (value.isEmpty()) {
			continue;
		}
		if (key == "path") {
			out << QDir(value).filePath("steamapps");
		} else if (!key.isEmpty() && key.toInt() > 0 && value.startsWith('/')) {
			out << QDir(value).filePath("steamapps"); /* the old format */
		}
	}
	return out;
}

QStringList libraries(bool flatpak) {
	QStringList roots = steamApps(flatpak);
	QSet<QString> seen;
	for (const QString &root : roots) {
		seen.insert(root);
	}
	for (int i = 0; i < roots.size(); ++i) {
		for (const QString &more : moreLibraries(roots[i])) {
			const QString apps = QFileInfo(more).canonicalFilePath();
			if (!apps.isEmpty() && !seen.contains(apps)) {
				seen.insert(apps);
				roots << apps;
			}
		}
	}
	return roots;
}

struct Manifest {
	QString id, name, state;
};

bool readManifest(const QString &path, Manifest *manifest) {
	QFile f(path);
	if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
		return false;
	}
	QTextStream in(&f);
	QString line;
	while (in.readLineInto(&line)) {
		const QString key = line.section('"', 1, 1);
		const QString value = line.section('"', 3, 3);
		if (key == "appid") {
			manifest->id = value;
		} else if (key == "name") {
			manifest->name = value;
		} else if (key == "StateFlags") {
			manifest->state = value;
		}
	}
	return true;
}

/* Steam's own tools install as apps too; they are not games. */
bool isSteamTool(const QString &name) {
	return name.startsWith("Proton") || name.startsWith("Steam Linux Runtime") ||
		name.startsWith("Steamworks Common Redistributables") || name.startsWith("SteamVR");
}

} // namespace

SteamModule::SteamModule(CollarExtensionService *service, QObject *parent)
	: QObject(parent), m_service(service) {
	m_service->modules = [this] { return QList<CollarModule>{ module() }; };
	m_service->activate = [this](const QString &, const QString &item, Done done) { activate(item, std::move(done)); };
	refresh();
	m_refresh.setInterval(30000);
	connect(&m_refresh, &QTimer::timeout, this, [this] { refresh(); m_service->changed(); });
	m_refresh.start();
}

void SteamModule::refresh() {
	m_native = !QStandardPaths::findExecutable("steam").isEmpty();
	m_flatpak = !flatpakApp("com.valvesoftware.Steam").isEmpty();
	m_steamInstalled = m_native || m_flatpak;
	m_games.clear();
	if (!m_steamInstalled) {
		return;
	}
	QSet<QString> seen;
	const auto add = [&](bool flatpak) {
		if (flatpak ? !m_flatpak : !m_native) {
			return;
		}
		for (const QString &root : libraries(flatpak)) {
			QDir dir(root);
			for (const QString &file : dir.entryList(QStringList() << "appmanifest_*.acf", QDir::Files)) {
				Manifest manifest;
				if (!readManifest(dir.filePath(file), &manifest) || manifest.id.isEmpty() ||
						manifest.name.isEmpty() || seen.contains(manifest.id)) {
					continue;
				}
				/* StateFlags' 4 bit means installed; other bits (an update
				 * pending, for one) don't make the game unplayable. */
				if (!manifest.state.isEmpty() && !(manifest.state.toInt() & 4)) {
					continue;
				}
				if (isSteamTool(manifest.name)) {
					continue;
				}
				seen.insert(manifest.id);
				m_games.append(Game{ manifest.id, manifest.name, flatpak });
			}
		}
	};
	add(false);
	add(true);
	std::sort(m_games.begin(), m_games.end(), [](const Game &a, const Game &b) {
		return a.name.localeAwareCompare(b.name) < 0;
	});
}

CollarModule SteamModule::module() const {
	CollarModule m;
	m.id = "steam";
	m.label = "Steam";
	m.active = m_steamInstalled;
	m.icon = steamIcon();
	if (!m_steamInstalled) {
		m.description = "Steam is not installed";
		m.menu << CollarMenuItem::text("Steam is not installed")
			<< CollarMenuItem::action("install", "Install Steam");
		return m;
	}
	m.description = m_games.size() == 1 ? QStringLiteral("1 game")
		: QString("%1 games").arg(m_games.size());
	m.menu << CollarMenuItem::action("open", "Open Steam");
	if (m_games.isEmpty()) {
		m.menu << CollarMenuItem::text("No installed games found");
	} else {
		m.menu << CollarMenuItem::separator();
		for (const Game &g : m_games) {
			m.menu << CollarMenuItem::action("launch:" + g.id, g.name);
		}
	}
	return m;
}

void SteamModule::activate(const QString &item, Done done) {
	if (item == "install") {
		/* Flatpak, as ZacOS's Software installs apps: Debian's steam
		 * package is not in its default sources. */
		if (!QStandardPaths::findExecutable("flatpak").isEmpty()) {
			QProcess::startDetached("flatpak", QStringList() << "--user" << "install" << "--noninteractive"
				<< "flathub" << "com.valvesoftware.Steam");
		} else {
			QProcess::startDetached("pkexec", QStringList() << "apt-get" << "install" << "-y" << "steam");
		}
		done(QString());
		return;
	}
	if (item == "open") {
		if (m_flatpak && !m_native) {
			QProcess::startDetached("flatpak", QStringList() << "run" << "com.valvesoftware.Steam");
		} else {
			QProcess::startDetached("steam", QStringList());
		}
		done(QString());
		return;
	}
	if (item.startsWith("launch:")) {
		const QString id = item.mid(7);
		bool flatpak = false;
		for (const Game &game : m_games) {
			if (game.id == id) {
				flatpak = game.flatpak;
				break;
			}
		}
		if (flatpak) {
			/* The Flatpak's own desktop entry starts Steam this way. */
			QProcess::startDetached("flatpak", QStringList() << "run" << "--command=/app/bin/steam"
				<< "com.valvesoftware.Steam" << ("steam://rungameid/" + id));
		} else {
			QProcess::startDetached("steam", QStringList() << ("steam://rungameid/" + id));
		}
		done(QString());
		return;
	}
	done(QString());
}
