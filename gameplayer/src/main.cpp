/* zacos9-gameplayer: run a console game file (cartridge or disc) in ZacOS 9. */

#include <QApplication>
#include <QFile>
#include <QFileInfo>
#include <QMessageBox>
#include <QProcess>
#include <QProcessEnvironment>
#include <QSet>
#include <QMap>
#include <QDir>
#include <QFile>
#include <QSettings>
#include <QStandardPaths>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "collarextension.h"
#include "gameplayer.h"
#include "gamemodule.h"

#include <SDL2/SDL.h>
#include <linux/joystick.h>
#include <fcntl.h>
#include <unistd.h>

using namespace gameplayer;

static void usage() {
	fprintf(stderr,
		"zacos9-gameplayer: play a console game file.\n"
		"  zacos9-gameplayer --play FILE\n"
		"  zacos9-gameplayer --check FILE\n"
		"\n"
		"FILE is a cartridge or disc image the Sniffer recognises (NES, Super NES,\n"
		"Sega, Game Boy, Game Boy Advance, PlayStation, PlayStation 2 or\n"
		"Nintendo 64). --check prints what would run without starting anything.\n");
}

static QString configDir() {
	return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) +
		"/zacos9/gameplayer";
}

static bool readFullscreen() {
	QSettings s(configDir() + "/gameplayer.conf", QSettings::IniFormat);
	return s.value("fullscreen", false).toBool();
}

static void writeFullscreen(bool v) {
	QSettings s(configDir() + "/gameplayer.conf", QSettings::IniFormat);
	s.setValue("fullscreen", v);
	s.sync();
}

static int checkGame(const QString &file) {
	const System sys = detectSystem(file);
	printf("system: %s\n", qPrintable(systemName(sys)));
	const QString lf = launchFile(sys, file);
	printf("game: %s\n", qPrintable(QFileInfo(lf).fileName()));
	QString error;
	const Launch launch = buildLaunch(sys, lf, configDir(), readFullscreen(), &error);
	if (launch.program.isEmpty()) {
		if (sys == System::None) {
			printf("error: that file isn't a game Game Player knows how to play.\n");
		} else {
			printf("error: %s\n", qPrintable(error));
		}
		return 2;
	}
	printf("engine: %s\n", qPrintable(launch.program));
	for (const QString &a : launch.args) {
		printf("arg: %s\n", qPrintable(a));
	}
	for (const QString &e : launch.env) {
		printf("env: %s\n", qPrintable(e));
	}
	const QString bios = biosInstructions(sys);
	printf("bios: %s\n", bios.isEmpty() ? "ok" : "needed");
	for (const QString &c : detectedControllers()) {
		printf("controller: %s\n", qPrintable(c));
	}
	return 0;
}

static int playGame(const QString &file) {
	const System sys = detectSystem(file);
	if (sys == System::None) {
		QMessageBox::critical(nullptr, "Game Player",
			QString("“%1” isn't a game Game Player knows how to play.").arg(QFileInfo(file).fileName()));
		return 2;
	}
	const QString bios = biosInstructions(sys);
	if (!bios.isEmpty()) {
		QMessageBox::information(nullptr, "Game Player", bios);
		return 0;
	}
	const QString lf = launchFile(sys, file);
	const QString game = QFileInfo(lf).completeBaseName();

	CollarExtensionService service("GamePlayer");
	QString startError;
	if (!service.start(&startError)) {
		fprintf(stderr, "zacos9-gameplayer: %s\n", qPrintable(startError));
	}

	const bool fullscreen = readFullscreen();
	QString error;
	const Launch launch = buildLaunch(sys, lf, configDir(), fullscreen, &error);
	if (launch.program.isEmpty()) {
		QMessageBox::critical(nullptr, "Game Player",
			error.isEmpty() ? "Game Player can't run this game." : error);
		return 2;
	}
	if (!ensureConfigs(configDir(), fullscreen)) {
		QMessageBox::critical(nullptr, "Game Player", "Game Player couldn't write its settings.");
		return 2;
	}

	QProcess engine;
	GameModule module(&service, game, sys, fullscreen, &engine);
	module.fullscreenChanged = [](bool v) { writeFullscreen(v); };
	service.changed();

	engine.setProgram(launch.program);
	engine.setArguments(launch.args);
	QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
	for (const QString &e : launch.env) {
		const int eq = e.indexOf('=');
		if (eq > 0) {
			env.insert(e.left(eq), e.mid(eq + 1));
		}
	}
	engine.setProcessEnvironment(env);
	engine.setProcessChannelMode(QProcess::ForwardedChannels);

	/* Log the launch for diagnosis. */
	{
		QFile logfile(configDir() + "/engine.log");
		if (logfile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
			QTextStream out(&logfile);
			out << "program: " << launch.program << "\n";
			out << "args: " << launch.args.join(" ") << "\n";
			out << "env:\n";
			for (const QString &k : env.keys()) {
				out << "  " << k << "=" << env.value(k) << "\n";
			}
			out << "---\n";
			logfile.close();
		}
	}

	QObject::connect(&engine, &QProcess::errorOccurred, [&](QProcess::ProcessError e) {
		if (e == QProcess::FailedToStart) {
			QMessageBox::critical(nullptr, "Game Player",
				QString("Game Player couldn't start %1.").arg(launch.program));
			QCoreApplication::exit(2);
		}
	});
	QObject::connect(&engine, &QProcess::finished, qApp, [&]() {
		if (module.restartRequested()) {
			writeFullscreen(module.fullscreen());
			QProcess::startDetached(QCoreApplication::applicationFilePath(),
				QStringList() << "--play" << file);
			QCoreApplication::quit();
		} else if (module.quitRequested()) {
			QCoreApplication::quit();
		} else {
			QCoreApplication::quit();
		}
	});

	engine.start();
	return qApp->exec();
}

static int configureController(int argc, char **argv) {
	QApplication::setApplicationName("zacos9-gameplayer");

	if (SDL_Init(SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER) != 0) {
		QMessageBox::critical(nullptr, "Game Player", "SDL couldn't start: " + QString::fromUtf8(SDL_GetError()));
		return 1;
	}
	if (SDL_NumJoysticks() == 0) {
		QMessageBox::information(nullptr, "Game Player", "No game controller is connected.");
		SDL_Quit();
		return 0;
	}
	if (SDL_NumJoysticks() == 0) {
		QMessageBox::information(nullptr, "Game Player", "No game controller is connected.");
		SDL_Quit();
		return 0;
	}
	const int index = 0;
	fprintf(stderr, "isGameController(0)=%d numJoysticks=%d\n", SDL_IsGameController(index), SDL_NumJoysticks());
	SDL_Joystick *joy = SDL_JoystickOpen(index);
	if (!joy) {
		QMessageBox::critical(nullptr, "Game Player", "Couldn't open the controller.");
		SDL_Quit();
		return 1;
	}
	char guidStr[33];
	SDL_JoystickGetGUIDString(SDL_JoystickGetGUID(joy), guidStr, sizeof(guidStr));
	const QString guid = QString::fromUtf8(guidStr);
	const QString name = QString::fromUtf8(SDL_JoystickName(joy));

	const QStringList labels = { "D-pad up", "D-pad down", "D-pad left", "D-pad right",
		"A", "B", "X", "Y", "Left shoulder", "Right shoulder", "Start", "Select" };
	const QStringList sdlNames = { "dpup", "dpdown", "dpleft", "dpright",
		"a", "b", "x", "y", "leftshoulder", "rightshoulder", "start", "back" };

	QSet<QString> used;
	QMap<QString, QString> mapping;
	for (int i = 0; i < labels.size(); ++i) {
		QMessageBox box(QMessageBox::Information, "Game Player",
			QString("Press %1 on \"%2\".").arg(labels[i], name));
		box.setWindowFlags(box.windowFlags() | Qt::WindowStaysOnTopHint);
		box.setStandardButtons(QMessageBox::Cancel);
		box.show();
		QCoreApplication::processEvents();
		bool got = false;
		Uint32 start = SDL_GetTicks();
		int fd = open("/dev/input/js0", O_RDONLY | O_NONBLOCK);
		// Drain any pending events first.
		if (fd >= 0) {
			struct js_event drain;
			while (read(fd, &drain, sizeof(drain)) == sizeof(drain)) {}
		}
		while (!got && SDL_GetTicks() - start < 10000) {
			QCoreApplication::processEvents();
			// Also pump SDL events and log them for debugging.
			SDL_Event se;
			while (SDL_PollEvent(&se)) {
				FILE *f = fopen("/tmp/cfg.log", "a");
				if (f) { fprintf(f, "SDL event type=%d\n", se.type); fclose(f); }
			}
			if (fd >= 0) {
				struct js_event e;
				while (read(fd, &e, sizeof(e)) == sizeof(e)) {
					if ((e.type & ~JS_EVENT_INIT) == JS_EVENT_BUTTON && e.value == 1) {
						const QString key = QString("b%1").arg(e.number);
						if (!used.contains(key)) {
							mapping.insert(sdlNames[i], key);
							used.insert(key);
							got = true;
						}
					}
				}
			}
			SDL_Delay(10);
		}
		if (fd >= 0) close(fd);
		box.close();
		if (!got) {
			QMessageBox::warning(nullptr, "Game Player",
				QString("Timed out waiting for %1; it won't be mapped.").arg(labels[i]));
		}
	}

	QStringList parts;
	for (auto it = mapping.begin(); it != mapping.end(); ++it) {
		parts << QString("%1:%2").arg(it.key(), it.value());
	}
	const QString mappingStr = guid + "," + name + "," + parts.join(",") + ",";

	QDir().mkpath(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + "/zacos9/gameplayer");
	QFile f(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + "/zacos9/gameplayer/controller_mappings.txt");
	if (f.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
		f.write((mappingStr + "\n").toUtf8());
		f.close();
	}

	SDL_JoystickClose(joy);
	SDL_Quit();

	QMessageBox::information(nullptr, "Game Player",
		"Controller configured. Its mapping was saved; it will be used the next time you play.");
	return 0;
}

int main(int argc, char **argv) {
	FILE *f = fopen("/tmp/cfg.log", "w");
	if (f) { fprintf(f, "main started argc=%d\n", argc); fclose(f); }
	QApplication app(argc, argv);
	app.setApplicationName("zacos9-gameplayer");

	if (argc < 2) {
		QMessageBox::information(nullptr, "Game Player",
			"Game Player plays console games. Put a game in your Games folder and\n"
			"double-click it, or drag a cartridge or disc onto this program.");
		return 0;
	}
	if (std::strcmp(argv[1], "--configure-controller") == 0) {
		return configureController(argc, argv);
	}
	if (std::strcmp(argv[1], "--check") == 0 && argc >= 3) {
		return checkGame(QString::fromLocal8Bit(argv[2]));
	}
	if (std::strcmp(argv[1], "--play") == 0 && argc >= 3) {
		return playGame(QString::fromLocal8Bit(argv[2]));
	}
	if (std::strcmp(argv[1], "--help") == 0 || std::strcmp(argv[1], "-h") == 0) {
		usage();
		return 0;
	}
	usage();
	return 2;
}