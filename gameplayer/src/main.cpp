/* zacos9-gameplayer: run a console game file (cartridge or disc) in ZacOS 9. */

#include <QApplication>
#include <QFile>
#include <QFileInfo>
#include <QMessageBox>
#include <QProcess>
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
	GameModule module(&service, game, sys, &engine);
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
	QObject::connect(&engine, &QProcess::finished, qApp, []() {
		QCoreApplication::quit();
	});

	engine.start();
	return qApp->exec();
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