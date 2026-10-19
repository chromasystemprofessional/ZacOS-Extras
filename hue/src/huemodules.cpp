#include "huemodules.h"

#include "huecolor.h"

#include <QFile>
#include <QSettings>
#include <QTimer>
#include <algorithm>
#include <cmath>

/* 16 x 16: # outline, o glass, = screw base, * highlight. */
static const char *const BulbArt[16] = {
	".....######.....",
	"...##oooooo##...",
	"..#o*ooooooo#...",
	".#o*oooooooooo#.",
	".#o*oooooooooo#.",
	".#oooooooooooo#.",
	".#oooooooooooo#.",
	"..#oooooooooo#..",
	"...#oooooooo#...",
	"....#oooooo#....",
	".....######.....",
	".....#====#.....",
	".....######.....",
	".....#====#.....",
	"......####......",
	"................",
};

HueModules::HueModules(HueBridge *bridge, CollarExtensionService *service, QObject *parent)
	: QObject(parent), m_bridge(bridge), m_service(service), m_debounce(new QTimer(this)) {
	// A scene change sends an event per light; tell The Collar once they've settled.
	m_debounce->setSingleShot(true);
	m_debounce->setInterval(100);
	connect(m_debounce, &QTimer::timeout, this, [this] { m_service->changed(); });
	m_bridge->changed = [this] { m_debounce->start(); };
	m_service->modules = [this] { return modules(); };
	m_service->activate = [this](const QString &module, const QString &item, std::function<void(const QString &)> done) {
		activate(module, item, std::move(done));
	};
	QSettings s(HueBridge::settingsPath(), QSettings::IniFormat);
	m_chosen = s.value("strip/rooms").toStringList();
}

bool HueModules::shown(const HueGroup &group) const {
	return m_chosen.contains(group.id);
}

QList<HueGroup> HueModules::byName() const {
	// The Hue app's line between rooms and zones isn't one people think in.
	QList<HueGroup> list = m_bridge->groups;
	std::stable_sort(list.begin(), list.end(), [](const HueGroup &a, const HueGroup &b) {
		return QString::localeAwareCompare(a.name.toCaseFolded(), b.name.toCaseFolded()) < 0;
	});
	return list;
}

void HueModules::toggleShown(const QString &group) {
	if (!m_chosen.removeAll(group)) {
		m_chosen << group;
	}
	QSettings s(HueBridge::settingsPath(), QSettings::IniFormat);
	s.setValue("strip/rooms", m_chosen);
	s.remove("strip/show"); // from the first version, which showed rooms unless hidden
	s.remove("strip/hide");
	s.sync();
	// The file also holds the bridge's key.
	QFile::setPermissions(HueBridge::settingsPath(), QFileDevice::ReadOwner | QFileDevice::WriteOwner);
	m_service->changed();
}

QImage HueModules::bulb(QColor color) {
	const bool lit = color.isValid();
	const QRgb ink = lit ? qRgb(0, 0, 0) : qRgb(0x80, 0x80, 0x80);
	const QRgb glass = lit ? color.rgb() : qRgb(0xDD, 0xDD, 0xDD);
	const QRgb base = lit ? qRgb(0xA0, 0xA0, 0xA0) : qRgb(0xC0, 0xC0, 0xC0);
	const QRgb shine = lit ? qRgb(0xFF, 0xFF, 0xFF) : glass;
	QImage image(16, 16, QImage::Format_ARGB32);
	image.fill(Qt::transparent);
	for (int y = 0; y < 16; ++y) {
		for (int x = 0; x < 16; ++x) {
			switch (BulbArt[y][x]) {
			case '#': image.setPixel(x, y, ink); break;
			case 'o': image.setPixel(x, y, glass); break;
			case '=': image.setPixel(x, y, base); break;
			case '*': image.setPixel(x, y, shine); break;
			}
		}
	}
	return image;
}

QColor HueModules::glow(const HueGroup &group) const {
	double r = 0, g = 0, b = 0, brightness = 0;
	int count = 0;
	for (const QString &id : group.lights) {
		const HueLight *l = m_bridge->light(id);
		if (!l || !l->on) {
			continue;
		}
		const QColor c = l->mirek > 0 ? huecolor::rgbFromMirek(l->mirek)
			: l->color ? huecolor::rgbFromXy(l->xy) : huecolor::rgbFromMirek(366); // plain white bulbs are warm
		r += c.redF();
		g += c.greenF();
		b += c.blueF();
		brightness += l->brightness;
		++count;
	}
	if (!count) {
		return {};
	}
	// Dim rooms still read as lit: the icon spans half to full strength.
	const double strength = 0.5 + 0.5 * std::clamp(brightness / count / 100, 0.0, 1.0);
	const double top = std::max({ r, g, b, 1e-6 });
	return QColor::fromRgbF(r / top * strength, g / top * strength, b / top * strength);
}

QList<CollarModule> HueModules::modules() const {
	if (m_bridge->state != HueBridge::Ready || m_bridge->groups.isEmpty()) {
		return { setupModule() };
	}
	QList<CollarModule> list = { hueModule() };
	for (const HueGroup &g : byName()) {
		if (shown(g)) {
			list << groupModule(g);
		}
	}
	return list;
}

CollarModule HueModules::setupModule() const {
	CollarModule m;
	m.id = "hue";
	m.label = "Hue";
	m.icon = bulb(QColor());
	auto &menu = m.menu;
	switch (m_bridge->state) {
	case HueBridge::Searching:
		m.description = "Looking for a Hue Bridge...";
		menu << CollarMenuItem::text(m.description);
		break;
	case HueBridge::NoBridge:
		m.description = m_bridge->error;
		menu << CollarMenuItem::text(m.description) << CollarMenuItem::action("search", "Look Again");
		break;
	case HueBridge::NeedsPairing:
		m.description = "Hue Bridge found: press its link button, then Connect";
		menu << CollarMenuItem::text("Hue Bridge at " + m_bridge->address)
			 << CollarMenuItem::text("Press the round button on the bridge, then:")
			 << CollarMenuItem::action("pair", "Connect to Hue Bridge")
			 << CollarMenuItem::separator()
			 << CollarMenuItem::action("search", "Look for Bridges Again");
		break;
	case HueBridge::Connecting:
		m.description = "Connecting to the Hue Bridge...";
		menu << CollarMenuItem::text(m.description);
		break;
	case HueBridge::Unreachable:
		m.description = m_bridge->error;
		menu << CollarMenuItem::text(m.description) << CollarMenuItem::action("retry", "Try Again")
			 << CollarMenuItem::separator() << CollarMenuItem::action("search", "Look for Bridges Again");
		break;
	case HueBridge::Ready:
		m.description = "No rooms or zones are set up in the Hue app.";
		menu << CollarMenuItem::text(m.description);
		break;
	}
	return m;
}

CollarModule HueModules::hueModule() const {
	HueGroup house;
	house.lights = m_bridge->lights.keys();
	int lit = 0;
	for (const HueLight &l : m_bridge->lights) {
		lit += l.on;
	}
	CollarModule m;
	m.id = "hue";
	m.label = "Hue";
	m.active = lit > 0;
	m.icon = bulb(glow(house));
	m.description = lit == 0 ? QString("Hue: all lights off")
		: lit == 1 ? QString("Hue: 1 light on") : QString("Hue: %1 lights on").arg(lit);
	auto &menu = m.menu;
	if (!m_bridge->allLights.isEmpty()) {
		menu << CollarMenuItem::action("all-off", "Turn Off All Lights", lit > 0) << CollarMenuItem::separator();
	}
	menu << CollarMenuItem::text("Show in Control Strip");
	for (const HueGroup &g : byName()) {
		menu << CollarMenuItem::action("show:" + g.id, "  " + g.name, true, shown(g));
	}
	return m;
}

CollarModule HueModules::groupModule(const HueGroup &g) const {
	CollarModule m;
	m.id = g.id;
	m.label = g.name;
	m.active = g.on;
	m.icon = bulb(g.on ? glow(g) : QColor());
	const int percent = qRound(g.brightness);
	m.description = g.on ? QString("%1: on, %2%").arg(g.name).arg(percent) : g.name + ": off";

	auto &menu = m.menu;
	menu << CollarMenuItem::action("on", "On", true, g.on) << CollarMenuItem::action("off", "Off", true, !g.on)
		 << CollarMenuItem::separator();
	menu << CollarMenuItem::text(g.on ? QString("Brightness: %1%").arg(percent) : QString("Brightness"));
	for (int level : { 25, 50, 75, 100 }) {
		menu << CollarMenuItem::action(QString("brightness:%1").arg(level), QString("  %1%").arg(level), true,
			g.on && std::abs(percent - level) <= 6);
	}

	const QList<HueScene> scenes = m_bridge->scenesFor(g.id);
	if (!scenes.isEmpty()) {
		menu << CollarMenuItem::separator() << CollarMenuItem::text("Scenes");
		for (const HueScene &s : scenes) {
			menu << CollarMenuItem::action("scene:" + s.id, "  " + s.name, true, g.on && s.active);
		}
	}

	return m;
}

void HueModules::activate(const QString &module, const QString &item, std::function<void(const QString &)> done) {
	if (item == "pair") {
		m_bridge->pair(done);
		return;
	}
	if (item == "search") {
		m_bridge->search();
		done(QString());
		return;
	}
	if (item == "retry") {
		m_bridge->retry();
		done(QString());
		return;
	}
	if (item == "all-off") {
		m_bridge->setOn(m_bridge->allLights, false, done);
		return;
	}
	if (item.startsWith("show:")) {
		toggleShown(item.mid(5));
		done(QString());
		return;
	}
	const HueGroup *group = nullptr;
	for (const HueGroup &g : m_bridge->groups) {
		if (g.id == module) {
			group = &g;
		}
	}
	if (!group) {
		done("That room is no longer on the Hue Bridge.");
		return;
	}
	if (item == "on" || item == "off") {
		m_bridge->setOn(group->groupedLight, item == "on", done);
	} else if (item.startsWith("brightness:")) {
		m_bridge->setBrightness(group->groupedLight, item.mid(11).toInt(), done);
	} else if (item.startsWith("scene:")) {
		m_bridge->recallScene(item.mid(6), done);
	} else {
		done(QString());
	}
}
