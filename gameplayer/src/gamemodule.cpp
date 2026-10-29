#include "gamemodule.h"

#include <QDir>
#include <QImage>
#include <QUdpSocket>

/* 16 x 16: a game cartridge, drawn the way The Collar's other modules draw
 * their icons. */
static QImage moduleIcon() {
	static const char *const art[16] = {
		"................",
		"................",
		"...KKKKKKKKKK...",
		"...KWWWWWWWWK...",
		"...K00000000K...",
		"...K0eeeeee0K...",
		"...K0eeeeee0K...",
		"...K00000000K...",
		"...KWWWWWWWWK...",
		"...KccccccccK...",
		"...KccccccccK...",
		"...KccccccccK...",
		"...K99999999K...",
		"...KKKKKKKKKK...",
		"................",
		"................",
	};
	QImage image(16, 16, QImage::Format_ARGB32);
	image.fill(Qt::transparent);
	for (int y = 0; y < 16; ++y) {
		for (int x = 0; x < 16; ++x) {
			switch (art[y][x]) {
			case 'K': image.setPixel(x, y, qRgb(0, 0, 0)); break;
			case 'W': image.setPixel(x, y, qRgb(255, 255, 255)); break;
			case '0': image.setPixel(x, y, qRgb(0, 0, 0)); break;
			case 'e': image.setPixel(x, y, qRgb(0xE0, 0xE0, 0xE0)); break;
			case 'c': image.setPixel(x, y, qRgb(0xC0, 0xC0, 0xC0)); break;
			case '9': image.setPixel(x, y, qRgb(0x90, 0x90, 0x90)); break;
			}
		}
	}
	return image;
}

static bool isRetroArchSystem(gameplayer::System s) {
	return s == gameplayer::System::Nes || s == gameplayer::System::Snes ||
		s == gameplayer::System::Sega || s == gameplayer::System::GameBoy ||
		s == gameplayer::System::Gba;
}

GameModule::GameModule(CollarExtensionService *service, const QString &game,
		gameplayer::System system, bool fullscreen, QProcess *engine, QObject *parent)
	: QObject(parent), m_service(service), m_game(game), m_system(system),
	  m_fullscreen(fullscreen), m_engine(engine) {
	m_service->modules = [this] { return QList<CollarModule>{ module() }; };
	m_service->activate = [this](const QString &, const QString &item, Done done) {
		activate(item, std::move(done));
	};
	m_kill.setSingleShot(true);
	m_kill.setInterval(3000);
	connect(&m_kill, &QTimer::timeout, this, [this] {
		if (m_engine && m_engine->state() != QProcess::NotRunning) {
			m_engine->kill();
		}
	});
}

CollarModule GameModule::module() const {
	CollarModule m;
	m.id = "gameplayer";
	m.label = "Game Player";
	m.active = true;
	m.description = m_game.isEmpty() ? QString("Playing a game")
		: (m_game + " (" + gameplayer::systemName(m_system) + ")");
	m.icon = moduleIcon();
	m.menu << CollarMenuItem::text(m_game.isEmpty() ? "Playing a game" : m_game)
		<< CollarMenuItem::text(gameplayer::systemName(m_system))
		<< CollarMenuItem::separator()
		<< CollarMenuItem::action("fullscreen", "Fullscreen", true, m_fullscreen)
		<< CollarMenuItem::action("save", "Save State")
		<< CollarMenuItem::action("load", "Load State")
		<< CollarMenuItem::action("opensaves", "Open Saves Folder")
		<< CollarMenuItem::separator()
		<< CollarMenuItem::action("quit", "Quit Game");
	return m;
}

void GameModule::activate(const QString &item, Done done) {
	if (item == "fullscreen") {
		m_fullscreen = !m_fullscreen;
		if (fullscreenChanged) {
			fullscreenChanged(m_fullscreen);
		}
		if (isRetroArchSystem(m_system)) {
			/* RetroArch: toggle instantly via its network command interface
			 * (no restart, no state loss). */
			QUdpSocket sock;
			sock.writeDatagram("FULLSCREEN_TOGGLE",
				QHostAddress("127.0.0.1"), 55355);
			sock.flush();
			m_service->changed();
			done(QString());
		} else {
			/* mednafen / mupen64plus: restart the engine in the other mode. */
			m_restartRequested = true;
			if (m_engine && m_engine->state() != QProcess::NotRunning) {
				m_engine->terminate();
				m_kill.start();
			}
			done(QString());
		}
	} else if (item == "save") {
		if (isRetroArchSystem(m_system)) {
			QUdpSocket sock;
			sock.writeDatagram("SAVE_STATE", QHostAddress("127.0.0.1"), 55355);
			sock.flush();
		}
		done(QString());
	} else if (item == "load") {
		if (isRetroArchSystem(m_system)) {
			QUdpSocket sock;
			sock.writeDatagram("LOAD_STATE", QHostAddress("127.0.0.1"), 55355);
			sock.flush();
		}
		done(QString());
	} else if (item == "opensaves") {
		const QString saveDir = QDir::homePath() + "/Games/Saved Games";
		QDir().mkpath(saveDir);
		QProcess::startDetached("xdg-open", QStringList() << saveDir);
		done(QString());
	} else if (item == "quit") {
		m_quitRequested = true;
		if (m_engine && m_engine->state() != QProcess::NotRunning) {
			m_engine->terminate();
			m_kill.start();
		}
		done(QString());
	} else {
		done(QString());
	}
}