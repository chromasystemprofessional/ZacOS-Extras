#pragma once

#include <QColor>
#include <QFont>
#include <QPainter>
#include <QPalette>
#include <QPixmap>
#include <QRect>
#include <QString>

class QApplication;

struct PlatinumColors {
    QColor window;
    QColor button;
    QColor light;
    QColor midlight;
    QColor mid;
    QColor dark;
    QColor shadow;
    QColor text;
    QColor base;
    QColor highlight;
    QColor disabled;
};

QFont zacosFont(const QString &preferred, int pixelSize, bool bold = false);
void applyZacos9Theme(QApplication &app);
PlatinumColors platinumColors(const QPalette &palette);

QPixmap makeDisabledPixmap(const QPixmap &pm);
QPixmap makeNavIcon(const QString &name, int size = 20);
QPixmap makeMiniIcon(const QString &name, int size = 14);
QPixmap makeAlertIcon(const QString &kind = QStringLiteral("info"), int size = 32);

void drawPlatinumBevelButton(
    QPainter &painter,
    const QRect &rect,
    const QPalette &palette,
    bool enabled,
    bool down,
    bool hovered);
