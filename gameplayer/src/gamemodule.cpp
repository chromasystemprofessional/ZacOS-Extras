#include "gamemodule.h"

#include <QImage>

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

GameModule::GameModule(CollarExtensionService *service, const QString &game,
		gameplayer::System system, QProcess *engine, QObject *parent)
	: QObject(parent), m_service(service), m_game(game), m_system(system), m_engine(engine) {
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
	refreshControllers();
	m_refresh.setInterval(5000);
	connect(&m_refresh, &QTimer::timeout, this, [this] {
		const QStringList now = gameplayer::detectedControllers();
		if (now != m_controllers) {
			m_controllers = now;
			m_service->changed();
		}
	});
	m_refresh.start();
}

void GameModule::refreshControllers() {
	m_controllers = gameplayer::detectedControllers();
}

CollarModule GameModule::module() const {
	CollarModule m;
	m.id = "gameplayer";
	m.label = "Game Player";
	m.active = true;
	const QString sysName = gameplayer::systemName(m_system);
	m.description = (sysName.isEmpty() || sysName == "unknown") ? m_game : (m_game + " (" + sysName + ")");
	m.icon = moduleIcon();
	m.menu << CollarMenuItem::text(m_game.isEmpty() ? "Playing a game" : m_game);
	if (!(sysName.isEmpty() || sysName == "unknown")) {
		m.menu << CollarMenuItem::text(sysName);
	}
	m.menu << CollarMenuItem::separator();
	if (m_controllers.isEmpty()) {
		m.menu << CollarMenuItem::text("Controller: none detected");
	} else {
		m.menu << CollarMenuItem::text("Controllers");
		for (const QString &c : m_controllers) {
			m.menu << CollarMenuItem::text("  " + c);
		}
	}
	m.menu << CollarMenuItem::separator()
		<< CollarMenuItem::action("quit", "Quit Game");
	return m;
}

void GameModule::activate(const QString &item, Done done) {
	if (item == "quit") {
		/* Terminate the engine; if it ignores that, finish it off. The program
		 * quits through the engine's finished signal, so progress saved on exit
		 * (RetroArch's auto-save) has a chance to be written. */
		if (m_engine && m_engine->state() != QProcess::NotRunning) {
			m_engine->terminate();
			m_kill.start();
		}
		done(QString());
	} else {
		done(QString());
	}
}