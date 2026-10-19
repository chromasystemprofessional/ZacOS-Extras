#pragma once

/* Hue's colour spaces: CIE 1931 xy (colour lights), mireds (white ambiance) and sRGB (our icons). */

#include <QColor>
#include <QPointF>

namespace huecolor {

/* sRGB -> CIE xy, the conversion Philips documents for its lights. */
QPointF xyFromRgb(QColor color);
/* CIE xy at full brightness -> sRGB, normalised so the brightest channel is 255. */
QColor rgbFromXy(QPointF xy);
/* A colour temperature in mireds (153-500) -> roughly the white it looks like. */
QColor rgbFromMirek(int mirek);

struct Preset {
	const char *id, *name;
	int mirek;   /* > 0: a white, by colour temperature */
	QRgb rgb;    /* otherwise a colour */
};
/* Warm White, Daylight, then the rainbow. */
const QList<Preset> &presets();
const Preset *preset(const QString &id);

} // namespace huecolor
