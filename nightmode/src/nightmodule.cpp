#include "nightmodule.h"

#include <QApplication>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QDir>
#include <QFile>
#include <QFormLayout>
#include <QGuiApplication>
#include <QPainter>
#include <QProcess>
#include <QScreen>
#include <QSettings>
#include <QStandardPaths>
#include <QString>
#include <QTimer>
#include <QTimeZone>
#include <QWidget>
#include <cmath>

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

NightOverlay::NightOverlay(QWidget *parent) : QWidget(parent) {
	setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint |
		Qt::Tool | Qt::WindowTransparentForInput);
	setAttribute(Qt::WA_TranslucentBackground);
	setAttribute(Qt::WA_ShowWithoutActivating);
}

void NightOverlay::paintEvent(QPaintEvent *) {
	QPainter p(this);
	p.fillRect(rect(), QColor(255, 120, 0, 110));
}

NightModule::~NightModule() {
	if (m_timer) {
		m_timer->stop();
	}
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

	m_timer = new QTimer(this);
	m_timer->setInterval(60000);
	connect(m_timer, &QTimer::timeout, this, &NightModule::updateAuto);
	m_timer->start();
	updateAuto();
}

void NightModule::load() {
	const QString path = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) +
		QStringLiteral("/zacos9/night-mode.conf");
	QSettings s(path, QSettings::IniFormat);
	m_on = s.value("on", false).toBool();
	m_auto = s.value("auto", false).toBool();
	m_lat = s.value("lat", 51.5).toDouble();
	m_lon = s.value("lon", -0.13).toDouble();
	if (m_on && !m_auto) {
		// restore a manual on state
	}
	apply();
}

void NightModule::save() {
	const QString path = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) +
		QStringLiteral("/zacos9/night-mode.conf");
	QSettings s(path, QSettings::IniFormat);
	s.setValue("on", m_on);
	s.setValue("auto", m_auto);
	s.setValue("lat", m_lat);
	s.setValue("lon", m_lon);
	s.sync();
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
		auto *overlay = new NightOverlay(nullptr);
		overlay->setScreen(screen);
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

void NightModule::updateAuto() {
	if (!m_auto) {
		return;
	}
	int sunrise = 0, sunset = 0;
	sunTimes(m_lat, m_lon, sunrise, sunset);
	const QTime now = QTime::currentTime();
	const int nowMin = now.hour() * 60 + now.minute();
	bool shouldOn;
	if (sunset > sunrise) {
		// night spans midnight: on if now >= sunset or now < sunrise
		shouldOn = (nowMin >= sunset) || (nowMin < sunrise);
	} else {
		shouldOn = (nowMin >= sunset) && (nowMin < sunrise);
	}
	if (shouldOn != m_on) {
		m_on = shouldOn;
		apply();
		save();
		m_service->changed();
	}
}

void NightModule::showLocationDialog() {
	QDialog dlg;
	dlg.setWindowTitle(QStringLiteral("Night Mode location"));
	QFormLayout *form = new QFormLayout(&dlg);
	QDoubleSpinBox *lat = new QDoubleSpinBox(&dlg);
	lat->setRange(-90.0, 90.0);
	lat->setDecimals(4);
	lat->setValue(m_lat);
	QDoubleSpinBox *lon = new QDoubleSpinBox(&dlg);
	lon->setRange(-180.0, 180.0);
	lon->setDecimals(4);
	lon->setValue(m_lon);
	form->addRow(QStringLiteral("Latitude"), lat);
	form->addRow(QStringLiteral("Longitude"), lon);
	QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
	form->addRow(buttons);
	connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
	connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
	if (dlg.exec() == QDialog::Accepted) {
		m_lat = lat->value();
		m_lon = lon->value();
		save();
		updateAuto();
	}
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
	CollarMenuItem autoItem = CollarMenuItem::action(QStringLiteral("auto"),
		QStringLiteral("Automatic"), true, m_auto);
	CollarMenuItem locItem = CollarMenuItem::action(QStringLiteral("location"),
		QStringLiteral("Set Location…"), true, false);
	m.menu << item << CollarMenuItem::separator() << autoItem << locItem;
	return { m };
}

void NightModule::activate(const QString &module, const QString &item,
	std::function<void(const QString &)> done) {
	Q_UNUSED(module);
	if (item == QStringLiteral("toggle")) {
		m_auto = false;
		m_on = !m_on;
		apply();
		save();
		m_service->changed();
	} else if (item == QStringLiteral("auto")) {
		m_auto = !m_auto;
		if (m_auto) {
			updateAuto();
		}
		save();
		m_service->changed();
	} else if (item == QStringLiteral("location")) {
		showLocationDialog();
		m_service->changed();
	}
	done(QString());
}

QImage NightModule::icon(bool on) {
	return makeIcon(on ? MoonArt : SunArt, on ? qRgb(0x40, 0x40, 0x80) : qRgb(0xC0, 0x80, 0x00));
}

/* ---- sunrise / sunset ---------------------------------------------------- */

double NightModule::sunHourAngle(double lat, double decl) {
	const double latr = lat * M_PI / 180.0;
	const double declr = decl * M_PI / 180.0;
	double cosHa = (cos(90.833 * M_PI / 180.0) - sin(latr) * sin(declr)) /
		(cos(latr) * cos(declr));
	if (cosHa > 1.0) cosHa = 1.0;
	if (cosHa < -1.0) cosHa = -1.0;
	return acos(cosHa) * 180.0 / M_PI;
}

void NightModule::sunTimes(double lat, double lon, int &sunriseMin, int &sunsetMin) {
	QDate today = QDate::currentDate();
	int Y = today.year(), M = today.month(), D = today.day();
	if (M <= 2) { Y -= 1; M += 12; }
	int A = Y / 100;
	int B = 2 - A + A / 4;
	double jd = std::floor(365.25 * (Y + 4716)) + std::floor(30.6001 * (M + 1)) + D + B - 1524.5;
	double T = (jd - 2451545.0) / 36525.0;

	double L0 = 280.46646 + T * (36000.76983 + T * 0.0003032);
	double Mmean = 357.52911 + T * (35999.05029 - 0.0001537 * T);
	double e = 0.016708634 - T * (0.000042037 + 0.0000001267 * T);
	double C = sin(Mmean * M_PI / 180.0) * 1.914602
		- sin(2 * Mmean * M_PI / 180.0) * 0.019993
		+ sin(3 * Mmean * M_PI / 180.0) * 0.000289;
	double trueLon = L0 + C;
	double omega = 125.04 - 1934.136 * T;
	double appLon = trueLon - 0.00569 - 0.00478 * sin(omega * M_PI / 180.0);
	double meanObliq = 23.439291 - T * (0.0130042 + T * (0.00000016 - T * 0.0000005));
	double corrObliq = meanObliq + 0.00256 * cos(omega * M_PI / 180.0);
	double ra = atan2(cos(corrObliq * M_PI / 180.0) * sin(appLon * M_PI / 180.0),
		cos(appLon * M_PI / 180.0)) * 180.0 / M_PI;
	double decl = asin(sin(corrObliq * M_PI / 180.0) * sin(appLon * M_PI / 180.0)) * 180.0 / M_PI;

	double eqTime = 4.0 * (L0 - 0.0057183 - ra);
	// normalise L0 to [0,360)
	while (L0 >= 360.0) L0 -= 360.0;
	while (L0 < 0.0) L0 += 360.0;
	eqTime = 4.0 * (L0 - 0.0057183 - ra);
	while (eqTime > 20.0) eqTime -= 1440.0; // not strictly needed
	// wrap ra to [0,360)
	while (ra < 0.0) ra += 360.0;

	double ha = sunHourAngle(lat, decl);

	double sunriseUTC = 720.0 - 4.0 * (lon + ha) - eqTime;
	double sunsetUTC = 720.0 - 4.0 * (lon - ha) - eqTime;

	QDateTime noonUtc(today, QTime(12, 0), Qt::UTC);
	int offsetMin = QTimeZone::systemTimeZone().offsetFromUtc(noonUtc) / 60;

	sunriseMin = (int)(sunriseUTC + offsetMin);
	sunsetMin = (int)(sunsetUTC + offsetMin);
	if (sunriseMin < 0) sunriseMin += 1440;
	if (sunsetMin < 0) sunsetMin += 1440;
	if (sunriseMin > 1440) sunriseMin -= 1440;
	if (sunsetMin > 1440) sunsetMin -= 1440;
}
