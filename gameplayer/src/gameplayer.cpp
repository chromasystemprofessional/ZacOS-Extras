#include "gameplayer.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcessEnvironment>
#include <QStandardPaths>

#include <SDL2/SDL.h>

using namespace gameplayer;

namespace {

static QByteArray header(const QString &path, qint64 offset, int length) {
	QFile f(path);
	if (!f.open(QIODevice::ReadOnly) || !f.seek(offset)) {
		return QByteArray();
	}
	return f.read(length);
}

static bool siblingCue(const QFileInfo &info) {
	const QString cue = info.completeBaseName() + ".cue";
	const QStringList names = info.absoluteDir().entryList(QDir::Files, QDir::Name);
	for (const QString &n : names) {
		if (n.compare(cue, Qt::CaseInsensitive) == 0) {
			return true;
		}
	}
	return false;
}

static QString siblingCuePath(const QFileInfo &info) {
	const QString stem = info.completeBaseName();
	const QStringList names = info.absoluteDir().entryList(QDir::Files, QDir::Name);
	for (const QString &n : names) {
		if (n.compare(stem + ".cue", Qt::CaseInsensitive) == 0) {
			return info.absolutePath() + "/" + n;
		}
	}
	return QString();
}

static bool executableOnPath(const QString &program) {
	if (program.contains('/')) {
		return QFileInfo(program).isExecutable();
	}
	for (const QString &dir : qEnvironmentVariable("PATH").split(':', Qt::SkipEmptyParts)) {
		if (QFileInfo(dir + "/" + program).isExecutable()) {
			return true;
		}
	}
	return false;
}

static QStringList libretroDirs(const QString &override) {
	QStringList dirs;
	if (!override.isEmpty()) {
		for (const QString &d : override.split(':', Qt::SkipEmptyParts)) {
			if (QFileInfo(d).isDir()) {
				dirs << d;
			}
		}
	}
	for (const QString &lib : { QStringLiteral("/usr/lib"), QStringLiteral("/usr/local/lib") }) {
		QDir d(lib);
		for (const QString &e : d.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
			const QString p = lib + "/" + e + "/libretro";
			if (QFileInfo(p).isDir()) {
				dirs << p;
			}
		}
	}
	return dirs;
}

static QString findCore(const QString &name, const QString &override) {
	for (const QString &dir : libretroDirs(override)) {
		const QString p = dir + "/" + name;
		if (QFileInfo(p).isFile()) {
			return p;
		}
	}
	return name; /* let RetroArch find it by name */
}

static const char *coreName(System s) {
	switch (s) {
	case System::Nes: return "nestopia_libretro.so";
	case System::Snes: return "snes9x_libretro.so";
	case System::Sega: return "genesis_plus_gx_libretro.so";
	case System::GameBoy: return "gambatte_libretro.so";
	case System::Gba: return "mgba_libretro.so";
	default: return nullptr;
	}
}

static QString mupenCorelib() {
	const QByteArray v = qgetenv("ZACOS9_MUPEN_CORELIB");
	if (!v.isEmpty()) {
		return QString::fromLocal8Bit(v);
	}
	for (const QString &lib : { QStringLiteral("/usr/lib"), QStringLiteral("/usr/local/lib") }) {
		QDir d(lib);
		for (const QString &e : d.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
			const QStringList files = QDir(lib + "/" + e).entryList(
				QStringList() << "libmupen64plus.so*", QDir::Files);
			for (const QString &f : files) {
				if (f.startsWith("libmupen64plus.so")) {
					return lib + "/" + e + "/" + f;
				}
			}
		}
	}
	return "libmupen64plus.so.2";
}

static QString mupenDatadir() {
	const QByteArray v = qgetenv("ZACOS9_MUPEN_DATADIR");
	return v.isEmpty() ? QString("/usr/share/mupen64plus") : QString::fromLocal8Bit(v);
}

static QString logitechDualActionProfile() {
	return QStringLiteral(
		"input_device = \"Logitech Logitech Dual Action\"\n"
		"input_driver = \"udev\"\n"
		"input_vendor_id = \"1133\"\n"
		"input_product_id = \"49686\"\n"
		"input_b_btn = \"2\"\n"
		"input_a_btn = \"1\"\n"
		"input_y_btn = \"0\"\n"
		"input_x_btn = \"3\"\n"
		"input_start_btn = \"9\"\n"
		"input_select_btn = \"8\"\n"
		"input_l_btn = \"4\"\n"
		"input_r_btn = \"5\"\n"
		"input_l2_btn = \"6\"\n"
		"input_r2_btn = \"7\"\n"
		"input_l3_btn = \"10\"\n"
		"input_r3_btn = \"11\"\n"
		"input_l_x_plus_axis = \"+0\"\n"
		"input_l_x_minus_axis = \"-0\"\n"
		"input_l_y_plus_axis = \"+1\"\n"
		"input_l_y_minus_axis = \"-1\"\n"
		"input_r_x_plus_axis = \"+2\"\n"
		"input_r_x_minus_axis = \"-2\"\n"
		"input_r_y_plus_axis = \"+3\"\n"
		"input_r_y_minus_axis = \"-3\"\n"
		"input_up_btn = \"h0up\"\n"
		"input_down_btn = \"h0down\"\n"
		"input_left_btn = \"h0left\"\n"
		"input_right_btn = \"h0right\"\n");
}

static QString mupenPluginDir() {
	return "/usr/lib/x86_64-linux-gnu/mupen64plus";
}

} // namespace

System gameplayer::detectSystem(const QString &path) {
	const QFileInfo info(path);
	if (!info.isFile()) {
		return System::None;
	}
	const QString suffix = info.suffix().toLower();
	if (suffix == "nes") {
		return header(path, 0, 4) == QByteArray("NES\x1A", 4) ? System::Nes : System::None;
	}
	if (suffix == "sfc" || suffix == "smc") {
		return System::Snes;
	}
	if (suffix == "n64" || suffix == "z64" || suffix == "v64") {
		return System::N64;
	}
	if (suffix == "gen" || suffix == "smd" || suffix == "sms") {
		return System::Sega;
	}
	if (suffix == "md") {
		if (info.size() < 128 * 1024) {
			return System::None;
		}
		return header(path, 0x100, 4) == QByteArray("SEGA", 4) ? System::Sega : System::None;
	}
	if (suffix == "gb" || suffix == "gbc") {
		return System::GameBoy;
	}
	if (suffix == "gba") {
		return System::Gba;
	}
	if (suffix == "cue") {
		return System::Ps1;
	}
	if (suffix == "bin") {
		return siblingCue(info) ? System::Ps1 : System::None;
	}
	return System::None;
}

QString gameplayer::launchFile(System system, const QString &path) {
	if (system != System::Ps1) {
		return path;
	}
	const QFileInfo info(path);
	if (info.suffix().compare("cue", Qt::CaseInsensitive) == 0) {
		return path;
	}
	const QString cue = siblingCuePath(info);
	return cue.isEmpty() ? path : cue;
}

QString gameplayer::systemName(System system) {
	switch (system) {
	case System::Nes: return "NES";
	case System::Snes: return "Super NES";
	case System::Ps1: return "PlayStation";
	case System::N64: return "Nintendo 64";
	case System::Sega: return "Sega";
	case System::GameBoy: return "Game Boy";
	case System::Gba: return "Game Boy Advance";
	case System::None: return "unknown";
	}
	return "unknown";
}

Launch gameplayer::buildLaunch(System system, const QString &file, const QString &configDir,
		bool fullscreen, QString *error) {
	Launch l;
	const QString coreOverride = qgetenv("ZACOS9_GAMEPLAYER_CORES");
	switch (system) {
	case System::Nes:
	case System::Snes:
	case System::Sega:
	case System::GameBoy:
	case System::Gba: {
		const char *core = coreName(system);
		if (!core) {
			if (error) {
				*error = "Game Player doesn't have an engine for this system.";
			}
			return l;
		}
		l.program = "retroarch";
		l.args << "--config" << (configDir + "/retroarch.cfg")
			<< "-L" << findCore(QString::fromLatin1(core), coreOverride)
			<< file;
		l.env << "SDL_AUDIODRIVER=pulse";
		break;
	}
	case System::Ps1: {
		l.program = "mednafen";
		l.args << launchFile(System::Ps1, file);
		l.env << ("MEDNAFEN_HOME=" + configDir + "/mednafen");
		l.env << "SDL_VIDEODRIVER=x11";
		l.env << "SDL_AUDIODRIVER=pulse";
		break;
	}
	case System::N64: {
		l.program = "mupen64plus";
		l.args << "--corelib" << mupenCorelib()
			<< "--configdir" << (configDir + "/mupen64plus")
			<< "--datadir" << mupenDatadir();
		if (fullscreen) {
			l.args << "--fullscreen";
		} else {
			l.args << "--windowed";
		}
		l.args << "--gfx" << (mupenPluginDir() + "/mupen64plus-video-glide64mk2.so")
			<< "--audio" << (mupenPluginDir() + "/mupen64plus-audio-sdl.so")
			<< "--input" << (mupenPluginDir() + "/mupen64plus-input-sdl.so")
			<< "--rsp" << (mupenPluginDir() + "/mupen64plus-rsp-hle.so")
			<< file;
		l.env << "SDL_VIDEODRIVER=x11";
		l.env << "SDL_AUDIODRIVER=pulse";
		break;
	}
	case System::None:
		if (error) {
			*error = "That file isn't a game Game Player knows how to play.";
		}
		return l;
	}
	const QStringList mappings = controllerMappings();
	if (!mappings.isEmpty()) {
		l.env << ("SDL_GAMECONTROLLERCONFIG=" + mappings.join("\n"));
	}
	if (!executableOnPath(l.program)) {
		if (error) {
			*error = QString("Game Player needs %1, but it isn't installed.").arg(l.program);
		}
		l.program.clear();
		l.args.clear();
		l.env.clear();
		return l;
	}
	return l;
}

bool gameplayer::ensureConfigs(const QString &configDir, bool fullscreen) {
	QDir().mkpath(configDir);
	QDir().mkpath(configDir + "/mednafen/firmware");
	QDir().mkpath(configDir + "/mupen64plus");
	const QString saveDir = QDir::homePath() + "/Games/Saved Games";
	QDir().mkpath(saveDir);

	/* Autoconfig profile for the Logitech Dual Action gamepad, so RetroArch
	 * picks it up without manual button mapping. Written once. */
	const QString autoconfigDir = QDir::homePath() + "/.config/retroarch/autoconfig";
	QDir().mkpath(autoconfigDir);
	const QString profile = autoconfigDir + "/Logitech Logitech Dual Action.cfg";
	if (!QFileInfo(profile).exists()) {
		QFile f(profile);
		if (f.open(QIODevice::WriteOnly)) {
			f.write(logitechDualActionProfile().toUtf8());
			f.close();
		}
	}
	QFile rc(configDir + "/retroarch.cfg");
	if (!rc.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		return false;
	}
	rc.write(retroarchConfig(saveDir, fullscreen).toUtf8());
	rc.close();
	const QString med = configDir + "/mednafen/mednafen.cfg";
	QFile f(med);
	if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		f.write(mednafenConfig(fullscreen).toUtf8());
		f.close();
	}
	return true;
}

QString gameplayer::retroarchConfig(const QString &saveDir, bool fullscreen) {
	return QStringLiteral(
		"config_save_on_exit = \"false\"\n"
		"savestate_auto_save = \"true\"\n"
		"savestate_auto_load = \"true\"\n"
		"savestate_directory = \"%1\"\n"
		"savefile_directory = \"%1\"\n"
		"video_fullscreen = \"%2\"\n"
		"video_windowed_fullscreen = \"false\"\n"
		"aspect_ratio_index = \"0\"\n"
		"input_mouse_grab = \"false\"\n"
		"input_toggle_fullscreen = \"f11\"\n"
		"audio_driver = \"sdl\"\n"
		"joypad_autoconfig_dir = \"%3/.config/retroarch/autoconfig\"\n"
		"audio_enable = \"true\"\n"
		"audio_volume = \"1.0\"\n"
		"input_toggle_fullscreen = \"f11\"\n"
		"network_cmd_enable = \"true\"\n"
		"network_cmd_port = \"55355\"\n"
		"input_autodetect_enable = \"true\"\n").arg(saveDir).arg(fullscreen ? "true" : "false").arg(QDir::homePath());
}

QString gameplayer::mednafenConfig(bool fullscreen) {
	return QStringLiteral(
		"; Game Player: BIOS files (scph5501.bin and the like) belong in the\n"
		"; firmware folder beside this file.\n"
		"video.fs %1\n"
		"autosave 1\n"
		"load_state 1\n"
		"sound.driver sdl\n"
		"sound.volume 1\n").arg(fullscreen ? 1 : 0);
}


QStringList gameplayer::controllerMappings() {
	if (qEnvironmentVariableIsSet("ZACOS9_GAMEPLAYER_CONTROLLER_MAPPINGS")) {
		QStringList maps;
		for (const QByteArray &m : qgetenv("ZACOS9_GAMEPLAYER_CONTROLLER_MAPPINGS").split('\n')) {
			const QString s = QString::fromUtf8(m).trimmed();
			if (!s.isEmpty()) {
				maps << s;
			}
		}
		return maps;
	}
	QStringList maps;
	// Saved mappings from a previous --configure-controller run.
	const QString cfg = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) +
		"/zacos9/gameplayer/controller_mappings.txt";
	QFile saved(cfg);
	if (saved.open(QIODevice::ReadOnly | QIODevice::Text)) {
		while (!saved.atEnd()) {
			const QString line = QString::fromUtf8(saved.readLine()).trimmed();
			if (!line.isEmpty()) {
				maps << line;
			}
		}
		saved.close();
	}
	if (SDL_Init(SDL_INIT_GAMECONTROLLER) == 0) {
		const int n = SDL_NumJoysticks();
		for (int i = 0; i < n; ++i) {
			if (SDL_IsGameController(i)) {
				char *mapping = SDL_GameControllerMappingForDeviceIndex(i);
				if (mapping) {
					maps << QString::fromUtf8(mapping);
					SDL_free(mapping);
				}
			}
		}
		SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);
	}
	return maps;
}

QStringList gameplayer::detectedControllers() {
	if (qEnvironmentVariableIsSet("ZACOS9_GAMEPLAYER_CONTROLLERS")) {
		QStringList names;
		for (const QByteArray &n : qgetenv("ZACOS9_GAMEPLAYER_CONTROLLERS").split(',')) {
			const QString s = QString::fromUtf8(n).trimmed();
			if (!s.isEmpty()) {
				names << s;
			}
		}
		return names;
	}
	QStringList names;
	if (SDL_Init(SDL_INIT_JOYSTICK) == 0) {
		const int n = SDL_NumJoysticks();
		for (int i = 0; i < n; ++i) {
			const char *name = SDL_JoystickNameForIndex(i);
			names << (name ? QString::fromUtf8(name) : QString("Controller %1").arg(i + 1));
		}
		SDL_QuitSubSystem(SDL_INIT_JOYSTICK);
	}
	return names;
}
