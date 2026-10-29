/* zacos9-gameplayer: run a console game file (cartridge or disc) in ZacOS 9. */

#include <QApplication>
#include <QMessageBox>
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QTextStream>
#include <QProcessEnvironment>
#include <QSettings>
#include <QStandardPaths>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "collarextension.h"
#include "gameplayer.h"
#include "gamemodule.h"

using namespace gameplayer;

static void usage() {
	fprintf(stderr,
		"zacos9-gameplayer: play a console game file.\n"
		"  zacos9-gameplayer --play FILE\n"
		"  zacos9-gameplayer --check FILE\n"
		"\n"
		"FILE is a cartridge or disc image the Sniffer recognises (NES, Super NES,\n"
		"Sega, Game Boy, Game Boy Advance, PlayStation or Nintendo 64). --check\n"
		"prints what would run without starting anything.\n");
}

static QString configDir() {
	return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) +
		"/zacos9/gameplayer";
}

static bool readFullscreen() {
	QSettings s(configDir() + "/gameplayer.conf", QSettings::IniFormat);
	return s.value("fullscreen", false).toBool();
}

static void writeFullscreen(bool value) {
	QSettings s(configDir() + "/gameplayer.conf", QSettings::IniFormat);
	s.setValue("fullscreen", value);
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
	return 0;
}

static int playGame(const QString &file) {
	/* Write a startup marker so we can tell whether the program ran at all. */
	{
		QDir().mkpath(configDir());
		QFile marker(configDir() + "/startup.log");
		if (marker.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
			QTextStream out(&marker);
			out << "file: " << file << "\n";
			out << "exists: " << QFile::exists(file) << "\n";
			out << "system: " << (int)detectSystem(file) << " (" << systemName(detectSystem(file)) << ")\n";
			marker.close();
		}
	}
	const System sys = detectSystem(file);
	if (sys == System::None) {
		QMessageBox::critical(nullptr, "Game Player",
			QString("“%1” isn't a game Game Player knows how to play.").arg(QFileInfo(file).fileName()));
		return 2;
	}
	const QString lf = launchFile(sys, file);
	const QString game = QFileInfo(lf).completeBaseName();

	CollarExtensionService service("GamePlayer");
	QString startError;
	if (!service.start(&startError)) {
		fprintf(stderr, "zacos9-gameplayer: %s\n", qPrintable(startError));
	}

	bool running = true;
	while (running) {
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
		module.clearRestartRequested();
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
			QFile log(configDir() + "/engine.log");
			if (log.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
				QTextStream out(&log);
				out << "program: " << launch.program << "\n";
				out << "args: " << launch.args.join(" ") << "\n";
				out << "env:\n";
				for (const QString &k : env.keys()) {
					out << "  " << k << "=" << env.value(k) << "\n";
				}
				out << "---\n";
				log.close();
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
			if (module.quitRequested()) {
				QCoreApplication::quit();
			} else if (module.restartRequested()) {
				writeFullscreen(module.fullscreen());
				/* Restart: the loop relaunches the engine. */
				QCoreApplication::exit(0);  // exit code 0 signals "restart"
			} else {
				QCoreApplication::quit();
			}
		});

		engine.start();
		const int code = qApp->exec();
		if (code == 2) {
			return 2;  // fatal error
		}
		if (module.restartRequested()) {
			writeFullscreen(module.fullscreen());
			continue;  // relaunch
		}
		running = false;
	}
	return 0;
}

int main(int argc, char **argv) {
	QApplication app(argc, argv);
	app.setApplicationName("zacos9-gameplayer");

	if (argc < 2) {
		QMessageBox::information(nullptr, "Game Player",
			"Game Player plays console games. Put a game in your Games folder and\n"
			"double-click it, or drag a cartridge or disc onto this program.");
		return 0;
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