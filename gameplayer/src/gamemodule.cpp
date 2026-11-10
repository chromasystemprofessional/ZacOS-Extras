#include "gamemodule.h"

#include <QImage>
#include <QProcess>
#include <QCoreApplication>
#include <QHostAddress>
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

GameModule::GameModule(CollarExtensionService *service, const QString &game,
		gameplayer::System system, bool fullscreen, int volume, QProcess *engine, QObject *parent)
	: QObject(parent), m_service(service), m_game(game), m_system(system), m_fullscreen(fullscreen), m_volume(volume), m_engine(engine) {
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
	/* The engine's PulseAudio stream appears shortly after it starts. Apply
	 * the saved volume once it does, retrying for a few seconds. */
	m_volumeTimer.setInterval(500);
	connect(&m_volumeTimer, &QTimer::timeout, this, [this] {
		if (++m_volumeAttempts > 20) {
			m_volumeTimer.stop();
			return;
		}
		applyVolume();
	});
	m_volumeTimer.start();
}

void GameModule::refreshControllers() {
	m_controllers = gameplayer::detectedControllers();
}

/* Change the running engine's own PulseAudio stream, so the volume can be
 * adjusted without stopping the game. The stream appears shortly after the
 * engine starts, so the caller retries until it is found. */
void GameModule::applyVolume() {
	if (!m_engine || m_engine->processId() <= 0) {
		return;
	}
	const qint64 pid = m_engine->processId();
	auto *query = new QProcess(this);
	connect(query, &QProcess::errorOccurred, query, &QProcess::deleteLater);
	connect(query, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
		[this, query, pid](int code, QProcess::ExitStatus status) {
			const QByteArray output = query->readAllStandardOutput();
			query->deleteLater();
			if (code != 0 || status != QProcess::NormalExit) {
				return;
			}
			const QString text = QString::fromUtf8(output);
			const QString wanted = QString("application.process.id = \"%1\"").arg(pid);
			QString input;
			int at = 0;
			while ((at = text.indexOf("Sink Input #", at)) >= 0) {
				const int lineEnd = text.indexOf(QLatin1Char('\n'), at);
				const int next = lineEnd < 0 ? -1 : text.indexOf("\nSink Input #", lineEnd);
				const QString block = text.mid(lineEnd, next < 0 ? -1 : next - lineEnd);
				const int numberEnd = lineEnd < 0 ? text.size() : lineEnd;
				const QString number = text.mid(at + 12, numberEnd - at - 12).trimmed();
				if (block.contains(wanted)) {
					input = number;
					break;
				}
				if (lineEnd < 0) {
					break;
				}
				at = lineEnd + 1;
			}
			if (input.isEmpty()) {
				return;
			}
			if (QProcess::startDetached("pactl", { "set-sink-input-volume", input,
					QString::number(m_volume) + "%" })) {
				m_volumeTimer.stop();
			}
		});
	query->start("pactl", { "list", "sink-inputs" });
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
	const QStringList maps = gameplayer::controllerMappings();
	if (m_controllers.isEmpty()) {
		m.menu << CollarMenuItem::text("Controller: none detected");
	} else {
		m.menu << CollarMenuItem::text("Controllers");
		for (int i = 0; i < m_controllers.size(); ++i) {
			const bool configured = i < maps.size();
			m.menu << CollarMenuItem::text("  " + m_controllers.at(i) +
				(configured ? " (configured)" : " (detected)"));
		}
	}
	m.menu << CollarMenuItem::separator();
	if (m_system != gameplayer::System::Ps3) {
		/* RPCS3 keeps its own fullscreen setting. */
		m.menu << CollarMenuItem::action("fullscreen", "Fullscreen", true, m_fullscreen);
	}
	m.menu << CollarMenuItem::separator()
		<< CollarMenuItem::text(QString("Volume: %1%").arg(m_volume))
		<< CollarMenuItem::action("volume-down", "Volume Down")
		<< CollarMenuItem::action("volume-up", "Volume Up")
		<< CollarMenuItem::separator()
		<< CollarMenuItem::action("configure", "Configure Controller...")
		<< CollarMenuItem::separator()
		<< CollarMenuItem::action("quit", "Quit Game");
	return m;
}

void GameModule::activate(const QString &item, Done done) {
	if (item == "volume-down" || item == "volume-up") {
		const int step = item == "volume-up" ? 10 : -10;
		m_volume = qBound(0, m_volume + step, 150);
		if (volumeChanged) {
			volumeChanged(m_volume);
		}
		m_volumeAttempts = 0;
		applyVolume();
		m_volumeTimer.start();
		m_service->changed();
		done(QString());
		return;
	}
	if (item == "fullscreen" && m_system == gameplayer::System::Ps3) {
		done("RPCS3 keeps its own fullscreen setting.");
		return;
	}
	if (item == "fullscreen") {
		m_fullscreen = !m_fullscreen;
		if (fullscreenChanged) {
			fullscreenChanged(m_fullscreen);
		}
		const bool retro = (m_system == gameplayer::System::Nes || m_system == gameplayer::System::Snes ||
			m_system == gameplayer::System::Sega || m_system == gameplayer::System::GameBoy ||
			m_system == gameplayer::System::Gba);
		if (retro) {
			/* RetroArch: toggle instantly via its network command interface. */
			QUdpSocket sock;
			sock.writeDatagram("FULLSCREEN_TOGGLE",
				QHostAddress("127.0.0.1"), 55355);
			sock.flush();
			m_service->changed();
			done(QString());
			return;
		}
		m_restartRequested = true;
		if (m_engine && m_engine->state() != QProcess::NotRunning) {
			m_engine->terminate();
			m_kill.start();
		}
		m_service->changed();
		done(QString());
		return;
	}
	if (item == "configure") {
		const bool ok = QProcess::startDetached(QCoreApplication::applicationFilePath(),
			QStringList() << "--configure-controller");
		done(ok ? QString() : "Couldn't start the controller configuration.");
		return;
	}
	if (item == "quit") {
		m_quitRequested = true;
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