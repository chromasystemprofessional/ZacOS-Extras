/* zacos9-gameplayer: run a console game file (cartridge or disc) in ZacOS 9. */

#include <QApplication>
#include <QCryptographicHash>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QMessageBox>
#include <QProcess>
#include <QProcessEnvironment>
#include <QProgressDialog>
#include <QSet>
#include <QMap>
#include <QDir>
#include <QFile>
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>

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
		"  zacos9-gameplayer --play-disc DEVICE MOUNT\n"
		"  zacos9-gameplayer --check FILE\n"
		"  zacos9-gameplayer --check-disc DEVICE MOUNT\n"
		"\n"
		"FILE is a cartridge or disc image, or a PlayStation 3 game folder,\n"
		"that the Sniffer recognises (NES, Super NES, Sega, Game Boy, Game Boy\n"
		"Advance, PlayStation, PlayStation 2, PlayStation 3 or Nintendo 64).\n"
		"--check prints what would run without starting anything. --play-disc\n"
		"reads a mounted PlayStation 1 or 2 disc and plays it from the drive.\n"
		"\n"
		"Games go in ~/Games. A PlayStation BIOS (scph5500.bin, scph5501.bin or\n"
		"scph5502.bin) or a 4 MB PlayStation 2 BIOS .bin dropped there is installed\n"
		"where mednafen or PCSX2 needs it.\n");
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

static int readVolume() {
	QSettings s(configDir() + "/gameplayer.conf", QSettings::IniFormat);
	return qBound(0, s.value("volume", 100).toInt(), 150);
}

static void writeVolume(int v) {
	QSettings s(configDir() + "/gameplayer.conf", QSettings::IniFormat);
	s.setValue("volume", qBound(0, v, 150));
	s.sync();
}

static int playGame(const QString &file);

/* A cache folder for a disc's image. The mount's timestamp changes for a
 * different disc, so a swapped disc doesn't reuse the old rip. */
static QString discCacheDir(const QString &device, const QString &mount) {
	const QString base = QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation) +
		"/zacos9/gameplayer/discs";
	const QString stamp = QFileInfo(mount).lastModified().toString(Qt::ISODate);
	const QByteArray key = QCryptographicHash::hash(
		(device + "|" + mount + "|" + stamp).toUtf8(), QCryptographicHash::Sha256).toHex().left(20);
	return base + "/" + QString::fromLatin1(key);
}

static QString discImageName(const QString &mount, DiscSystem system) {
	QString name = QFileInfo(mount).fileName();
	for (QChar &c : name) {
		if (!c.isLetterOrNumber() && c != ' ' && c != '-' && c != '_' &&
				c != '(' && c != ')' && c != '[' && c != ']' && c != '.') {
			c = '-';
		}
	}
	name = name.trimmed();
	if (name.isEmpty()) {
		name = "disc";
	}
	return name + (system == DiscSystem::Ps1 ? ".cue" : ".iso");
}

/* The image and its marker. The marker is written only after the rip
 * completes, so an interrupted rip is never mistaken for a finished one. */
static bool discImageReady(const QString &cacheDir, const QString &image) {
	if (!QFileInfo(cacheDir + "/disc.ready").isFile() ||
			!QFileInfo(image).isFile() || QFileInfo(image).size() <= 0) {
		return false;
	}
	if (image.endsWith(QLatin1String(".cue"))) {
		for (const QFileInfo &track : QDir(cacheDir).entryInfoList(
				QStringList() << "*.bin", QDir::Files)) {
			if (track.size() > 0) {
				return true;
			}
		}
		return false;
	}
	return true;
}

/* Reads a physical disc into a cache image and reports it through `done`.
 * PlayStation 1 needs a CUE/BIN (mednafen cannot read a drive directly);
 * PlayStation 2 is read to an ISO. */
class DiscImageReader : public QObject {
public:
	using Done = std::function<void(const QString &image, const QString &error)>;

	DiscImageReader(QWidget *parent, DiscSystem system, const QString &device,
			const QString &mount, const QString &cacheDir, Done done)
		: QObject(parent), m_system(system), m_device(device), m_cacheDir(cacheDir),
		  m_done(std::move(done)) {
		m_progress = new QProgressDialog(
			QString("Reading “%1” from the disc…").arg(QFileInfo(mount).fileName()),
			"Stop", 0, 0, parent);
		m_progress->setWindowTitle("Game Player");
		m_progress->setWindowModality(Qt::ApplicationModal);
		m_progress->setMinimumDuration(0);
		m_progress->setAutoClose(false);
		m_progress->setAutoReset(false);
		connect(m_progress, &QProgressDialog::canceled, this, [this] { cancel(); });
		m_process = new QProcess(this);
		m_process->setWorkingDirectory(cacheDir);
		m_process->setProcessChannelMode(QProcess::MergedChannels);
		connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
			if (error == QProcess::FailedToStart && !m_cancelled) {
				fail(m_program + " could not be started.");
			}
		});
		connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
			[this](int code, QProcess::ExitStatus status) { finished(code, status); });
	}

	void start() {
		QDir().mkpath(m_cacheDir);
		m_progress->show();
		if (m_system == DiscSystem::Ps1) {
			m_step = Step::Cdrdao;
			run("cdrdao", { "read-cd", "--read-raw", "--device", m_device,
				"--datafile", "disc.bin", "disc.toc" });
		} else {
			m_step = Step::Blockdev;
			run("blockdev", { "--getsize64", m_device });
		}
	}

private:
	enum class Step { Cdrdao, Toc2cue, Blockdev, Dd };
	DiscSystem m_system;
	QString m_device;
	QString m_cacheDir;
	QString m_program;
	QStringList m_args;
	Done m_done;
	QProgressDialog *m_progress = nullptr;
	QProcess *m_process = nullptr;
	Step m_step = Step::Cdrdao;
	bool m_cancelled = false;

	void run(const QString &program, const QStringList &args) {
		m_program = program;
		m_args = args;
		m_process->start(program, args);
	}

	void finish(const QString &image) {
		QFile marker(m_cacheDir + "/disc.ready");
		if (marker.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
			marker.write(m_system == DiscSystem::Ps1 ? "ps1\n" : "ps2\n");
			marker.close();
		}
		m_progress->close();
		m_progress->deleteLater();
		m_process->deleteLater();
		auto done = m_done;
		const QString path = image;
		QTimer::singleShot(0, [done, path] { done(path, QString()); });
		deleteLater();
	}

	void fail(const QString &error) {
		if (m_cancelled) {
			return;
		}
		m_cancelled = true;
		if (m_process->state() != QProcess::NotRunning) {
			m_process->kill();
		}
		m_progress->close();
		m_progress->deleteLater();
		m_process->deleteLater();
		auto done = m_done;
		QTimer::singleShot(0, [done, error] { done(QString(), error); });
		deleteLater();
	}

	void cancel() {
		if (m_cancelled) {
			return;
		}
		m_cancelled = true;
		if (m_process->state() != QProcess::NotRunning) {
			m_process->kill();
		}
		m_progress->close();
		m_progress->deleteLater();
		m_process->deleteLater();
		auto done = m_done;
		QTimer::singleShot(0, [done] { done(QString(), "Reading the disc was stopped."); });
		deleteLater();
	}

	void finished(int code, QProcess::ExitStatus status) {
		if (m_cancelled) {
			return;
		}
		const QString output = QString::fromLocal8Bit(m_process->readAll()).trimmed();
		if (code != 0 || status != QProcess::NormalExit) {
			QString detail = output;
			if (detail.size() > 600) {
				detail = detail.right(600);
			}
			fail(detail.isEmpty() ? m_program + " failed." : detail);
			return;
		}
		if (m_step == Step::Cdrdao) {
			m_step = Step::Toc2cue;
			run("toc2cue", { "disc.toc", "disc.cue" });
			return;
		}
		if (m_step == Step::Toc2cue) {
			finish(m_cacheDir + "/disc.cue");
			return;
		}
		if (m_step == Step::Blockdev) {
			bool ok = false;
			const qint64 size = output.toLongLong(&ok);
			if (!ok || size <= 0) {
				fail("The disc's size could not be read.");
				return;
			}
			m_step = Step::Dd;
			run("dd", { "if=" + m_device, "of=disc.iso", "bs=2048",
				"count=" + QString::number(size / 2048), "status=none" });
			return;
		}
		if (m_step == Step::Dd) {
			finish(m_cacheDir + "/disc.iso");
		}
	}
};

static int playDisc(const QString &device, const QString &mount) {
	if (!device.startsWith(QLatin1String("/dev/"))) {
		QMessageBox::critical(nullptr, "Game Player", "The disc's drive path is invalid.");
		return 2;
	}
	QString error;
	const DiscSystem disc = detectDiscSystem(mount, &error);
	if (disc == DiscSystem::None || disc == DiscSystem::Ps3) {
		QMessageBox::information(nullptr, "Game Player",
			error.isEmpty() ? "That disc isn't a PlayStation game." : error);
		return 2;
	}
	const QString cacheDir = discCacheDir(device, mount);
	const QString image = cacheDir + "/" + discImageName(mount, disc);
	if (!discImageReady(cacheDir, image)) {
		QEventLoop loop;
		QString readError;
		QString readImage;
		auto *reader = new DiscImageReader(nullptr, disc, device, mount, cacheDir,
			[&](const QString &path, const QString &problem) {
				readImage = path;
				readError = problem;
				loop.quit();
			});
		reader->start();
		loop.exec();
		if (!readError.isEmpty()) {
			QMessageBox::critical(nullptr, "Game Player", readError);
			return 2;
		}
		if (readImage.isEmpty()) {
			QMessageBox::critical(nullptr, "Game Player", "Game Player couldn't read this disc.");
			return 2;
		}
		return playGame(readImage);
	}
	return playGame(image);
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
			printf("error: that isn't a game Game Player knows how to play.\n");
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
	const QString mednafenInput = mednafenInputConfig();
	for (const QString &line : mednafenInput.split('\n', Qt::SkipEmptyParts)) {
		printf("mednafen: %s\n", qPrintable(line));
	}
	for (const QString &c : detectedControllers()) {
		printf("controller: %s\n", qPrintable(c));
	}
	return 0;
}

static int checkDisc(const QString &device, const QString &mount) {
	printf("device: %s\n", qPrintable(device));
	printf("mount: %s\n", qPrintable(mount));
	QString error;
	const DiscSystem disc = detectDiscSystem(mount, &error);
	if (disc == DiscSystem::None) {
		if (error.isEmpty()) {
			error = "That disc isn't a PlayStation game.";
		}
		printf("error: %s\n", qPrintable(error));
		return 2;
	}
	printf("system: %s\n", disc == DiscSystem::Ps1 ? "PlayStation" :
		disc == DiscSystem::Ps2 ? "PlayStation 2" : "PlayStation 3");
	if (disc == DiscSystem::Ps3) {
		if (error.isEmpty()) {
			error = "PlayStation 3 discs can't be read directly.";
		}
		printf("error: %s\n", qPrintable(error));
		return 2;
	}
	printf("engine: %s\n", disc == DiscSystem::Ps1 ? "mednafen" : "PCSX2");
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
		QMessageBox::critical(nullptr, "Game Player",
			"Game Player couldn't prepare its folders and settings.");
		return 2;
	}
	for (const QString &firmware : installDroppedFirmware(configDir())) {
		fprintf(stderr, "zacos9-gameplayer: installed %s\n", qPrintable(firmware));
	}
	const QString bios = biosInstructions(sys);
	if (!bios.isEmpty()) {
		QMessageBox::information(nullptr, "Game Player", bios);
		return 0;
	}

	QProcess engine;
	GameModule module(&service, game, sys, fullscreen, readVolume(), &engine);
	module.fullscreenChanged = [](bool v) { writeFullscreen(v); };
	module.volumeChanged = [](int v) { writeVolume(v); };
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
		QString error;
		if (!ensureGamesFolder(&error)) {
			QMessageBox::critical(nullptr, "Game Player", error);
			return 2;
		}
		installDroppedFirmware(configDir());
		QMessageBox::information(nullptr, "Game Player",
			"Game Player plays console games. Put a game in ~/Games and\n"
			"double-click it, or drag a cartridge or disc onto this program.\n\n"
			"PlayStation and PlayStation 2 games need their BIOS. Drop its\n"
			".bin file into ~/Games; Game Player installs it for you.");
		return 0;
	}
	if (std::strcmp(argv[1], "--configure-controller") == 0) {
		return configureController(argc, argv);
	}
	if (std::strcmp(argv[1], "--check") == 0 && argc >= 3) {
		QString error;
		if (!ensureGamesFolder(&error)) {
			fprintf(stderr, "zacos9-gameplayer: %s\n", qPrintable(error));
		}
		return checkGame(QString::fromLocal8Bit(argv[2]));
	}
	if (std::strcmp(argv[1], "--check-disc") == 0 && argc >= 4) {
		return checkDisc(QString::fromLocal8Bit(argv[2]), QString::fromLocal8Bit(argv[3]));
	}
	if (std::strcmp(argv[1], "--play-disc") == 0 && argc >= 4) {
		return playDisc(QString::fromLocal8Bit(argv[2]), QString::fromLocal8Bit(argv[3]));
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