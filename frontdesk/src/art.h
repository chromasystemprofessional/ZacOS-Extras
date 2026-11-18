#pragma once

#include <QColor>
#include <QFont>
#include <QPainter>
#include <QPixmap>
#include <QRect>
#include <QString>

namespace Art {

QColor desk();
QColor appPanel();
QColor docPanel();
QColor tabFace(bool apps, bool active);
QColor tabText(bool active);

QPixmap stipple(const QColor &a, const QColor &b, int size = 4);
void fillStipple(QPainter &p, const QRect &r, const QPixmap &tile);

void bevel(QPainter &p, const QRect &r, bool pressed = false, bool hover = false);
void label(QPainter &p, const QRect &r, const QString &text, bool selected = false);
QFont smallFont();
QFont tabFont();

} // namespace Art
