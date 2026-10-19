#include "huecolor.h"

#include <QList>
#include <algorithm>
#include <cmath>

namespace huecolor {

static double linear(double c) {
	return c > 0.04045 ? std::pow((c + 0.055) / 1.055, 2.4) : c / 12.92;
}

static double gamma(double c) {
	return c <= 0.0031308 ? 12.92 * c : 1.055 * std::pow(c, 1 / 2.4) - 0.055;
}

QPointF xyFromRgb(QColor color) {
	const double r = linear(color.redF()), g = linear(color.greenF()), b = linear(color.blueF());
	const double X = r * 0.664511 + g * 0.154324 + b * 0.162028;
	const double Y = r * 0.283881 + g * 0.668433 + b * 0.047685;
	const double Z = r * 0.000088 + g * 0.072310 + b * 0.986039;
	const double sum = X + Y + Z;
	if (sum <= 0) {
		return { 0.3227, 0.3290 }; // white
	}
	return { X / sum, Y / sum };
}

QColor rgbFromXy(QPointF xy) {
	const double x = xy.x(), y = std::max(xy.y(), 1e-6);
	const double Y = 1.0, X = Y / y * x, Z = Y / y * (1 - x - y);
	double r = X * 1.656492 - Y * 0.354851 - Z * 0.255038;
	double g = -X * 0.707196 + Y * 1.655397 + Z * 0.036152;
	double b = X * 0.051713 - Y * 0.121364 + Z * 1.011530;
	r = std::max(0.0, r);
	g = std::max(0.0, g);
	b = std::max(0.0, b);
	const double top = std::max({ r, g, b, 1e-6 });
	r = gamma(r / top);
	g = gamma(g / top);
	b = gamma(b / top);
	return QColor::fromRgbF(std::clamp(r, 0.0, 1.0), std::clamp(g, 0.0, 1.0), std::clamp(b, 0.0, 1.0));
}

QColor rgbFromMirek(int mirek) {
	// Tanner Helland's blackbody fit, in kelvin/100.
	const double t = 1e6 / std::clamp(mirek, 153, 500) / 100;
	const double r = t <= 66 ? 255 : 329.698727446 * std::pow(t - 60, -0.1332047592);
	const double g = t <= 66 ? 99.4708025861 * std::log(t) - 161.1195681661
		: 288.1221695283 * std::pow(t - 60, -0.0755148492);
	const double b = t >= 66 ? 255 : t <= 19 ? 0 : 138.5177312231 * std::log(t - 10) - 305.0447927307;
	return QColor(std::clamp(int(r), 0, 255), std::clamp(int(g), 0, 255), std::clamp(int(b), 0, 255));
}

const QList<Preset> &presets() {
	static const QList<Preset> list = {
		{ "warm", "Warm White", 370, 0 },
		{ "daylight", "Daylight", 182, 0 },
		{ "red", "Red", 0, qRgb(0xFF, 0x10, 0x10) },
		{ "orange", "Orange", 0, qRgb(0xFF, 0x80, 0x00) },
		{ "yellow", "Yellow", 0, qRgb(0xFF, 0xE0, 0x00) },
		{ "green", "Green", 0, qRgb(0x20, 0xFF, 0x20) },
		{ "blue", "Blue", 0, qRgb(0x20, 0x30, 0xFF) },
		{ "purple", "Purple", 0, qRgb(0x90, 0x20, 0xFF) },
		{ "pink", "Pink", 0, qRgb(0xFF, 0x40, 0xB0) },
	};
	return list;
}

const Preset *preset(const QString &id) {
	for (const Preset &p : presets()) {
		if (id == p.id) {
			return &p;
		}
	}
	return nullptr;
}

} // namespace huecolor
