#include "steammodule.h"

#include <QImage>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QStandardPaths>
#include <QProcess>

static QImage steamIcon() {
	QImage image(16, 16, QImage::Format_ARGB32);
	image.fill(Qt::transparent);
	for (int y = 0; y < 16; ++y)
		for (int x = 0; x < 16; ++x)
			if ((x-8)*(x-8)+(y-8)*(y-8) < 49)
				image.setPixel(x, y, qRgb(0x1b, 0x28, 0x38));
	return image;
}

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
	m_steamInstalled = !QStandardPaths::findExecutable("steam").isEmpty();
	m_games.clear();
	if (!m_steamInstalled) return;
	QStringList roots;
	const QString home = QDir::homePath();
	roots << home + "/.steam/steam/steamapps"
		<< home + "/.local/share/Steam/steamapps"
		<< home + "/.steam/debian-installation/steamapps";
	for (const QString &root : roots) {
		QDir dir(root);
		const QStringList acfs = dir.entryList(QStringList() << "appmanifest_*.acf", QDir::Files);
		for (const QString &acf : acfs) {
			QFile f(dir.filePath(acf));
			if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) continue;
			QTextStream in(&f);
			QString id, name, state;
			QString line;
			while (!in.atEnd()) {
				line = in.readLine();
				if (line.contains("\"appid\"")) { id = line.section('"', 3, 3); }
				else if (line.contains("\"name\"")) { name = line.section('"', 3, 3); }
				else if (line.contains("\"StateFlags\"")) { state = line.section('"', 3, 3); }
			}
			f.close();
			if (id.isEmpty() || name.isEmpty()) continue;
			// StateFlags 4 = fully installed
			if (!state.isEmpty() && state != "4") continue;
			Game g; g.id = id; g.name = name;
			m_games.append(g);
		}
	}
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
	m.description = QString("%1 games").arg(m_games.size());
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
		QProcess::startDetached("pkexec", QStringList() << "apt-get" << "install" << "-y" << "steam");
		done(QString());
		return;
	}
	if (item == "open") {
		QProcess::startDetached("steam", QStringList());
		done(QString());
		return;
	}
	if (item.startsWith("launch:")) {
		const QString id = item.mid(7);
		QProcess::startDetached("steam", QStringList() << ("steam://rungameid/" + id));
		done(QString());
		return;
	}
	done(QString());
}