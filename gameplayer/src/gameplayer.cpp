#include "gameplayer.h"

#include <QBuffer>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QImage>
#include <QProcessEnvironment>
#include <QSaveFile>
#include <QStandardPaths>

#include <cstdio>
#include <fcntl.h>
#include <linux/input.h>
#include <linux/joystick.h>
#include <sys/ioctl.h>
#include <sys/xattr.h>
#include <unistd.h>

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

/* Is this a PlayStation 3 game folder? A disc dump has PS3_GAME/ with
 * PARAM.SFO (or USRDIR/EBOOT.BIN); an installed PSN game has PARAM.SFO and
 * USRDIR/EBOOT.BIN at the folder's top. */
static bool ps3GameFolder(const QString &path) {
	const QFileInfo info(path);
	if (!info.isDir()) {
		return false;
	}
	if (QFileInfo(path + "/PS3_GAME").isDir()) {
		return QFileInfo(path + "/PS3_GAME/PARAM.SFO").isFile() ||
			QFileInfo(path + "/PS3_GAME/USRDIR/EBOOT.BIN").isFile();
	}
	return QFileInfo(path + "/PARAM.SFO").isFile() &&
		QFileInfo(path + "/USRDIR/EBOOT.BIN").isFile();
}

/* RPCS3's own config, where its dev_flash firmware lives. A Flatpak has a
 * private one under ~/.var. */
static QString rpcs3ConfigDir() {
	return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + "/rpcs3";
}

static bool rpcs3FirmwarePresent() {
	const QStringList dirs = {
		rpcs3ConfigDir() + "/dev_flash/sys/external",
		QDir::homePath() + "/.var/app/net.rpcs3.RPCS3/config/rpcs3/dev_flash/sys/external",
	};
	for (const QString &dir : dirs) {
		const QDir firmware(dir);
		if (firmware.exists() &&
				!firmware.entryList(QStringList() << "*.sprx", QDir::Files).isEmpty()) {
			return true;
		}
	}
	return false;
}

/* The command that starts RPCS3 on GAME. It can be rpcs3 on PATH, an
 * AppImage the user downloaded, or the Flatpak. Empty when none is found. */
static QStringList rpcs3Command(const QString &game, QString *error) {
	const QString override = qEnvironmentVariable("ZACOS9_RPCS3");
	if (!override.isEmpty()) {
		const QFileInfo file(override);
		if (file.isFile() && file.isExecutable()) {
			return { override, "--no-gui", game };
		}
		if (error) {
			*error = "ZACOS9_RPCS3 does not name an executable.";
		}
		return {};
	}
	if (executableOnPath("rpcs3")) {
		return { "rpcs3", "--no-gui", game };
	}
	const QStringList dirs = {
		QDir::homePath() + "/Applications",
		QDir::homePath() + "/Downloads",
		QDir::homePath() + "/.local/bin",
		QStringLiteral("/opt/rpcs3"),
		QStringLiteral("/usr/local/bin"),
		QStringLiteral("/usr/bin"),
	};
	for (const QString &dir : dirs) {
		const QStringList names = QDir(dir).entryList(QDir::Files);
		for (const QString &name : names) {
			if (!name.startsWith("rpcs3", Qt::CaseInsensitive) ||
					!name.endsWith(".AppImage", Qt::CaseInsensitive)) {
				continue;
			}
			const QString path = dir + "/" + name;
			const QFileInfo file(path);
			if (file.isFile() && file.isExecutable()) {
				return { path, "--no-gui", game };
			}
		}
	}
	if (executableOnPath("flatpak") &&
			(QFileInfo(QDir::homePath() + "/.local/share/flatpak/app/net.rpcs3.RPCS3").isDir() ||
			 QFileInfo("/var/lib/flatpak/app/net.rpcs3.RPCS3").isDir())) {
		return { "flatpak", "run", "net.rpcs3.RPCS3", "--no-gui", game };
	}
	if (error) {
		*error = "Game Player needs RPCS3 to play PlayStation 3 games. "
			"Get it from https://rpcs3.net or install the Flatpak.";
	}
	return {};
}

/* One physical controller as mednafen sees it: the ID it prints at startup
 * (bus, vendor, product, version, axes and buttons), and where a hat's two
 * axes sit among the joystick API's axes. */
struct MednafenPad {
	QString name;
	QString id;
	int hatX = -1;
	int hatY = -1;
};

static bool mednafenPad(const QString &jsPath, MednafenPad *pad) {
	const int fd = open(QFile::encodeName(jsPath).constData(), O_RDONLY | O_NONBLOCK);
	if (fd < 0) {
		return false;
	}
	unsigned char axes = 0, buttons = 0;
	char name[256] = {};
	const bool ok = ioctl(fd, JSIOCGAXES, &axes) == 0 &&
		ioctl(fd, JSIOCGBUTTONS, &buttons) == 0 &&
		ioctl(fd, JSIOCGNAME(sizeof(name)), name) >= 0;
	close(fd);
	if (!ok) {
		return false;
	}
	const QString base = "/sys/class/input/" + QFileInfo(jsPath).fileName() + "/device";
	const auto hex = [&](const char *file) {
		QFile f(base + "/id/" + file);
		if (!f.open(QIODevice::ReadOnly)) {
			return -1;
		}
		bool parsed = false;
		const int value = f.readAll().trimmed().toInt(&parsed, 16);
		return parsed ? value : -1;
	};
	const int bus = hex("bustype");
	const int vendor = hex("vendor");
	const int product = hex("product");
	const int version = hex("version");
	if (bus < 0 || vendor < 0 || product < 0 || version < 0) {
		return false;
	}
	pad->name = QString::fromUtf8(name).trimmed();
	pad->id = QString::asprintf("0x%04x%04x%04x%04x%04x%04x%08x",
		static_cast<unsigned>(bus), static_cast<unsigned>(vendor),
		static_cast<unsigned>(product), static_cast<unsigned>(version),
		static_cast<unsigned>(axes), static_cast<unsigned>(buttons), 0u);
	/* mednafen numbers hats among the axes; find their place the way it
	 * does, from the event device's ABS capabilities. */
	QFile caps(base + "/capabilities/abs");
	if (caps.open(QIODevice::ReadOnly)) {
		bool parsed = false;
		const quint64 bits = caps.readAll().trimmed().toULongLong(&parsed, 16);
		if (parsed) {
			int count = 0;
			for (int code = 0; code < ABS_CNT; code++) {
				if (!(bits & (quint64(1) << code))) {
					continue;
				}
				if (code == ABS_HAT0X) {
					pad->hatX = count;
				} else if (code == ABS_HAT0Y) {
					pad->hatY = count;
				}
				count++;
			}
		}
	}
	if (pad->hatX < 0 && axes >= 2) {
		pad->hatX = axes - 2;
	}
	if (pad->hatY < 0 && axes >= 1) {
		pad->hatY = axes - 1;
	}
	return true;
}

static QList<MednafenPad> mednafenPads() {
	QList<MednafenPad> pads;
	const QDir input("/dev/input");
	const QStringList names = input.entryList(QStringList() << "js*",
		QDir::Files | QDir::System, QDir::Name);
	for (const QString &name : names) {
		MednafenPad pad;
		if (mednafenPad(input.filePath(name), &pad)) {
			pads << pad;
		}
	}
	return pads;
}

/* The SDL mapping for a controller, matched by name. When none matches
 * (a saved mapping for an older device, say), the last one is the one
 * SDL detected for the connected controller. */
static QString mednafenMappingFor(const QString &padName) {
	const QStringList mappings = controllerMappings();
	QString fallback;
	for (const QString &mapping : mappings) {
		const QStringList parts = mapping.split(',');
		if (parts.size() < 3) {
			continue;
		}
		fallback = mapping;
		if (parts[1].trimmed().compare(padName, Qt::CaseInsensitive) == 0) {
			return mapping;
		}
	}
	return fallback;
}

static QHash<QString, QString> sdlMapping(const QString &mapping) {
	QHash<QString, QString> values;
	const QStringList parts = mapping.split(',');
	for (int i = 2; i < parts.size(); i++) {
		const int colon = parts[i].indexOf(':');
		if (colon > 0) {
			values.insert(parts[i].left(colon), parts[i].mid(colon + 1));
		}
	}
	return values;
}

/* One SDL mapping value as mednafen's input syntax: buttons, axes and
 * hats (which mednafen exposes as the axes after the sticks). */
static QString mednafenInput(const QString &sdl, const MednafenPad &pad) {
	QString value = sdl;
	if (value.isEmpty()) {
		return QString();
	}
	if (value.startsWith('b')) {
		bool ok = false;
		const int button = value.mid(1).toInt(&ok);
		return ok ? QString("button_%1").arg(button) : QString();
	}
	if (value.startsWith('h')) {
		const int dot = value.indexOf('.');
		if (dot < 0) {
			return QString();
		}
		bool ok = false;
		const int hat = value.mid(1, dot - 1).toInt(&ok);
		const int direction = value.mid(dot + 1).toInt(&ok);
		if (!ok || hat != 0) {
			return QString();
		}
		switch (direction) {
		case 1: return pad.hatY >= 0 ? QString("abs_%1-").arg(pad.hatY) : QString();
		case 4: return pad.hatY >= 0 ? QString("abs_%1+").arg(pad.hatY) : QString();
		case 8: return pad.hatX >= 0 ? QString("abs_%1-").arg(pad.hatX) : QString();
		case 2: return pad.hatX >= 0 ? QString("abs_%1+").arg(pad.hatX) : QString();
		default: return QString();
		}
	}
	bool inverted = false;
	if (value.startsWith('~') || value.startsWith('-')) {
		inverted = true;
		value = value.mid(1);
	} else if (value.startsWith('+')) {
		value = value.mid(1);
	}
	if (value.startsWith('a')) {
		bool ok = false;
		const int axis = value.mid(1).toInt(&ok);
		if (!ok) {
			return QString();
		}
		return QString("abs_%1%2").arg(axis).arg(inverted ? "-" : "+");
	}
	return QString();
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

/* Debian's PCSX2 binary is /usr/games/PCSX2; some builds and wrappers ship
 * it as pcsx2. Prefer a real path, then PATH. */
static QString pcsx2Program() {
	for (const char *path : { "/usr/games/PCSX2", "/usr/bin/PCSX2",
			"/usr/games/pcsx2", "/usr/bin/pcsx2" }) {
		if (executableOnPath(path)) {
			return QString::fromLatin1(path);
		}
	}
	if (executableOnPath("PCSX2")) {
		return "PCSX2";
	}
	if (executableOnPath("pcsx2")) {
		return "pcsx2";
	}
	return "PCSX2";
}

/* The PS1 firmware folder mednafen reads (written by ensureConfigs). */
static QString mednafenFirmwareDir() {
	return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) +
		"/zacos9/gameplayer/mednafen/firmware";
}

static bool mednafenBiosPresent() {
	for (const char *name : { "scph5501.bin", "scph5502.bin", "scph5503.bin",
			"scph1001.bin", "scph7001.bin" }) {
		if (QFileInfo(mednafenFirmwareDir() + "/" + name).isFile()) {
			return true;
		}
	}
	return false;
}

/* PCSX2's BIOS folders (1.6 config dir, and 2.x data dir). */
static QStringList pcsx2BiosDirs() {
	const QString home = QDir::homePath();
	return { home + "/.config/PCSX2/bios", home + "/.local/share/PCSX2/bios" };
}

static bool pcsx2BiosPresent() {
	for (const QString &dir : pcsx2BiosDirs()) {
		for (const QFileInfo &f : QDir(dir).entryInfoList(QDir::Files)) {
			/* A PS2 BIOS is a multi-megabyte bin/rom; the smaller files in
			 * the folder are save-state and config helpers. */
			if (f.size() >= 1024 * 1024) {
				return true;
			}
		}
	}
	return false;
}

/* The folder icon the Finder puts on ~/Games: the Platinum folder with a
 * cartridge on its face, 32 x 32 with on/off transparency like the desktop
 * icons. */
static QImage gamesFolderIcon() {
	static const char *const art[32] = {
		"................................",
		"................................",
		"................................",
		"................................",
		"................................",
		".KKKKKKKKKKKK...................",
		".KWWWWWWWWWWK...................",
		".KWLLLLLLLLLK...................",
		".KLLLLLLLLLLKKKKKKKKKKKKKKKKKKK.",
		".KWWWWWWWWWWWWWWWWWWWWWWWWWWWWK.",
		".KWLLLLLLLLLLLLLLLLLLLLLLLLLLMK.",
		".KNNNNNNNNKKKKKKKKKKKKNNNNNNNMK.",
		".KWWWWWWWWKPPPPPPPPPPKWWWWWWWWK.",
		".KWLLLLLLLLKPGGGGGGGGGPKLLLLLMK.",
		".KWLLLLLLLLKPGRRRRRRRGPKLLLLLMK.",
		".KWLLLLLLLLKPGGGGGGGGGPKLLLLLMK.",
		".KWLLLLLLLLKPGRRRRRRRGPKLLLLLMK.",
		".KWLLLLLLLLKPGGGGGGGGGPKLLLLLMK.",
		".KWLLLLLLLLKPPPPPPPPPPKLLLLLLMK.",
		".KWLLLLLLLLKKPKPKPKPKPKKLLLLLMK.",
		".KWLLLLLLLLLKKKKKKKKKKKLLLLLLMK.",
		".KWLLLLLLLLLLLLLLLLLLLLLLLLLLMK.",
		".KWLLLLLLLLLLLLLLLLLLLLLLLLLLMK.",
		".KWLLLLLLLLLLLLLLLLLLLLLLLLLLMK.",
		".KWLLLLLLLLLLLLLLLLLLLLLLLLLLMK.",
		".KWLLLLLLLLLLLLLLLLLLLLLLLLLLMK.",
		".KMMMMMMMMMMMMMMMMMMMMMMMMMMMMK.",
		".KKKKKKKKKKKKKKKKKKKKKKKKKKKKKK.",
		"................................",
		"................................",
		"................................",
		"................................",
	};
	QImage image(32, 32, QImage::Format_ARGB32);
	image.fill(Qt::transparent);
	for (int y = 0; y < 32; ++y) {
		for (int x = 0; x < 32; ++x) {
			switch (art[y][x]) {
			case 'K': image.setPixel(x, y, qRgb(0x00, 0x00, 0x00)); break;
			case 'W': image.setPixel(x, y, qRgb(0xFF, 0xFF, 0xFF)); break;
			case 'L': image.setPixel(x, y, qRgb(0xCC, 0xCC, 0xFF)); break;
			case 'M': image.setPixel(x, y, qRgb(0x99, 0x99, 0xFF)); break;
			case 'N': image.setPixel(x, y, qRgb(0x66, 0x66, 0xCC)); break;
			case 'P': image.setPixel(x, y, qRgb(0x33, 0x33, 0x99)); break;
			case 'G': image.setPixel(x, y, qRgb(0xDD, 0xDD, 0xDD)); break;
			case 'R': image.setPixel(x, y, qRgb(0x66, 0x66, 0xCC)); break;
			}
		}
	}
	return image;
}

/* The Finder's custom-icon storage: a user.zacos9.custom-icon extended
 * attribute on the item, or a hashed PNG under the user's data directory
 * where the filesystem cannot hold one. The key and hash mirror
 * customIconKey() in the Sniffer's shell/files/items.cpp. */
static constexpr const char *CUSTOM_ICON_ATTR = "user.zacos9.custom-icon";

static QString customIconKey(const QString &path) {
	const QFileInfo info(path);
	const QString canonical = info.canonicalFilePath();
	return QStringLiteral("file:") + (canonical.isEmpty() ? info.absoluteFilePath() : canonical);
}

static QString customIconStorePath(const QString &path) {
	const QByteArray hash = QCryptographicHash::hash(customIconKey(path).toUtf8(),
		QCryptographicHash::Sha256).toHex();
	return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) +
		"/zacos9/custom-icons/" + QString::fromLatin1(hash) + ".png";
}

static bool hasCustomIcon(const QString &path) {
	if (lgetxattr(QFile::encodeName(path).constData(), CUSTOM_ICON_ATTR,
			nullptr, 0) >= 0) {
		return true;
	}
	return QFileInfo::exists(customIconStorePath(path));
}

static bool writeCustomIcon(const QString &path, const QImage &image) {
	QByteArray encoded;
	QBuffer buffer(&encoded);
	if (!buffer.open(QIODevice::WriteOnly) || !image.save(&buffer, "PNG") ||
			encoded.isEmpty()) {
		return false;
	}
	const QByteArray name = QFile::encodeName(path);
	if (lsetxattr(name.constData(), CUSTOM_ICON_ATTR, encoded.constData(),
			static_cast<size_t>(encoded.size()), 0) == 0) {
		QFile::remove(customIconStorePath(path));
		return true;
	}
	const QString store = customIconStorePath(path);
	if (!QDir().mkpath(QFileInfo(store).absolutePath())) {
		return false;
	}
	QSaveFile file(store);
	return file.open(QIODevice::WriteOnly) && file.write(encoded) == encoded.size() &&
		file.commit();
}

} // namespace

QString gameplayer::gamesFolder() {
	const QByteArray override = qgetenv("ZACOS9_GAMEPLAYER_GAMES");
	if (!override.isEmpty()) {
		return QString::fromLocal8Bit(override);
	}
	return QDir::homePath() + "/Games";
}

bool gameplayer::ensureGamesFolder(QString *error) {
	const QString games = gamesFolder();
	if (!QDir().mkpath(games)) {
		if (error) {
			*error = QString("Game Player couldn't create %1.").arg(games);
		}
		return false;
	}
	QDir().mkpath(games + "/Saved Games");
	if (!hasCustomIcon(games) && !writeCustomIcon(games, gamesFolderIcon())) {
		fprintf(stderr, "zacos9-gameplayer: couldn't give %s its folder icon.\n",
			qPrintable(games));
	}
	return true;
}

QStringList gameplayer::installDroppedFirmware(const QString &configDir) {
	static const QStringList firmwareNames = {
		QStringLiteral("scph5500.bin"),
		QStringLiteral("scph5501.bin"),
		QStringLiteral("scph5502.bin"),
	};
	QStringList installed;
	const QString games = gamesFolder();
	const QString firmware = configDir + "/mednafen/firmware";
	const auto removeDropped = [](const QString &path) {
		if (!QFile::remove(path)) {
			fprintf(stderr, "zacos9-gameplayer: couldn't remove %s after installing it.\n",
				qPrintable(path));
		}
	};
	/* Move the dropped file into place. Across filesystems, copy it and
	 * remove the original; either way nothing is left in ~/Games. */
	const auto moveIntoPlace = [&](const QString &source, const QString &target) {
		if (!QFileInfo::exists(target)) {
			if (!QFile::rename(source, target)) {
				if (!QFile::copy(source, target)) {
					return false;
				}
				removeDropped(source);
			}
		} else {
			removeDropped(source);
		}
		return QFileInfo::exists(target);
	};
	const auto firmwareName = [&](const QFileInfo &file) -> QString {
		const QString name = file.fileName().toLower();
		if (firmwareNames.contains(name)) {
			return name;
		}
		/* Recognise a BIOS the user renamed: the three SCPH images are
		 * 512 KiB, and mednafen documents their SHA-256. */
		if (file.suffix().compare(QLatin1String("bin"), Qt::CaseInsensitive) != 0 ||
				file.size() != 512 * 1024) {
			return {};
		}
		QFile f(file.absoluteFilePath());
		if (!f.open(QIODevice::ReadOnly)) {
			return {};
		}
		const QByteArray digest = QCryptographicHash::hash(f.readAll(),
			QCryptographicHash::Sha256).toHex();
		if (digest == "9c0421858e217805f4abe18698afea8d5aa36ff0727eb8484944e00eb5e7eadb") {
			return QStringLiteral("scph5500.bin");
		}
		if (digest == "11052b6499e466bbf0a709b1f9cb6834a9418e66680387912451e971cf8a1fef") {
			return QStringLiteral("scph5501.bin");
		}
		if (digest == "1faaa18fa820a0225e488d9f086296b8e6c46df739666093987ff7d8fd352c09") {
			return QStringLiteral("scph5502.bin");
		}
		return {};
	};
	if (!QDir().mkpath(firmware)) {
		return installed;
	}
	const QFileInfoList files = QDir(games).entryInfoList(
		QDir::Files | QDir::NoDotAndDotDot, QDir::Name);
	for (const QFileInfo &file : files) {
		const QString name = firmwareName(file);
		if (!name.isEmpty()) {
			const QString target = firmware + "/" + name;
			if (moveIntoPlace(file.absoluteFilePath(), target)) {
				installed << target;
			}
			continue;
		}
		/* A PS2 BIOS is a 4 MB .bin and PCSX2 accepts it under any name.
		 * Copy it to both the 1.6 config folder and the 2.x data folder,
		 * then remove the dropped file from ~/Games. */
		if (file.suffix().compare(QLatin1String("bin"), Qt::CaseInsensitive) == 0 &&
				file.size() == 4 * 1024 * 1024) {
			bool placedEverywhere = true;
			for (const QString &dir : pcsx2BiosDirs()) {
				if (!QDir().mkpath(dir)) {
					placedEverywhere = false;
					continue;
				}
				const QString target = dir + "/" + file.fileName();
				if (QFileInfo::exists(target)) {
					continue;
				}
				if (!QFile::copy(file.absoluteFilePath(), target)) {
					placedEverywhere = false;
					continue;
				}
				installed << target;
			}
			if (placedEverywhere) {
				removeDropped(file.absoluteFilePath());
			}
		}
	}
	return installed;
}

System gameplayer::detectSystem(const QString &path) {
	const QFileInfo info(path);
	if (info.isDir()) {
		return ps3GameFolder(path) ? System::Ps3 : System::None;
	}
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
	if (suffix == "iso" || suffix == "mdf") {
		return System::Ps2;
	}
	return System::None;
}

DiscSystem gameplayer::detectDiscSystem(const QString &mountPath, QString *error) {
	if (!QFileInfo(mountPath).isDir()) {
		if (error) {
			*error = "The disc isn't mounted.";
		}
		return DiscSystem::None;
	}
	QDir dir(mountPath);
	QString cnf;
	for (const QString &name : dir.entryList(QDir::Files | QDir::NoDotAndDotDot)) {
		if (name.compare(QLatin1String("SYSTEM.CNF"), Qt::CaseInsensitive) == 0) {
			cnf = mountPath + "/" + name;
			break;
		}
	}
	if (cnf.isEmpty()) {
		if (QFileInfo(mountPath + "/PS3_GAME").isDir()) {
			if (error) {
				*error = "PlayStation 3 discs can't be read directly. Use a game folder or RPCS3.";
			}
			return DiscSystem::Ps3;
		}
		if (QFileInfo(mountPath + "/PSX.EXE").isFile()) {
			return DiscSystem::Ps1;
		}
		if (error) {
			*error = "That disc isn't a PlayStation game.";
		}
		return DiscSystem::None;
	}
	QFile file(cnf);
	if (!file.open(QIODevice::ReadOnly)) {
		if (error) {
			*error = "The disc's SYSTEM.CNF could not be read.";
		}
		return DiscSystem::None;
	}
	const QString text = QString::fromLatin1(file.read(64 * 1024)).toUpper();
	for (const QString &line : text.split(QLatin1Char('\n'))) {
		const QString setting = line.trimmed();
		if (setting.startsWith(QLatin1String("BOOT2"))) {
			return DiscSystem::Ps2;
		}
		if (setting.startsWith(QLatin1String("BOOT"))) {
			return DiscSystem::Ps1;
		}
	}
	if (QFileInfo(mountPath + "/PS3_GAME").isDir()) {
		if (error) {
			*error = "PlayStation 3 discs can't be read directly. Use a game folder or RPCS3.";
		}
		return DiscSystem::Ps3;
	}
	if (error) {
		*error = "That disc isn't a PlayStation game.";
	}
	return DiscSystem::None;
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
	case System::Ps2: return "PlayStation 2";
	case System::Ps3: return "PlayStation 3";
	case System::N64: return "Nintendo 64";
	case System::Sega: return "Sega";
	case System::GameBoy: return "Game Boy";
	case System::Gba: return "Game Boy Advance";
	case System::None: return "unknown";
	}
	return "unknown";
}

QString gameplayer::biosInstructions(System system) {
	switch (system) {
	case System::Ps1:
		if (mednafenBiosPresent()) {
			return QString();
		}
		return QString("PlayStation needs a Sony BIOS to run.\n\n"
			"Drop scph5501.bin (North America), scph5502.bin (Europe) or\n"
			"scph5500.bin (Japan) into ~/Games, then open the game again.\n\n"
			"Game Player installs it for mednafen. Other region files, like\n"
			"scph1001.bin, also work when named for mednafen.");
	case System::Ps2:
		if (pcsx2BiosPresent()) {
			return QString();
		}
		return QString("PlayStation 2 needs a PS2 BIOS to run.\n\n"
			"Drop the PS2 BIOS .bin (the 4 MB file from your own\n"
			"PlayStation 2) into ~/Games, then open the game again.\n\n"
			"Game Player installs it for PCSX2.");
	case System::Ps3:
		if (rpcs3FirmwarePresent()) {
			return QString();
		}
		return QString("PlayStation 3 needs RPCS3 and its firmware.\n\n"
			"1. Install RPCS3 from https://rpcs3.net (AppImage or Flatpak).\n"
			"2. Run RPCS3 once and choose File > Install Firmware.\n"
			"3. Select the official PS3UPDAT.PUP from Sony.\n\n"
			"Game Player then starts games with RPCS3's --no-gui option.");
	default:
		return QString();
	}
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
	case System::Ps2: {
		l.program = pcsx2Program();
		l.args << "--nogui" << (fullscreen ? "--fullscreen" : "--windowed") << file;
		/* PCSX2 1.6's GSdx plugin needs an X11 window. ZacOS's session
		 * sets GDK_BACKEND=wayland,x11, so without this GTK picks Wayland
		 * and the plugin fails with "Wrong X11 display/window". */
		l.env << "GDK_BACKEND=x11";
		l.env << "SDL_VIDEODRIVER=x11";
		l.env << "SDL_AUDIODRIVER=pulse";
		break;
	}
	case System::Ps3: {
		QStringList command = rpcs3Command(file, error);
		if (command.isEmpty()) {
			return l;
		}
		l.program = command.takeFirst();
		l.args = command;
		/* RPCS3 draws with Qt and Vulkan; ZacOS's XWayland is where
		 * its X11 build runs. Fullscreen is RPCS3's own setting. */
		l.env << "QT_QPA_PLATFORM=xcb";
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
	if (!ensureGamesFolder()) {
		return false;
	}
	QDir().mkpath(configDir);
	QDir().mkpath(configDir + "/mednafen/firmware");
	QDir().mkpath(configDir + "/mupen64plus");
	const QString saveDir = gamesFolder() + "/Saved Games";
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
		f.write((mednafenConfig(fullscreen) + mednafenInputConfig()).toUtf8());
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
		"; Game Player copies PlayStation BIOS files dropped in ~/Games\n"
		"; (scph5500.bin, scph5501.bin, scph5502.bin) into the firmware\n"
		"; folder beside this file.\n"
		"video.fs %1\n"
		"autosave 1\n"
		"load_state 1\n"
		"sound.driver sdl\n"
		"sound.volume 100\n").arg(fullscreen ? 1 : 0);
}

QString gameplayer::mednafenInputConfig() {
	QList<MednafenPad> pads;
	const QString idOverride = qEnvironmentVariable("ZACOS9_GAMEPLAYER_MEDNAFEN_ID");
	if (!idOverride.isEmpty()) {
		MednafenPad pad;
		pad.id = idOverride;
		bool ok = false;
		const int hat = qEnvironmentVariableIntValue("ZACOS9_GAMEPLAYER_MEDNAFEN_HAT", &ok);
		pad.hatX = ok ? hat : -1;
		pad.hatY = ok ? hat + 1 : -1;
		pads << pad;
	} else {
		pads = mednafenPads();
	}
	if (pads.isEmpty()) {
		return QString();
	}
	const MednafenPad pad = pads.first();
	QHash<QString, QString> mapping = sdlMapping(mednafenMappingFor(pad.name));
	if (mapping.isEmpty() && pad.name.contains(QLatin1String("Dual Action"), Qt::CaseInsensitive)) {
		/* SDL has no mapping for it: the same layout as Game Player's
		 * RetroArch profile for this pad. */
		mapping = sdlMapping(QStringLiteral(
			"guid,Logitech Dual Action,a:b1,b:b2,x:b3,y:b0,back:b8,start:b9,"
			"leftshoulder:b4,rightshoulder:b5,lefttrigger:b6,righttrigger:b7,"
			"dpup:h0.1,dpdown:h0.4,dpleft:h0.8,dpright:h0.2,"));
	}
	if (mapping.isEmpty()) {
		return QString();
	}
	struct Input {
		const char *key;
		const char *first;
		const char *second;
	};
	static const Input inputs[] = {
		{ "up", "dpup", "-a1" },
		{ "down", "dpdown", "+a1" },
		{ "left", "dpleft", "-a0" },
		{ "right", "dpright", "+a0" },
		{ "triangle", "y", nullptr },
		{ "cross", "a", nullptr },
		{ "square", "x", nullptr },
		{ "circle", "b", nullptr },
		{ "l1", "leftshoulder", nullptr },
		{ "r1", "rightshoulder", nullptr },
		{ "l2", "lefttrigger", nullptr },
		{ "r2", "righttrigger", nullptr },
		{ "select", "back", nullptr },
		{ "start", "start", nullptr },
	};
	QString config = QStringLiteral("psx.input.port1 gamepad\n");
	bool any = false;
	for (const Input &input : inputs) {
		QStringList bindings;
		for (int i = 0; i < 2; i++) {
			const char *sdl = i == 0 ? input.first : input.second;
			if (!sdl) {
				continue;
			}
			const QString value = i == 0
				? mapping.value(QString::fromLatin1(sdl))
				: QString::fromLatin1(sdl);
			const QString mapped = mednafenInput(value, pad);
			if (!mapped.isEmpty()) {
				bindings << QString("joystick %1 %2").arg(pad.id, mapped);
			}
		}
		if (bindings.isEmpty()) {
			continue;
		}
		any = true;
		config += QString("psx.input.port1.gamepad.%1 %2\n")
			.arg(QString::fromLatin1(input.key), bindings.join(" || "));
	}
	return any ? config : QString();
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
