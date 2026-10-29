#include "nightmodule.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QPainter>
#include <QProcess>
#include <QScreen>
#include <QStandardPaths>
#include <QString>
#include <QWidget>

static const char *const MoonArt[16] = {
	"..............##",
	"............####",
	"..........######",
	"........########",
	"........########",
	"........########",
	"........########",
	"........########",
	"........########",
	"........########",
	"........########",
	"........########",
	"..........######",
	"............####",
	"..............##",
	"................",
};

static const char *const SunArt[16] = {
	"...#.......#....",
	"....#.....#.....",
	".....#####......",
	"...##.....##....",
	"..#.........#...",
	"..#.........#...",
	"..#.........#...",
	"...##.....##....",
	".....#####......",
	"....#.....#.....",
	"...#.......#....",
	"................",
	"................",
	"................",
	"................",
	"................",
};

static QImage makeIcon(const char *const art[16], QRgb ink) {
	QImage image(16, 16, QImage::Format_ARGB32);
	image.fill(Qt::transparent);
	for (int y = 0; y < 16; ++y) {
		for (int x = 0; x < 16; ++x) {
			if (art[y][x] == '#') {
				image.setPixel(x, y, ink);
			}
		}
	}
	return image;
}

NightModule::~NightModule() {
	destroyOverlays();
}

NightModule::NightModule(CollarExtensionService *service, QObject *parent)
	: QObject(parent), m_service(service) {
	load();
	m_service->modules = [this] { return modules(); };
	m_service->activate = [this](const QString &module, const QString &item,
		std::function<void(const QString &)> done) {
		activate(module, item, std::move(done));
	};
}

void NightModule::load() {
	const QString path = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) +
		QStringLiteral("/zacos9/night-mode");
	QFile f(path);
	if (f.open(QIODevice::ReadOnly)) {
		m_on = QString::fromUtf8(f.readAll()).trimmed() == QStringLiteral("on");
	}
}

void NightModule::save() {
	const QString dir = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + QStringLiteral("/zacos9");
	QDir().mkpath(dir);
	QFile f(dir + QStringLiteral("/night-mode"));
	if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		f.write(m_on ? "on\n" : "off\n");
	}
}

void NightModule::apply() {
	if (m_on) {
		createOverlays();
	} else {
		destroyOverlays();
	}
}

void NightModule::createOverlays() {
	destroyOverlays();
	for (QScreen *screen : QGuiApplication::screens()) {
		auto *overlay = new QWidget(nullptr);
		overlay->setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint |
			Qt::Tool | Qt::WindowTransparentForInput);
		overlay->setAttribute(Qt::WA_TranslucentBackground);
		overlay->setAttribute(Qt::WA_ShowWithoutActivating);
		overlay->setAttribute(Qt::WA_StyledBackground);
		overlay->setStyleSheet(QStringLiteral("background-color: rgba(255, 120, 0, 90);"));
		overlay->setGeometry(screen->geometry());
		overlay->show();
		overlay->raise();
		m_overlays.append(overlay);
	}
}

void NightModule::destroyOverlays() {
	for (QWidget *w : m_overlays) {
		w->hide();
		w->deleteLater();
	}
	m_overlays.clear();
}

QList<CollarModule> NightModule::modules() const {
	CollarModule m;
	m.id = QStringLiteral("night-mode");
	m.label = QStringLiteral("Night Mode");
	m.description = QStringLiteral("Warm the screen colour for the evening.");
	m.active = m_on;
	m.icon = icon(m_on);
	CollarMenuItem item = CollarMenuItem::action(QStringLiteral("toggle"),
		m_on ? QStringLiteral("Turn Off") : QStringLiteral("Turn On"), true, m_on);
	m.menu << item;
	return { m };
}

void NightModule::activate(const QString &module, const QString &item,
	std::function<void(const QString &)> done) {
	Q_UNUSED(module);
	if (item == QStringLiteral("toggle")) {
		m_on = !m_on;
		apply();
		save();
		m_service->changed();
	}
	done(QString());
}

QImage NightModule::icon(bool on) {
	return makeIcon(on ? MoonArt : SunArt, on ? qRgb(0x40, 0x40, 0x80) : qRgb(0xC0, 0x80, 0x00));
}
