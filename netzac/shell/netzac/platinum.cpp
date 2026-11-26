#include "platinum.h"

#include <QApplication>
#include <QFontDatabase>
#include <QImage>
#include <QLinearGradient>
#include <QPen>
#include <QPoint>
#include <QPolygon>
#include <QSet>
#include <QStyle>
#include <QStyleFactory>

QFont zacosFont(const QString &preferred, int pixelSize, bool bold)
{
    const QStringList famList = QFontDatabase::families();
    const QSet<QString> available(famList.begin(), famList.end());
    QFont font;
    if (available.contains(preferred)) {
        font = QFont(preferred);
    } else if (available.contains(QStringLiteral("ZacOS 9 System"))) {
        font = QFont(QStringLiteral("ZacOS 9 System"));
    } else if (available.contains(QStringLiteral("ZacOS 9 Views"))) {
        font = QFont(QStringLiteral("ZacOS 9 Views"));
    } else {
        font = QFont(QStringLiteral("Sans Serif"));
    }
    font.setPixelSize(pixelSize);
    font.setBold(bold);
    font.setStyleStrategy(QFont::NoAntialias);
    return font;
}

void applyZacos9Theme(QApplication &app)
{
    const QStringList keys = QStyleFactory::keys();
    for (const QString &key : keys) {
        if (key.compare(QStringLiteral("zacos9"), Qt::CaseInsensitive) == 0) {
            app.setStyle(key);
            break;
        }
    }
    app.setFont(zacosFont(QStringLiteral("ZacOS 9 System"), 12));
}

PlatinumColors platinumColors(const QPalette &palette)
{
    return PlatinumColors{
        palette.color(QPalette::Window),
        palette.color(QPalette::Button),
        palette.color(QPalette::Light),
        palette.color(QPalette::Midlight),
        palette.color(QPalette::Mid),
        palette.color(QPalette::Dark),
        palette.color(QPalette::Shadow),
        palette.color(QPalette::WindowText),
        palette.color(QPalette::Base),
        palette.color(QPalette::Highlight),
        palette.color(QPalette::Disabled, QPalette::WindowText),
    };
}

QPixmap makeDisabledPixmap(const QPixmap &pm)
{
    QImage img = pm.toImage().convertToFormat(QImage::Format_ARGB32);
    for (int y = 0; y < img.height(); ++y) {
        for (int x = 0; x < img.width(); ++x) {
            const QColor c = img.pixelColor(x, y);
            if (c.alpha() == 0)
                continue;
            const int lum = int(0.299 * c.red() + 0.587 * c.green() + 0.114 * c.blue());
            const int muted = 128 + (lum - 128) / 2;
            img.setPixelColor(x, y, QColor(muted, muted, muted, qMin(c.alpha(), 170)));
        }
    }
    return QPixmap::fromImage(img);
}

QPixmap makeNavIcon(const QString &name, int size)
{
    QPixmap pm(size, size);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, false);

    const QColor black(QStringLiteral("#000000"));
    const QColor white(QStringLiteral("#ffffff"));
    // Classic Mac OS 8 Navigator purple & light pink highlight palette
    const QColor purple(QStringLiteral("#685096"));
    const QColor purpleLight(QStringLiteral("#8b72ba"));
    const QColor purpleDark(QStringLiteral("#3b2a5e"));
    const QColor pinkLight(QStringLiteral("#f4c6e7"));
    const QColor pinkSoft(QStringLiteral("#ffe3f5"));
    const QColor gold(QStringLiteral("#f2c230"));
    const QColor goldDark(QStringLiteral("#996600"));
    const QColor red(QStringLiteral("#cc2222"));
    const QColor redLight(QStringLiteral("#ff6655"));
    const QColor grayLight(QStringLiteral("#eeeeee"));
    const QColor grayMid(QStringLiteral("#999999"));

    if (name == QLatin1String("back")) {
        const QPolygon arrow({
            QPoint(2, 10), QPoint(9, 3), QPoint(9, 7),
            QPoint(17, 7), QPoint(17, 13), QPoint(9, 13), QPoint(9, 17),
        });
        p.setPen(QPen(black, 1));
        p.setBrush(purple);
        p.drawPolygon(arrow);
        p.setPen(pinkLight);
        p.drawLine(4, 9, 8, 5);
        p.drawLine(9, 8, 16, 8);
        p.setPen(purpleLight);
        p.drawLine(9, 9, 16, 9);
        p.setPen(purpleDark);
        p.drawLine(9, 12, 16, 12);
    } else if (name == QLatin1String("forward")) {
        const QPolygon arrow({
            QPoint(17, 10), QPoint(10, 3), QPoint(10, 7),
            QPoint(2, 7), QPoint(2, 13), QPoint(10, 13), QPoint(10, 17),
        });
        p.setPen(QPen(black, 1));
        p.setBrush(purple);
        p.drawPolygon(arrow);
        p.setPen(pinkLight);
        p.drawLine(3, 8, 10, 8);
        p.drawLine(11, 5, 15, 9);
        p.setPen(purpleLight);
        p.drawLine(3, 9, 10, 9);
        p.setPen(purpleDark);
        p.drawLine(3, 12, 10, 12);
    } else if (name == QLatin1String("reload")) {
        p.setPen(black);
        p.setBrush(white);
        p.drawRect(5, 3, 10, 14);
        p.setPen(pinkLight);
        p.drawLine(6, 4, 14, 4);
        p.setPen(purpleDark);
        p.drawArc(2, 2, 15, 15, 30 * 16, 140 * 16);
        p.drawArc(2, 2, 15, 15, 210 * 16, 140 * 16);
        p.setBrush(purple);
        p.setPen(black);
        p.drawPolygon(QPolygon({QPoint(15, 5), QPoint(18, 9), QPoint(12, 9)}));
        p.drawPolygon(QPolygon({QPoint(4, 14), QPoint(1, 10), QPoint(7, 10)}));
        p.setPen(pinkLight);
        p.drawPoint(15, 7);
        p.drawPoint(4, 12);
    } else if (name == QLatin1String("home")) {
        p.setPen(black);
        p.setBrush(purpleDark);
        p.drawRect(13, 3, 2, 4);
        const QPolygon roof({QPoint(10, 2), QPoint(2, 10), QPoint(18, 10)});
        p.setBrush(purple);
        p.drawPolygon(roof);
        p.setPen(pinkLight);
        p.drawLine(10, 4, 4, 9);
        p.setPen(black);
        p.setBrush(pinkSoft);
        p.drawRect(4, 10, 12, 8);
        p.setPen(pinkLight);
        p.drawLine(5, 11, 15, 11);
        p.setPen(black);
        p.setBrush(purpleDark);
        p.drawRect(8, 12, 4, 6);
        p.setBrush(purpleLight);
        p.drawRect(5, 12, 2, 2);
    } else if (name == QLatin1String("find")) {
        // Purple/pink magnifying glass over white page
        p.setPen(black);
        p.setBrush(white);
        p.drawRect(3, 2, 11, 14);
        p.setPen(grayMid);
        p.drawLine(5, 5, 11, 5);
        p.drawLine(5, 8, 9, 8);
        p.drawLine(5, 11, 10, 11);
        p.setPen(black);
        p.setBrush(pinkSoft);
        p.drawEllipse(6, 5, 8, 8);
        p.setPen(pinkLight);
        p.drawArc(7, 6, 6, 6, 45 * 16, 120 * 16);
        p.setPen(QPen(purpleDark, 2));
        p.drawLine(13, 12, 17, 16);
        p.setPen(black);
        p.drawRect(6, 5, 8, 8);
    } else if (name == QLatin1String("print")) {
        p.setPen(black);
        p.setBrush(white);
        p.drawRect(5, 2, 10, 6);
        p.setPen(purpleLight);
        p.drawLine(7, 4, 13, 4);
        p.drawLine(7, 6, 11, 6);
        p.setPen(black);
        p.setBrush(pinkSoft);
        p.drawRect(2, 8, 16, 7);
        p.setPen(pinkLight);
        p.drawLine(3, 9, 17, 9);
        p.setPen(black);
        p.setBrush(purple);
        p.drawRect(4, 11, 12, 3);
        p.setBrush(white);
        p.drawRect(5, 13, 10, 5);
        p.setPen(purpleDark);
        p.drawPoint(15, 9);
    } else if (name == QLatin1String("security_open") || name == QLatin1String("security_locked")) {
        const bool locked = (name == QLatin1String("security_locked"));
        p.setPen(QPen(black, 1));
        if (locked) {
            p.drawArc(6, 2, 8, 9, 0, 180 * 16);
            p.drawLine(6, 6, 6, 9);
            p.drawLine(14, 6, 14, 9);
            p.setBrush(gold);
            p.drawRect(4, 9, 12, 9);
            p.setPen(goldDark);
            p.drawLine(5, 17, 15, 17);
            p.setPen(black);
            p.drawRect(9, 12, 2, 3);
        } else {
            p.drawArc(4, 2, 8, 8, 0, 180 * 16);
            p.drawLine(4, 6, 4, 10);
            p.setBrush(grayLight);
            p.drawRect(5, 9, 11, 9);
            p.setPen(grayMid);
            p.drawLine(6, 17, 15, 17);
            p.setPen(black);
            p.drawRect(10, 12, 2, 3);
        }
    } else if (name == QLatin1String("calm")) {
        // Serene open-book Calm reading icon in Mac OS 8 Navigator purple/pink palette
        p.setPen(black);
        p.setBrush(purple);
        p.drawRect(2, 6, 16, 10);
        p.setPen(purpleDark);
        p.drawLine(3, 15, 16, 15);
        // Left open page (white with pink highlight and text lines)
        p.setPen(black);
        p.setBrush(white);
        p.drawRect(3, 4, 7, 10);
        p.setPen(pinkLight);
        p.drawLine(4, 5, 9, 5);
        p.setPen(purple);
        p.drawLine(5, 7, 8, 7);
        p.drawLine(5, 9, 8, 9);
        p.drawLine(5, 11, 8, 11);
        // Right open page (soft pink with white highlight and text lines)
        p.setPen(black);
        p.setBrush(pinkSoft);
        p.drawRect(10, 4, 7, 10);
        p.setPen(white);
        p.drawLine(11, 5, 16, 5);
        p.setPen(purple);
        p.drawLine(12, 7, 15, 7);
        p.drawLine(12, 9, 15, 9);
        p.drawLine(12, 11, 15, 11);
        // Center spine shadow & hanging bookmark ribbon
        p.setPen(purpleDark);
        p.drawLine(10, 5, 10, 14);
        p.setPen(black);
        p.setBrush(pinkLight);
        p.drawRect(13, 14, 2, 4);
    } else if (name == QLatin1String("stop")) {
        const QPolygon octPts({
            QPoint(6, 2), QPoint(14, 2),
            QPoint(18, 6), QPoint(18, 14),
            QPoint(14, 18), QPoint(6, 18),
            QPoint(2, 14), QPoint(2, 6),
        });
        p.setPen(black);
        p.setBrush(red);
        p.drawPolygon(octPts);
        p.setPen(redLight);
        p.drawLine(6, 3, 13, 3);
        p.drawLine(3, 6, 3, 13);
        p.setPen(white);
        p.drawLine(7, 7, 13, 13);
        p.drawLine(13, 7, 7, 13);
    }

    p.end();
    return pm;
}

QPixmap makeMiniIcon(const QString &name, int size)
{
    QPixmap pm(size, size);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, false);

    const QColor black(QStringLiteral("#000000"));
    const QColor white(QStringLiteral("#ffffff"));
    const QColor teal(QStringLiteral("#2f8f83"));
    const QColor blue(QStringLiteral("#3366aa"));
    const QColor gold(QStringLiteral("#f2c230"));
    const QColor red(QStringLiteral("#cc3333"));
    const QColor gray(QStringLiteral("#bbbbbb"));

    if (name == QLatin1String("bookmarks")) {
        p.setPen(black);
        p.setBrush(teal);
        p.drawRect(2, 1, 9, 11);
        p.setBrush(red);
        p.drawPolygon(QPolygon({QPoint(5, 2), QPoint(9, 2), QPoint(9, 10), QPoint(7, 8), QPoint(5, 10)}));
    } else if (name == QLatin1String("page")) {
        p.setPen(black);
        p.setBrush(white);
        p.drawRect(2, 1, 9, 12);
        p.setPen(blue);
        p.drawLine(4, 4, 9, 4);
        p.drawLine(4, 6, 9, 6);
        p.drawLine(4, 8, 8, 8);
    } else if (name == QLatin1String("home")) {
        p.setPen(black);
        p.setBrush(blue);
        p.drawPolygon(QPolygon({QPoint(7, 1), QPoint(1, 7), QPoint(13, 7)}));
        p.setBrush(gold);
        p.drawRect(3, 7, 8, 6);
    } else if (name == QLatin1String("search")) {
        p.setPen(black);
        p.setBrush(QColor(QStringLiteral("#aaddff")));
        p.drawEllipse(1, 1, 8, 8);
        p.drawLine(8, 8, 12, 12);
    } else if (name == QLatin1String("guide")) {
        p.setPen(black);
        p.setBrush(QColor(QStringLiteral("#88ccaa")));
        p.drawEllipse(1, 1, 11, 11);
        p.setBrush(red);
        p.drawPolygon(QPolygon({QPoint(9, 4), QPoint(7, 8), QPoint(5, 6)}));
    } else if (name == QLatin1String("folder")) {
        p.setPen(black);
        p.setBrush(gold);
        p.drawRect(1, 3, 5, 2);
        p.drawRect(1, 4, 11, 8);
    } else if (name == QLatin1String("navigator")) {
        p.setPen(black);
        p.setBrush(gold);
        p.drawEllipse(2, 2, 10, 10);
        p.setBrush(white);
        p.drawEllipse(5, 5, 4, 4);
        p.drawLine(7, 0, 7, 13);
        p.drawLine(0, 7, 13, 7);
    } else if (name == QLatin1String("mail")) {
        p.setPen(black);
        p.setBrush(white);
        p.drawRect(1, 3, 12, 8);
        p.drawLine(1, 3, 7, 8);
        p.drawLine(13, 3, 7, 8);
    } else if (name == QLatin1String("collabra")) {
        p.setPen(black);
        p.setBrush(gray);
        p.drawRect(4, 1, 8, 9);
        p.setBrush(white);
        p.drawRect(1, 4, 8, 9);
        p.setPen(blue);
        p.drawLine(3, 7, 7, 7);
        p.drawLine(3, 9, 7, 9);
    } else if (name == QLatin1String("composer")) {
        p.setPen(black);
        p.setBrush(white);
        p.drawRect(2, 2, 8, 10);
        p.setPen(red);
        p.drawLine(12, 2, 6, 10);
        p.setPen(gold);
        p.drawLine(11, 2, 5, 10);
    } else if (name == QLatin1String("security_open") || name == QLatin1String("security_locked")) {
        const bool locked = (name == QLatin1String("security_locked"));
        p.setPen(black);
        if (locked) {
            p.drawArc(4, 1, 6, 6, 0, 180 * 16);
            p.drawLine(4, 4, 4, 6);
            p.drawLine(10, 4, 10, 6);
            p.setBrush(gold);
            p.drawRect(3, 6, 8, 6);
            p.drawPoint(7, 9);
        } else {
            p.drawArc(3, 1, 6, 6, 0, 180 * 16);
            p.drawLine(3, 4, 3, 7);
            p.setBrush(QColor(QStringLiteral("#dddddd")));
            p.drawRect(4, 6, 8, 6);
            p.drawPoint(8, 9);
        }
    }

    p.end();
    return pm;
}

QPixmap makeAlertIcon(const QString &kind, int size)
{
    QPixmap pm(size, size);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, false);

    const QColor black(QStringLiteral("#000000"));
    const QColor white(QStringLiteral("#ffffff"));
    const QColor gold(QStringLiteral("#ffcc00"));
    const QColor blue(QStringLiteral("#6666cc"));

    if (kind == QLatin1String("warning")) {
        const QPolygon tri({QPoint(16, 2), QPoint(30, 28), QPoint(2, 28)});
        p.setPen(black);
        p.setBrush(gold);
        p.drawPolygon(tri);
        p.setBrush(black);
        p.drawRect(15, 11, 2, 9);
        p.drawRect(15, 22, 2, 3);
    } else {
        p.setPen(black);
        p.setBrush(white);
        p.drawRect(3, 3, 26, 22);
        p.setBrush(QColor(QStringLiteral("#dddddd")));
        p.drawLine(4, 25, 29, 25);
        p.drawLine(29, 4, 29, 25);
        p.setBrush(blue);
        p.drawRect(14, 7, 4, 4);
        p.drawRect(14, 13, 4, 9);
    }

    p.end();
    return pm;
}

void drawPlatinumBevelButton(
    QPainter &painter,
    const QRect &rect,
    const QPalette &palette,
    bool enabled,
    bool down,
    bool hovered)
{
    const PlatinumColors c = platinumColors(palette);
    const bool isDark = c.button.lightness() < 128;
    const int x1 = rect.left();
    const int y1 = rect.top();
    const int x2 = rect.right();
    const int y2 = rect.bottom();
    if (x2 - x1 < 4 || y2 - y1 < 4)
        return;

    if (!enabled) {
        const QColor border = isDark ? c.dark : c.mid;
        painter.fillRect(rect, c.window);
        painter.setPen(border);
        painter.drawRect(x1, y1, x2 - x1, y2 - y1);
        return;
    }

    QColor fill, outerTl, outerBr, outerMix, innerTl, innerBr, innerMix;
    if (down) {
        if (isDark) {
            fill = QColor(QStringLiteral("#444444"));
            outerTl = QColor(QStringLiteral("#111111"));
            outerBr = QColor(QStringLiteral("#555555"));
            outerMix = QColor(QStringLiteral("#333333"));
            innerTl = QColor(QStringLiteral("#2a2a2a"));
            innerBr = QColor(QStringLiteral("#666666"));
            innerMix = QColor(QStringLiteral("#444444"));
        } else {
            fill = QColor(QStringLiteral("#888888"));
            outerTl = QColor(QStringLiteral("#222222"));
            outerBr = QColor(QStringLiteral("#666666"));
            outerMix = QColor(QStringLiteral("#555555"));
            innerTl = QColor(QStringLiteral("#555555"));
            innerBr = QColor(QStringLiteral("#aaaaaa"));
            innerMix = QColor(QStringLiteral("#888888"));
        }
    } else {
        if (isDark) {
            fill = hovered ? QColor(QStringLiteral("#3c3c3c")) : QColor(QStringLiteral("#333333"));
            outerTl = QColor(QStringLiteral("#4c4c4c"));
            outerBr = QColor(QStringLiteral("#141414"));
            outerMix = QColor(QStringLiteral("#2a2a2a"));
            innerTl = QColor(QStringLiteral("#5a5a5a"));
            innerBr = QColor(QStringLiteral("#222222"));
            innerMix = QColor(QStringLiteral("#333333"));
        } else {
            fill = hovered ? QColor(QStringLiteral("#d8d8d8")) : QColor(QStringLiteral("#cccccc"));
            outerTl = QColor(QStringLiteral("#888888"));
            outerBr = QColor(QStringLiteral("#444444"));
            outerMix = QColor(QStringLiteral("#777777"));
            innerTl = QColor(QStringLiteral("#ffffff"));
            innerBr = QColor(QStringLiteral("#999999"));
            innerMix = QColor(QStringLiteral("#cccccc"));
        }
    }

    painter.fillRect(QRect(x1 + 2, y1 + 2, x2 - x1 - 3, y2 - y1 - 3), fill);

    painter.setPen(outerTl);
    painter.drawLine(x1, y1, x2 - 1, y1);
    painter.drawLine(x1, y1, x1, y2 - 1);
    painter.setPen(outerBr);
    painter.drawLine(x1 + 1, y2, x2, y2);
    painter.drawLine(x2, y1 + 1, x2, y2);
    painter.setPen(outerMix);
    painter.drawPoint(x2, y1);
    painter.drawPoint(x1, y2);

    painter.setPen(innerTl);
    painter.drawLine(x1 + 1, y1 + 1, x2 - 2, y1 + 1);
    painter.drawLine(x1 + 1, y1 + 1, x1 + 1, y2 - 2);
    painter.setPen(innerBr);
    painter.drawLine(x1 + 2, y2 - 1, x2 - 1, y2 - 1);
    painter.drawLine(x2 - 1, y1 + 2, x2 - 1, y2 - 1);
    painter.setPen(innerMix);
    painter.drawPoint(x2 - 1, y1 + 1);
    painter.drawPoint(x1 + 1, y2 - 1);
}
