#include "art.h"

#include <QBrush>
#include <QFontMetrics>
#include <QPen>

namespace {

const QColor kBlack(0x00, 0x00, 0x00);
const QColor kLight(0xF4, 0xF4, 0xF4);
const QColor kMid(0xC8, 0xC8, 0xC8);
const QColor kDark(0x80, 0x80, 0x80);

} // namespace

namespace Art {

QColor desk() {
	/* ZacOS 9's blue-violet desktop, a little deeper for a full-screen shell. */
	return QColor(0x5A, 0x5F, 0x9A);
}

QColor appPanel() {
	/* The warm beige of the Applications panel, tuned to Platinum. */
	return QColor(0xD4, 0xBE, 0xA4);
}

QColor docPanel() {
	/* The blue user folder, tuned to ZacOS's desktop. */
	return QColor(0xA4, 0xBC, 0xD4);
}

QColor tabFace(bool apps, bool active) {
	QColor c = apps ? appPanel() : docPanel();
	if (!active) {
		c = c.darker(118);
	}
	return c;
}

QColor tabText(bool active) {
	return active ? kBlack : QColor(0x40, 0x40, 0x40);
}

QPixmap stipple(const QColor &a, const QColor &b, int size) {
	QPixmap tile(size, size);
	tile.fill(a);
	QPainter p(&tile);
	p.setPen(Qt::NoPen);
	p.setBrush(b);
	/* A deterministic 50% dither, like the classic folder fills. */
	for (int y = 0; y < size; y++) {
		for (int x = 0; x < size; x++) {
			if (((x + y) & 1) == 0) {
				p.drawPoint(x, y);
			}
		}
	}
	return tile;
}

void fillStipple(QPainter &p, const QRect &r, const QPixmap &tile) {
	p.save();
	p.setClipRect(r);
	p.drawTiledPixmap(r, tile);
	p.restore();
}

void bevel(QPainter &p, const QRect &r, bool pressed, bool hover) {
	QColor face = QColor(0xD8, 0xD8, 0xD8);
	if (hover) {
		face = face.lighter(106);
	}
	p.fillRect(r, pressed ? QColor(0xB8, 0xB8, 0xB8) : face);
	p.setPen(QPen(kBlack, 1));
	p.drawRect(r.adjusted(0, 0, -1, -1));
	p.setPen(QPen(pressed ? kDark : kLight, 1));
	p.drawLine(r.left() + 1, r.top() + 1, r.right() - 2, r.top() + 1);
	p.drawLine(r.left() + 1, r.top() + 1, r.left() + 1, r.bottom() - 2);
	p.setPen(QPen(pressed ? kLight : kDark, 1));
	p.drawLine(r.left() + 1, r.bottom() - 1, r.right() - 1, r.bottom() - 1);
	p.drawLine(r.right() - 1, r.top() + 1, r.right() - 1, r.bottom() - 1);
}

void label(QPainter &p, const QRect &r, const QString &text, bool selected) {
	p.save();
	p.fillRect(r, selected ? QColor(0x33, 0x33, 0x99) : QColor(0xEC, 0xEC, 0xEC));
	p.setPen(QPen(kBlack, 1));
	p.drawRect(r.adjusted(0, 0, -1, -1));
	p.setFont(smallFont());
	p.setPen(selected ? Qt::white : kBlack);
	p.drawText(r.adjusted(3, 0, -3, 0), Qt::AlignCenter | Qt::TextSingleLine,
		QFontMetrics(smallFont()).elidedText(text, Qt::ElideRight, r.width() - 6));
	p.restore();
}

QFont smallFont() {
	QFont f;
	f.setPointSize(9);
	return f;
}

QFont tabFont() {
	QFont f;
	f.setPointSize(10);
	f.setBold(true);
	return f;
}

} // namespace Art
