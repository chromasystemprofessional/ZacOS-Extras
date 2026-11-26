#include "widgets.h"

#include "i18n.h"
#include "platinum.h"

#include <QAction>
#include <QApplication>
#include <QEnterEvent>
#include <QEvent>
#include <QFocusEvent>
#include <QHBoxLayout>
#include <QIcon>
#include <QLineEdit>
#include <QLinearGradient>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QPolygon>
#include <QSizePolicy>
#include <QStyle>
#include <QStyleOptionButton>
#include <QVBoxLayout>

PlatinumNavButton::PlatinumNavButton(const QString &label, const QString &iconName, QWidget *parent)
    : QPushButton(parent)
    , m_label(label)
    , m_iconPm(makeNavIcon(iconName, 20))
    , m_disabledPm(makeDisabledPixmap(m_iconPm))
{
    m_holdTimer.setSingleShot(true);
    m_holdTimer.setInterval(350);
    connect(&m_holdTimer, &QTimer::timeout, this, &PlatinumNavButton::onHoldTimeout);

    setFixedSize(54, 44);
    setFocusPolicy(Qt::NoFocus);
    setFont(zacosFont(QStringLiteral("ZacOS 9 Views"), 7));
}

void PlatinumNavButton::setShowText(bool show)
{
    if (m_showText == show)
        return;
    m_showText = show;
    if (m_showText)
        setFixedSize(54, 44);
    else
        setFixedSize(30, 28);
    update();
}

void PlatinumNavButton::setHistoryMenuProvider(HistoryMenuProvider provider)
{
    m_historyMenuProvider = std::move(provider);
    update();
}

void PlatinumNavButton::setIconName(const QString &iconName)
{
    m_iconPm = makeNavIcon(iconName, 20);
    m_disabledPm = makeDisabledPixmap(m_iconPm);
    update();
}

void PlatinumNavButton::showHistoryPopup()
{
    if (!m_historyMenuProvider || !isEnabled())
        return;

    QMenu menu(this);
    m_historyMenuProvider(menu);
    if (!menu.isEmpty()) {
        m_holdTriggered = true;
        menu.exec(mapToGlobal(QPoint(0, height())));
    }
    setDown(false);
    update();
}

void PlatinumNavButton::onHoldTimeout()
{
    showHistoryPopup();
}

void PlatinumNavButton::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::RightButton && m_historyMenuProvider && isEnabled()) {
        showHistoryPopup();
        event->accept();
        return;
    }
    m_holdTriggered = false;
    if (event->button() == Qt::LeftButton && m_historyMenuProvider && isEnabled())
        m_holdTimer.start();
    QPushButton::mousePressEvent(event);
}

void PlatinumNavButton::mouseReleaseEvent(QMouseEvent *event)
{
    m_holdTimer.stop();
    if (m_holdTriggered) {
        m_holdTriggered = false;
        setDown(false);
        update();
        event->accept();
        return;
    }
    QPushButton::mouseReleaseEvent(event);
}

void PlatinumNavButton::enterEvent(QEnterEvent *event)
{
    m_hovered = true;
    update();
    QPushButton::enterEvent(event);
}

void PlatinumNavButton::leaveEvent(QEvent *event)
{
    m_hovered = false;
    update();
    QPushButton::leaveEvent(event);
}

void PlatinumNavButton::paintEvent(QPaintEvent * /*event*/)
{
    QPainter painter(this);
    const bool pressed = isDown() || isChecked();
    drawPlatinumBevelButton(
        painter, rect(), palette(), isEnabled(), pressed, m_hovered);

    const int offset = pressed ? 1 : 0;
    const QPixmap &pm = isEnabled() ? m_iconPm : m_disabledPm;
    const int ix = (width() - pm.width()) / 2 + offset;
    const int iy = m_showText ? (5 + offset) : ((height() - pm.height()) / 2 + offset);
    painter.drawPixmap(ix, iy, pm);

    const QPalette::ColorGroup group = isEnabled() ? QPalette::Normal : QPalette::Disabled;
    const QColor textColor = palette().color(group, QPalette::ButtonText);

    if (m_showText) {
        painter.setFont(font());
        painter.setPen(textColor);
        const QRect textRect(2 + offset, 26 + offset, width() - 4, 14);
        painter.drawText(textRect, Qt::AlignCenter, m_label);
    }

    if (m_historyMenuProvider) {
        const int tx = width() - 9 + offset;
        const int ty = height() - 7 + offset;
        painter.setBrush(textColor);
        painter.setPen(Qt::NoPen);
        painter.drawPolygon(QPolygon({
            QPoint(tx, ty),
            QPoint(tx + 5, ty),
            QPoint(tx + 2, ty + 3),
        }));
    }
}

PlatinumCompactButton::PlatinumCompactButton(
    const QString &label,
    const QString &iconName,
    bool hasArrow,
    QWidget *parent)
    : QPushButton(parent)
    , m_label(label)
    , m_iconPm(makeMiniIcon(iconName, 14))
    , m_hasArrow(hasArrow)
{
    setFocusPolicy(Qt::NoFocus);
    setFont(zacosFont(QStringLiteral("ZacOS 9 Views"), 9));
    setFixedHeight(22);
    const QFontMetrics fm = fontMetrics();
    const int w = 8 + m_iconPm.width() + 4 + fm.horizontalAdvance(label)
        + (hasArrow ? 12 : 6);
    setFixedWidth(qMax(44, w));
}

void PlatinumCompactButton::paintEvent(QPaintEvent * /*event*/)
{
    QPainter painter(this);
    QStyleOptionButton opt;
    initStyleOption(&opt);
    opt.text.clear();
    opt.icon = QIcon();
    style()->drawControl(QStyle::CE_PushButtonBevel, &opt, &painter, this);

    const int offset = isDown() ? 1 : 0;
    int x = 5 + offset;
    const int iy = (height() - m_iconPm.height()) / 2 + offset;
    painter.drawPixmap(x, iy, m_iconPm);
    x += m_iconPm.width() + 4;

    painter.setFont(font());
    painter.setPen(palette().color(QPalette::ButtonText));
    const int textW = width() - x - (m_hasArrow ? 12 : 4);
    painter.drawText(
        QRect(x, offset, textW, height()),
        Qt::AlignVCenter | Qt::AlignLeft,
        m_label);

    if (m_hasArrow) {
        const int ax = width() - 9 + offset;
        const int ay = height() / 2 - 1 + offset;
        painter.setBrush(palette().color(QPalette::ButtonText));
        painter.setPen(Qt::NoPen);
        painter.drawPolygon(QPolygon({
            QPoint(ax, ay),
            QPoint(ax + 5, ay),
            QPoint(ax + 2, ay + 3),
        }));
    }
}

PlatinumMiniIconButton::PlatinumMiniIconButton(
    const QString &iconName,
    const QString &tooltip,
    QWidget *parent)
    : QPushButton(parent)
    , m_iconPm(makeMiniIcon(iconName, 14))
{
    setFixedSize(20, 20);
    setFocusPolicy(Qt::NoFocus);
    setToolTip(tooltip);
}

void PlatinumMiniIconButton::setIconName(const QString &iconName)
{
    m_iconPm = makeMiniIcon(iconName, 14);
    update();
}

void PlatinumMiniIconButton::paintEvent(QPaintEvent * /*event*/)
{
    QPainter painter(this);
    QStyleOptionButton opt;
    initStyleOption(&opt);
    opt.text.clear();
    opt.icon = QIcon();
    style()->drawControl(QStyle::CE_PushButtonBevel, &opt, &painter, this);

    const int offset = isDown() ? 1 : 0;
    const int ix = (width() - m_iconPm.width()) / 2 + offset;
    const int iy = (height() - m_iconPm.height()) / 2 + offset;
    painter.drawPixmap(ix, iy, m_iconPm);
}

NetzacThrobber::NetzacThrobber(QWidget *parent)
    : QPushButton(parent)
{
    setFixedSize(44, 44);
    setFocusPolicy(Qt::NoFocus);
    setToolTip(ztr("Netzac Home"));
    m_timer.setInterval(75);
    connect(&m_timer, &QTimer::timeout, this, &NetzacThrobber::advanceFrame);
}

void NetzacThrobber::setCompact(bool compact)
{
    if (m_compact == compact)
        return;
    m_compact = compact;
    setFixedSize(m_compact ? QSize(28, 28) : QSize(44, 44));
    update();
}

void NetzacThrobber::setLoading(bool loading)
{
    if (loading == m_animating)
        return;
    m_animating = loading;
    if (loading) {
        m_timer.start();
    } else {
        m_timer.stop();
        m_frame = 0;
    }
    update();
}

void NetzacThrobber::advanceFrame()
{
    m_frame = (m_frame + 1) % 16;
    update();
}

void NetzacThrobber::paintEvent(QPaintEvent * /*event*/)
{
    QPainter painter(this);
    drawPlatinumBevelButton(
        painter, rect(), palette(), isEnabled(), isDown(), underMouse());

    const QRect inner = rect().adjusted(4, 4, -4, -4);
    const PlatinumColors c = platinumColors(palette());
    painter.setPen(c.shadow);
    painter.drawRect(inner.adjusted(-1, -1, 0, 0));

    QLinearGradient grad(inner.left(), inner.top(), inner.left(), inner.bottom());
    grad.setColorAt(0.0, QColor(QStringLiteral("#040b24")));
    grad.setColorAt(0.62, QColor(QStringLiteral("#0b3a4a")));
    grad.setColorAt(1.0, QColor(QStringLiteral("#1e7874")));
    painter.fillRect(inner, grad);

    if (m_animating) {
        painter.save();
        painter.setClipRect(inner);
        static constexpr int kStars[6][2] = {
            {6, 7}, {15, 5}, {27, 9}, {33, 14}, {10, 17}, {29, 20}
        };
        for (int idx = 0; idx < 6; ++idx) {
            const int sx = kStars[idx][0];
            const int sy = kStars[idx][1];
            const int px = inner.left() + (sx + m_frame * (idx % 2 + 2)) % inner.width();
            const int py = inner.top() + (sy + m_frame) % qMax(1, inner.height() - 10);
            painter.setPen(QColor(QStringLiteral("#aaffee")));
            painter.drawLine(px, py, px - 4, py + 2);
            painter.setPen(QColor(QStringLiteral("#ffffff")));
            painter.drawPoint(px, py);
        }
        painter.restore();
    } else {
        painter.setPen(QColor(QStringLiteral("#88bbbb")));
        static constexpr int kIdleStars[4][2] = {{8, 9}, {31, 8}, {12, 16}, {30, 18}};
        for (const auto &pt : kIdleStars)
            painter.drawPoint(inner.left() + pt[0], inner.top() + pt[1]);
    }

    painter.save();
    painter.setClipRect(inner);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(QStringLiteral("#114b4b")));
    painter.drawChord(inner.left() - 6, inner.bottom() - 10, inner.width() + 12, 22, 0, 180 * 16);
    painter.setPen(QColor(QStringLiteral("#66cccc")));
    painter.drawArc(inner.left() - 6, inner.bottom() - 10, inner.width() + 12, 22, 20 * 16, 140 * 16);
    painter.restore();

    const int offset = isDown() ? 1 : 0;
    QFont nFont(QStringLiteral("Nimbus Roman"));
    nFont.setPixelSize(m_compact ? 17 : 27);
    nFont.setBold(true);
    painter.setFont(nFont);
    painter.setPen(QColor(QStringLiteral("#000000")));
    painter.drawText(inner.adjusted(1 + offset, offset, 1, m_compact ? -1 : -2), Qt::AlignCenter, QStringLiteral("Z"));
    painter.setPen(QColor(QStringLiteral("#ffffff")));
    painter.drawText(inner.adjusted(offset, -1 + offset, 0, m_compact ? -2 : -3), Qt::AlignCenter, QStringLiteral("Z"));
}

PlatinumDropArrowButton::PlatinumDropArrowButton(QWidget *parent)
    : QPushButton(parent)
{
    setFixedSize(20, 22);
    setFocusPolicy(Qt::NoFocus);
    setToolTip(ztr("Show location history"));
}

void PlatinumDropArrowButton::enterEvent(QEnterEvent *event)
{
    m_hovered = true;
    update();
    QPushButton::enterEvent(event);
}

void PlatinumDropArrowButton::leaveEvent(QEvent *event)
{
    m_hovered = false;
    update();
    QPushButton::leaveEvent(event);
}

void PlatinumDropArrowButton::paintEvent(QPaintEvent * /*event*/)
{
    QPainter painter(this);
    drawPlatinumBevelButton(
        painter, rect(), palette(), isEnabled(), isDown(), m_hovered);

    const int offset = isDown() ? 1 : 0;
    const int cx = width() / 2 + offset;
    const int cy = height() / 2 - 1 + offset;
    painter.setBrush(palette().color(QPalette::ButtonText));
    painter.setPen(Qt::NoPen);
    painter.drawPolygon(QPolygon({
        QPoint(cx - 4, cy - 1),
        QPoint(cx + 4, cy - 1),
        QPoint(cx, cy + 3),
    }));
}

PlatinumPopupButton::PlatinumPopupButton(QWidget *parent)
    : QPushButton(parent)
{
    setFixedHeight(24);
    setMinimumWidth(140);
    setFocusPolicy(Qt::StrongFocus);
    setFont(zacosFont(QStringLiteral("ZacOS 9 System"), 12));
    connect(this, &QPushButton::clicked, this, &PlatinumPopupButton::showPopup);
}

void PlatinumPopupButton::addItem(const QString &label, const QVariant &data)
{
    m_items.append(Item{label, data});
    if (m_currentIndex < 0)
        m_currentIndex = 0;
    update();
}

void PlatinumPopupButton::setCurrentData(const QVariant &data)
{
    for (int i = 0; i < m_items.size(); ++i) {
        if (m_items.at(i).data == data) {
            setCurrentIndex(i);
            return;
        }
    }
}

void PlatinumPopupButton::setCurrentIndex(int index)
{
    if (index < 0 || index >= m_items.size() || index == m_currentIndex)
        return;
    m_currentIndex = index;
    update();
    emit currentIndexChanged(m_currentIndex);
}

QVariant PlatinumPopupButton::currentData() const
{
    if (m_currentIndex >= 0 && m_currentIndex < m_items.size())
        return m_items.at(m_currentIndex).data;
    return {};
}

void PlatinumPopupButton::showPopup()
{
    if (m_items.isEmpty())
        return;

    QMenu menu(this);
    menu.setFont(zacosFont(QStringLiteral("ZacOS 9 System"), 12));
    menu.setMinimumWidth(width());
    for (int i = 0; i < m_items.size(); ++i) {
        QAction *act = menu.addAction(m_items.at(i).label);
        act->setCheckable(true);
        act->setChecked(i == m_currentIndex);
        connect(act, &QAction::triggered, this, [this, i]() { setCurrentIndex(i); });
    }
    menu.exec(mapToGlobal(QPoint(0, height())));
}

void PlatinumPopupButton::paintEvent(QPaintEvent * /*event*/)
{
    QPainter painter(this);
    QStyleOptionButton opt;
    initStyleOption(&opt);
    opt.text.clear();
    opt.icon = QIcon();
    style()->drawControl(QStyle::CE_PushButtonBevel, &opt, &painter, this);

    const int offset = isDown() ? 1 : 0;
    const QPalette::ColorGroup group = isEnabled() ? QPalette::Normal : QPalette::Disabled;
    const QColor textColor = palette().color(group, QPalette::ButtonText);
    const QString label = (m_currentIndex >= 0 && m_currentIndex < m_items.size())
        ? m_items.at(m_currentIndex).label
        : QString();

    painter.setFont(font());
    painter.setPen(textColor);
    const QString elided = painter.fontMetrics().elidedText(label, Qt::ElideRight, qMax(10, width() - 32));
    painter.drawText(
        QRect(10 + offset, offset, qMax(10, width() - 32), height()),
        Qt::AlignVCenter | Qt::AlignLeft,
        elided);

    const PlatinumColors c = platinumColors(palette());
    const int divX = width() - 20 + offset;
    painter.setPen(c.dark);
    painter.drawLine(divX, 4, divX, height() - 5);
    painter.setPen(c.light);
    painter.drawLine(divX + 1, 4, divX + 1, height() - 5);

    const int cx = width() - 10 + offset;
    const int cy = height() / 2 + offset;
    painter.setBrush(textColor);
    painter.setPen(Qt::NoPen);
    // Up triangle
    painter.drawPolygon(QPolygon({QPoint(cx - 3, cy - 2), QPoint(cx + 3, cy - 2), QPoint(cx, cy - 5)}));
    // Down triangle
    painter.drawPolygon(QPolygon({QPoint(cx - 3, cy + 2), QPoint(cx + 3, cy + 2), QPoint(cx, cy + 5)}));
}

PlatinumLocationBar::PlatinumLocationBar(const QStringList &initialUrls, QWidget *parent)
    : QWidget(parent)
    , m_urls(initialUrls)
{
    setFixedHeight(22);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    auto *lay = new QHBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(1);

    m_edit = new QLineEdit(this);
    m_edit->setFixedHeight(22);
    m_edit->setMinimumWidth(80);
    m_edit->setFont(zacosFont(QStringLiteral("ZacOS 9 System"), 12));
    m_edit->installEventFilter(this);
    connect(m_edit, &QLineEdit::returnPressed, this, [this]() {
        if (!m_edit->isReadOnly())
            emit urlSubmitted(m_edit->text());
    });
    lay->addWidget(m_edit, 1);

    m_dropBtn = new PlatinumDropArrowButton(this);
    connect(m_dropBtn, &QPushButton::clicked, this, &PlatinumLocationBar::openDropdown);
    lay->addWidget(m_dropBtn);
}

void PlatinumLocationBar::setText(const QString &text)
{
    m_edit->setText(text);
}

QString PlatinumLocationBar::text() const
{
    return m_edit->text();
}

void PlatinumLocationBar::recordUrl(const QString &url)
{
    if (!url.isEmpty() && !m_urls.contains(url))
        m_urls.prepend(url);
}

void PlatinumLocationBar::setReadOnly(bool readOnly)
{
    m_edit->setReadOnly(readOnly);
    m_dropBtn->setEnabled(!readOnly);
    m_dropBtn->setVisible(!readOnly);
}

bool PlatinumLocationBar::isReadOnly() const
{
    return m_edit->isReadOnly();
}

bool PlatinumLocationBar::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_edit) {
        if (event->type() == QEvent::FocusIn) {
            auto *fe = static_cast<QFocusEvent *>(event);
            if (fe->reason() == Qt::MouseFocusReason) {
                m_selectAllOnMouseUp = true;
                QTimer::singleShot(0, m_edit, &QLineEdit::selectAll);
            }
        } else if (event->type() == QEvent::MouseButtonRelease) {
            if (m_selectAllOnMouseUp) {
                m_selectAllOnMouseUp = false;
                if (!m_edit->hasSelectedText())
                    m_edit->selectAll();
            }
        } else if (event->type() == QEvent::FocusOut) {
            m_selectAllOnMouseUp = false;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void PlatinumLocationBar::openDropdown()
{
    if (m_urls.isEmpty())
        return;

    QMenu menu(this);
    menu.setFont(zacosFont(QStringLiteral("ZacOS 9 System"), 12));
    menu.setMinimumWidth(width());
    const int limit = qMin(m_urls.size(), 20);
    for (int i = 0; i < limit; ++i) {
        const QString &url = m_urls.at(i);
        QAction *act = menu.addAction(url);
        connect(act, &QAction::triggered, this, [this, url]() { pickUrl(url); });
    }
    menu.exec(mapToGlobal(QPoint(0, height())));
}

void PlatinumLocationBar::pickUrl(const QString &url)
{
    m_edit->setText(url);
    emit urlSubmitted(url);
}

ToolbarGrip::ToolbarGrip(const QString &tooltip, QWidget *parent)
    : QWidget(parent)
{
    setFixedWidth(14);
    setCursor(Qt::PointingHandCursor);
    setToolTip(tooltip);
}

void ToolbarGrip::setCollapsed(bool collapsed)
{
    m_collapsed = collapsed;
    update();
}

void ToolbarGrip::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
        emit toggled();
    QWidget::mousePressEvent(event);
}

void ToolbarGrip::paintEvent(QPaintEvent * /*event*/)
{
    QPainter p(this);
    const PlatinumColors c = platinumColors(palette());
    const QRect box = rect().adjusted(2, 2, -2, -2);

    p.fillRect(box, c.button);
    p.setPen(c.light);
    p.drawLine(box.left(), box.top(), box.right(), box.top());
    p.drawLine(box.left(), box.top(), box.left(), box.bottom());
    p.setPen(c.dark);
    p.drawLine(box.right(), box.top(), box.right(), box.bottom());
    p.drawLine(box.left(), box.bottom(), box.right(), box.bottom());

    const int cx = box.center().x();
    const int ty = box.top() + 5;
    p.setBrush(c.text);
    p.setPen(Qt::NoPen);
    if (m_collapsed) {
        p.drawPolygon(QPolygon({QPoint(cx - 2, ty - 2), QPoint(cx + 2, ty + 1), QPoint(cx - 2, ty + 4)}));
    } else {
        p.drawPolygon(QPolygon({QPoint(cx - 3, ty), QPoint(cx + 3, ty), QPoint(cx, ty + 4)}));
    }

    int y = ty + 8;
    while (y < box.bottom() - 3) {
        p.setPen(c.light);
        p.drawPoint(cx - 1, y);
        p.setPen(c.dark);
        p.drawPoint(cx, y + 1);
        y += 3;
    }
}

CollapsedToolbarStrip::CollapsedToolbarStrip(const QString &title, QWidget *parent)
    : QWidget(parent)
    , m_title(title)
{
    setFixedHeight(14);
    setCursor(Qt::PointingHandCursor);
    setToolTip(ztr("Expand %1").arg(title));
    setFont(zacosFont(QStringLiteral("ZacOS 9 Views"), 9));
}

void CollapsedToolbarStrip::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
        emit clicked();
    QWidget::mousePressEvent(event);
}

void CollapsedToolbarStrip::paintEvent(QPaintEvent * /*event*/)
{
    QPainter p(this);
    const PlatinumColors c = platinumColors(palette());
    p.fillRect(rect(), c.window);
    p.setBrush(c.text);
    p.setPen(Qt::NoPen);
    const int cy = height() / 2;
    p.drawPolygon(QPolygon({QPoint(6, cy - 3), QPoint(10, cy), QPoint(6, cy + 3)}));
    p.setFont(font());
    p.setPen(c.text);
    p.drawText(QRect(15, 0, width() - 18, height()), Qt::AlignVCenter | Qt::AlignLeft, m_title);
}

CollapsibleToolbar::CollapsibleToolbar(const QString &title, QWidget *body, QWidget *parent)
    : QWidget(parent)
    , m_title(title)
    , m_body(body)
{
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    m_expandedRow = new QWidget(this);
    auto *rowLay = new QHBoxLayout(m_expandedRow);
    rowLay->setContentsMargins(2, 2, 4, 2);
    rowLay->setSpacing(4);
    m_grip = new ToolbarGrip(ztr("Collapse %1").arg(title), m_expandedRow);
    connect(m_grip, &ToolbarGrip::toggled, this, &CollapsibleToolbar::toggleCollapsed);
    rowLay->addWidget(m_grip);
    m_body->setParent(m_expandedRow);
    rowLay->addWidget(m_body, 1);

    m_collapsedBar = new CollapsedToolbarStrip(title, this);
    connect(m_collapsedBar, &CollapsedToolbarStrip::clicked, this, &CollapsibleToolbar::toggleCollapsed);
    m_collapsedBar->hide();

    outer->addWidget(m_expandedRow);
    outer->addWidget(m_collapsedBar);
}

void CollapsibleToolbar::toggleCollapsed()
{
    setCollapsed(!m_collapsed);
}

void CollapsibleToolbar::setCollapsed(bool collapsed)
{
    m_collapsed = collapsed;
    m_grip->setCollapsed(collapsed);
    m_expandedRow->setVisible(!collapsed);
    m_collapsedBar->setVisible(collapsed);
}

void CollapsibleToolbar::paintEvent(QPaintEvent * /*event*/)
{
    QPainter p(this);
    const PlatinumColors c = platinumColors(palette());
    p.fillRect(rect(), c.window);
    p.setPen(c.light);
    p.drawLine(0, 0, width() - 1, 0);
    p.setPen(c.dark);
    p.drawLine(0, height() - 1, width() - 1, height() - 1);
}

PlatinumViewportFrame::PlatinumViewportFrame(QWidget *child, QWidget *parent)
    : QFrame(parent)
{
    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(2, 2, 2, 2);
    lay->setSpacing(0);
    child->setParent(this);
    lay->addWidget(child);
}

void PlatinumViewportFrame::paintEvent(QPaintEvent * /*event*/)
{
    QPainter p(this);
    const PlatinumColors c = platinumColors(palette());
    const int w = width();
    const int h = height();
    p.fillRect(rect(), c.window);
    p.setPen(c.dark);
    p.drawLine(0, 0, w - 1, 0);
    p.drawLine(0, 0, 0, h - 1);
    p.setPen(c.light);
    p.drawLine(0, h - 1, w - 1, h - 1);
    p.drawLine(w - 1, 0, w - 1, h - 1);
    p.setPen(c.shadow);
    p.drawRect(1, 1, w - 3, h - 3);
}

PlatinumProgressBar::PlatinumProgressBar(QWidget *parent)
    : QWidget(parent)
{
    setFixedSize(116, 14);
    m_indeterminateTimer.setInterval(60);
    connect(&m_indeterminateTimer, &QTimer::timeout, this, &PlatinumProgressBar::advanceStripe);
}

void PlatinumProgressBar::setRange(int minimum, int maximum)
{
    m_min = minimum;
    m_max = qMax(minimum, maximum);
    setValue(m_value);
}

void PlatinumProgressBar::setValue(int value)
{
    const int clamped = qBound(m_min, value, m_max);
    if (clamped != m_value) {
        m_value = clamped;
        update();
    }
}

void PlatinumProgressBar::setIndeterminate(bool indeterminate)
{
    if (m_indeterminate == indeterminate)
        return;
    m_indeterminate = indeterminate;
    if (m_indeterminate) {
        m_indeterminateTimer.start();
    } else {
        m_indeterminateTimer.stop();
        m_stripeOffset = 0;
    }
    update();
}

void PlatinumProgressBar::advanceStripe()
{
    m_stripeOffset = (m_stripeOffset + 1) % 12;
    update();
}

void PlatinumProgressBar::paintEvent(QPaintEvent * /*event*/)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);

    const PlatinumColors c = platinumColors(palette());
    const bool isDark = c.button.lightness() < 128;
    const int w = width();
    const int h = height();
    if (w < 8 || h < 8)
        return;

    // Outer 1px sunken Platinum frame (matches pl_progress_paint)
    p.fillRect(rect(), c.window);
    p.setPen(isDark ? QColor(QStringLiteral("#444444")) : QColor(QStringLiteral("#bbbbbb")));
    p.drawLine(0, 0, w - 2, 0);
    p.drawLine(0, 0, 0, h - 2);
    p.setPen(isDark ? QColor(QStringLiteral("#555555")) : QColor(QStringLiteral("#ffffff")));
    p.drawLine(1, h - 1, w - 1, h - 1);
    p.drawLine(w - 1, 1, w - 1, h - 1);

    // Inner 1px crisp black box
    p.setPen(QColor(QStringLiteral("#000000")));
    p.drawRect(1, 1, w - 3, h - 3);

    const QRect inner(2, 2, w - 4, h - 4);
    const int innerW = inner.width();
    const int innerH = inner.height();
    if (innerW <= 0 || innerH <= 0)
        return;

    // Empty Platinum trough
    const QColor troughBg = isDark ? QColor(QStringLiteral("#2c2c2c")) : QColor(QStringLiteral("#cccccc"));
    const QColor troughHi = isDark ? QColor(QStringLiteral("#3a3a3a")) : QColor(QStringLiteral("#dddddd"));
    const QColor troughLo = isDark ? QColor(QStringLiteral("#1e1e1e")) : QColor(QStringLiteral("#999999"));
    p.fillRect(inner, troughBg);
    p.setPen(troughLo);
    p.drawLine(inner.left(), inner.top(), inner.right(), inner.top());
    p.drawLine(inner.left(), inner.top(), inner.left(), inner.bottom());
    p.setPen(troughHi);
    p.drawLine(inner.left(), inner.bottom(), inner.right(), inner.bottom());
    p.drawLine(inner.right(), inner.top(), inner.right(), inner.bottom());

    // 10-row cylindrical 3D accent ramp derived from palette Highlight
    QColor baseAccent = c.highlight;
    if (!baseAccent.isValid())
        baseAccent = QColor(QStringLiteral("#9999cc"));
    const bool neutral = baseAccent.saturation() < 12;
    const QColor specular = isDark ? baseAccent.lighter(150) : QColor(QStringLiteral("#eeeeee"));

    auto blend = [](const QColor &a, const QColor &b, double t) {
        return QColor(
            int(a.red() + (b.red() - a.red()) * t),
            int(a.green() + (b.green() - a.green()) * t),
            int(a.blue() + (b.blue() - a.blue()) * t));
    };

    QColor ramp[10];
    const QColor midCol = neutral ? QColor(QStringLiteral("#888888")) : baseAccent;
    const QColor darkCol = midCol.darker(155);
    const QColor darkMid = midCol.darker(122);
    const QColor lightCol = midCol.lighter(118);
    const QColor brightCol = blend(midCol, specular, 0.65);

    ramp[0] = darkMid;
    ramp[1] = midCol;
    ramp[2] = lightCol;
    ramp[3] = brightCol;
    ramp[4] = specular;
    ramp[5] = brightCol;
    ramp[6] = lightCol;
    ramp[7] = midCol;
    ramp[8] = darkMid;
    ramp[9] = darkCol;

    if (m_indeterminate) {
        p.save();
        p.setClipRect(inner);
        for (int y = 0; y < innerH; ++y) {
            const QColor stripeLight = specular;
            const QColor stripeDark = ramp[y < 10 ? y : 9];
            for (int x = 0; x < innerW; ++x) {
                const int phase = (x + y + m_stripeOffset) % 12;
                p.setPen(phase < 6 ? stripeDark : stripeLight);
                p.drawPoint(inner.left() + x, inner.top() + y);
            }
        }
        p.restore();
        return;
    }

    const int span = m_max - m_min;
    const int fillW = (span > 0) ? qRound((double(m_value - m_min) / double(span)) * innerW) : 0;
    if (fillW <= 0)
        return;

    const int clampedFill = qMin(fillW, innerW);
    for (int row = 0; row < innerH; ++row) {
        const int rIdx = qBound(0, (row * 10) / qMax(1, innerH), 9);
        p.setPen(ramp[rIdx]);
        p.drawLine(inner.left(), inner.top() + row, inner.left() + clampedFill - 1, inner.top() + row);
    }

    if (clampedFill >= 2) {
        p.setPen(darkCol);
        p.drawLine(
            inner.left() + clampedFill - 1, inner.top(),
            inner.left() + clampedFill - 1, inner.bottom());
    }

    if (clampedFill < innerW) {
        const int sepX = inner.left() + clampedFill;
        p.setPen(QColor(QStringLiteral("#000000")));
        p.drawLine(sepX, inner.top(), sepX, inner.bottom());
        if (sepX + 1 <= inner.right()) {
            p.setPen(isDark ? QColor(QStringLiteral("#141414")) : QColor(QStringLiteral("#666666")));
            p.drawLine(sepX + 1, inner.top(), sepX + 1, inner.bottom());
        }
        if (sepX + 2 <= inner.right()) {
            p.setPen(isDark ? QColor(QStringLiteral("#202020")) : QColor(QStringLiteral("#999999")));
            p.drawLine(sepX + 2, inner.top(), sepX + 2, inner.bottom());
        }
    }
}

ElidingStatusLabel::ElidingStatusLabel(const QString &text, QWidget *parent)
    : QLabel(parent)
    , m_fullText(text)
{
    setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    setMinimumWidth(0);
}

void ElidingStatusLabel::setStatusText(const QString &text)
{
    if (m_fullText == text)
        return;
    m_fullText = text;
    const QString elided = fontMetrics().elidedText(m_fullText, Qt::ElideMiddle, qMax(1, width()));
    setToolTip(elided != m_fullText ? m_fullText : QString());
    update();
}

QSize ElidingStatusLabel::minimumSizeHint() const
{
    return QSize(0, QLabel::minimumSizeHint().height());
}

QSize ElidingStatusLabel::sizeHint() const
{
    return QSize(0, QLabel::sizeHint().height());
}

void ElidingStatusLabel::paintEvent(QPaintEvent * /*event*/)
{
    QPainter p(this);
    p.setFont(font());
    p.setPen(palette().color(QPalette::WindowText));
    const QString elided = fontMetrics().elidedText(m_fullText, Qt::ElideMiddle, qMax(1, width()));
    p.drawText(rect(), Qt::AlignVCenter | Qt::AlignLeft, elided);
}

PlatinumTabBar::PlatinumTabBar(QWidget *parent)
    : QWidget(parent)
    , m_pageIcon(makeMiniIcon(QStringLiteral("page"), 12))
{
    setMouseTracking(true);
    setFont(zacosFont(QStringLiteral("ZacOS 9 System"), 11));
    updateGeometryConstraints();
}

void PlatinumTabBar::setTabStyle(TabStyle style)
{
    if (m_style == style)
        return;
    m_style = style;
    updateGeometryConstraints();
    update();
}

void PlatinumTabBar::updateGeometryConstraints()
{
    if (m_style == TabStyle::IE45Left) {
        setMinimumSize(142, 0);
        setMaximumSize(142, QWIDGETSIZE_MAX);
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    } else {
        setMinimumSize(0, 26);
        setMaximumSize(QWIDGETSIZE_MAX, 26);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }
}

int PlatinumTabBar::addTab(const QString &title)
{
    m_tabs.append(TabItem{title.isEmpty() ? ztr("Untitled") : title, QString(), false});
    const int idx = int(m_tabs.size()) - 1;
    if (m_currentIndex < 0)
        m_currentIndex = idx;
    update();
    return idx;
}

void PlatinumTabBar::removeTab(int index)
{
    if (index < 0 || index >= m_tabs.size())
        return;
    m_tabs.removeAt(index);
    if (m_tabs.isEmpty()) {
        m_currentIndex = -1;
    } else if (m_currentIndex >= m_tabs.size()) {
        m_currentIndex = int(m_tabs.size()) - 1;
    } else if (m_currentIndex > index) {
        --m_currentIndex;
    }
    m_hoveredTab = -1;
    m_hoveredClose = -1;
    update();
}

void PlatinumTabBar::moveTab(int from, int to)
{
    if (from == to || from < 0 || to < 0 || from >= m_tabs.size() || to >= m_tabs.size())
        return;
    m_tabs.move(from, to);
    if (m_currentIndex == from) {
        m_currentIndex = to;
    } else if (from < m_currentIndex && to >= m_currentIndex) {
        --m_currentIndex;
    } else if (from > m_currentIndex && to <= m_currentIndex) {
        ++m_currentIndex;
    }
    m_hoveredTab = -1;
    m_hoveredClose = -1;
    update();
}

void PlatinumTabBar::setCurrentIndex(int index)
{
    if (index < 0 || index >= m_tabs.size() || index == m_currentIndex)
        return;
    m_currentIndex = index;
    update();
}

void PlatinumTabBar::setTabTitle(int index, const QString &title)
{
    if (index < 0 || index >= m_tabs.size())
        return;
    const QString resolved = title.isEmpty() ? ztr("Untitled") : title;
    if (m_tabs[index].title != resolved) {
        m_tabs[index].title = resolved;
        update();
    }
}

void PlatinumTabBar::setTabUrl(int index, const QString &url)
{
    if (index < 0 || index >= m_tabs.size())
        return;
    m_tabs[index].url = url;
}

void PlatinumTabBar::setTabLoading(int index, bool loading)
{
    if (index < 0 || index >= m_tabs.size())
        return;
    if (m_tabs[index].loading != loading) {
        m_tabs[index].loading = loading;
        update();
    }
}

QVector<PlatinumTabBar::TabGeometry> PlatinumTabBar::computeTabGeometries(QRect *outNewTabRect) const
{
    QVector<TabGeometry> geoms;
    const int n = int(m_tabs.size());
    geoms.reserve(n);

    if (m_style == TabStyle::IE45Left) {
        const int tabH = 26;
        const int newBtnH = 22;
        int y = 4;
        for (int i = 0; i < n; ++i) {
            if (y + tabH + newBtnH + 8 > height())
                break;
            QRect tr(4, y, width() - 4, tabH);
            QRect cr(tr.right() - 16, tr.center().y() - 6, 12, 12);
            geoms.append({tr, cr});
            y += tabH + 2;
        }
        if (outNewTabRect)
            *outNewTabRect = QRect(6, qMin(y + 2, qMax(4, height() - newBtnH - 4)), width() - 12, newBtnH);
    } else {
        const int newBtnW = 22;
        const int avail = qMax(40, width() - newBtnW - 12);
        const int tabW = n > 0 ? qBound(64, avail / n, 175) : 120;
        int x = 4;
        for (int i = 0; i < n; ++i) {
            QRect tr(x, 3, tabW, height() - 3);
            QRect cr(tr.right() - 17, tr.center().y() - 5, 12, 12);
            geoms.append({tr, cr});
            x += tabW + 2;
        }
        if (outNewTabRect)
            *outNewTabRect = QRect(x + 2, 5, newBtnW, height() - 8);
    }
    return geoms;
}

int PlatinumTabBar::insertionIndexAt(const QVector<TabGeometry> &geoms, const QPoint &pos) const
{
    for (int i = 0; i < geoms.size(); ++i) {
        const QRect &r = geoms[i].tabRect;
        if (m_style == TabStyle::IE45Left) {
            if (pos.y() < r.center().y())
                return i;
        } else if (pos.x() < r.center().x()) {
            return i;
        }
    }
    return int(geoms.size());
}

void PlatinumTabBar::mousePressEvent(QMouseEvent *event)
{
    QRect newTabRect;
    const QVector<TabGeometry> geoms = computeTabGeometries(&newTabRect);
    const QPoint pos = event->pos();

    m_pressTab = -1;
    m_dragging = false;
    m_dropIndex = -1;

    if (event->button() == Qt::LeftButton && newTabRect.contains(pos)) {
        emit newTabRequested();
        return;
    }

    for (int i = 0; i < geoms.size(); ++i) {
        if (geoms[i].tabRect.contains(pos)) {
            if (event->button() == Qt::MiddleButton
                || (event->button() == Qt::LeftButton && m_tabs.size() > 1 && geoms[i].closeRect.contains(pos))) {
                emit tabCloseRequested(i);
                return;
            }
            if (event->button() == Qt::LeftButton) {
                m_pressTab = i;
                m_pressPos = pos;
                if (i != m_currentIndex) {
                    m_currentIndex = i;
                    update();
                    emit currentChanged(i);
                }
                return;
            }
        }
    }
    QWidget::mousePressEvent(event);
}

void PlatinumTabBar::mouseMoveEvent(QMouseEvent *event)
{
    QRect newTabRect;
    const QVector<TabGeometry> geoms = computeTabGeometries(&newTabRect);
    const QPoint pos = event->pos();

    if (m_pressTab >= 0 && (event->buttons() & Qt::LeftButton)) {
        if (!m_dragging
            && (pos - m_pressPos).manhattanLength() >= QApplication::startDragDistance()) {
            m_dragging = true;
            m_hoveredTab = -1;
            m_hoveredClose = -1;
            m_hoveredNewTab = false;
            setToolTip(QString());
            setCursor(Qt::ClosedHandCursor);
        }
        if (m_dragging) {
            const int drop = insertionIndexAt(geoms, pos);
            if (drop != m_dropIndex) {
                m_dropIndex = drop;
                update();
            }
            return;
        }
    }

    int hovTab = -1;
    int hovClose = -1;
    for (int i = 0; i < geoms.size(); ++i) {
        if (geoms[i].tabRect.contains(pos)) {
            hovTab = i;
            if (m_tabs.size() > 1 && geoms[i].closeRect.contains(pos))
                hovClose = i;
            break;
        }
    }
    const bool hovNew = newTabRect.contains(pos);

    if (hovTab != m_hoveredTab || hovClose != m_hoveredClose || hovNew != m_hoveredNewTab) {
        m_hoveredTab = hovTab;
        m_hoveredClose = hovClose;
        m_hoveredNewTab = hovNew;
        if (hovTab >= 0 && hovTab < m_tabs.size()) {
            const TabItem &t = m_tabs[hovTab];
            setToolTip(t.url.isEmpty() ? t.title : QStringLiteral("%1\n%2").arg(t.title, t.url));
        } else if (hovNew) {
            setToolTip(ztr("New Tab (Ctrl+T)"));
        } else {
            setToolTip(QString());
        }
        update();
    }
    QWidget::mouseMoveEvent(event);
}

void PlatinumTabBar::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_dragging && m_pressTab >= 0) {
        const int from = m_pressTab;
        const int drop = m_dropIndex;
        const QPoint pos = event->pos();
        const QPoint globalPos = mapToGlobal(pos);
        const QRect windowRect = window()->frameGeometry();
        const bool insideWindow = windowRect.contains(globalPos);
        const bool insideBar = rect().contains(pos);

        m_pressTab = -1;
        m_dragging = false;
        m_dropIndex = -1;
        unsetCursor();
        update();

        if (!insideWindow) {
            emit tabDetachRequested(from, globalPos);
            return;
        }
        if (insideBar && drop >= 0) {
            int to = drop;
            if (to > from)
                --to;
            to = qBound(0, to, int(m_tabs.size()) - 1);
            if (to != from)
                emit tabMoveRequested(from, to);
        }
        return;
    }

    m_pressTab = -1;
    m_dragging = false;
    m_dropIndex = -1;
    unsetCursor();
    QWidget::mouseReleaseEvent(event);
}

void PlatinumTabBar::leaveEvent(QEvent *event)
{
    m_hoveredTab = -1;
    m_hoveredClose = -1;
    m_hoveredNewTab = false;
    update();
    QWidget::leaveEvent(event);
}

void PlatinumTabBar::paintEvent(QPaintEvent * /*event*/)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);
    const PlatinumColors c = platinumColors(palette());

    QRect newTabRect;
    const QVector<TabGeometry> geoms = computeTabGeometries(&newTabRect);

    if (m_style == TabStyle::IE45Left) {
        p.fillRect(rect(), c.midlight);
        // Right separator line connecting to viewport
        p.setPen(c.dark);
        p.drawLine(width() - 1, 0, width() - 1, height() - 1);

        for (int i = 0; i < geoms.size(); ++i) {
            const QRect &r = geoms[i].tabRect;
            const QRect &cr = geoms[i].closeRect;
            const bool active = (i == m_currentIndex);
            const bool hovered = (i == m_hoveredTab);
            const bool dragging = m_dragging && (i == m_pressTab);

            p.fillRect(
                r,
                dragging ? c.mid
                         : (active ? c.highlight : (hovered ? c.midlight : c.button)));

            // Classic left-side folder tab bevel
            p.setPen(c.shadow);
            p.drawLine(r.left(), r.top() + 1, r.left(), r.bottom() - 1);
            p.drawLine(r.left() + 1, r.top(), r.right(), r.top());
            p.drawLine(r.left() + 1, r.bottom(), r.right(), r.bottom());

            p.setPen(active ? c.highlight.lighter(130) : c.midlight);
            p.drawLine(r.left() + 1, r.top() + 1, r.right() - (active ? 0 : 1), r.top() + 1);
            p.drawLine(r.left() + 1, r.top() + 1, r.left() + 1, r.bottom() - 1);

            if (!active) {
                p.setPen(c.dark);
                p.drawLine(r.right(), r.top() + 1, r.right(), r.bottom() - 1);
                p.drawLine(r.left() + 1, r.bottom() - 1, r.right(), r.bottom() - 1);
            } else {
                p.fillRect(r.right() - 1, r.top() + 1, 2, r.height() - 2, c.highlight);
            }

            if (!m_pageIcon.isNull())
                p.drawPixmap(r.left() + 5, r.center().y() - m_pageIcon.height() / 2, m_pageIcon);

            const QColor tabTextColor = active ? palette().color(QPalette::HighlightedText) : c.text;
            QFont f = font();
            f.setBold(active);
            p.setFont(f);
            p.setPen(tabTextColor);
            const int textLeft = r.left() + 20;
            const int textRight = (m_tabs.size() > 1) ? cr.left() - 3 : r.right() - 4;
            const QString elided = p.fontMetrics().elidedText(
                m_tabs[i].title, Qt::ElideRight, qMax(10, textRight - textLeft));
            p.drawText(
                QRect(textLeft, r.top(), qMax(10, textRight - textLeft), r.height()),
                Qt::AlignVCenter | Qt::AlignLeft,
                elided);

            if (m_tabs.size() > 1) {
                if (m_hoveredClose == i) {
                    p.fillRect(cr, c.mid);
                    p.setPen(c.dark);
                    p.drawRect(cr.adjusted(0, 0, -1, -1));
                    p.setPen(c.text);
                } else {
                    p.setPen(tabTextColor);
                }
                p.drawLine(cr.left() + 3, cr.top() + 3, cr.right() - 3, cr.bottom() - 3);
                p.drawLine(cr.right() - 3, cr.top() + 3, cr.left() + 3, cr.bottom() - 3);
            }
        }

        if (m_dragging && m_dropIndex >= 0 && !geoms.isEmpty()) {
            const int y = (m_dropIndex < geoms.size())
                ? geoms[m_dropIndex].tabRect.top() - 1
                : geoms.last().tabRect.bottom() + 1;
            p.setPen(QPen(c.dark, 2));
            p.drawLine(4, y, width() - 6, y);
        }

        // "+ New Tab" Platinum bevel button
        drawPlatinumBevelButton(p, newTabRect, palette(), true, false, m_hoveredNewTab);
        p.setFont(font());
        p.setPen(c.text);
        p.drawText(newTabRect, Qt::AlignCenter, QStringLiteral("+ ") + ztr("New Tab"));
    } else {
        p.fillRect(rect(), c.mid);
        p.setPen(c.dark);
        p.drawLine(0, height() - 1, width() - 1, height() - 1);

        for (int i = 0; i < geoms.size(); ++i) {
            const QRect &r = geoms[i].tabRect;
            const QRect &cr = geoms[i].closeRect;
            const bool active = (i == m_currentIndex);
            const bool hovered = (i == m_hoveredTab);
            const bool dragging = m_dragging && (i == m_pressTab);

            p.fillRect(
                r,
                dragging ? c.mid
                         : (active ? c.highlight : (hovered ? c.midlight : c.button)));

            p.setPen(c.shadow);
            p.drawLine(r.left(), r.top() + 1, r.left(), r.bottom());
            p.drawLine(r.left() + 1, r.top(), r.right() - 1, r.top());
            p.drawLine(r.right(), r.top() + 1, r.right(), r.bottom());

            p.setPen(active ? c.highlight.lighter(130) : c.midlight);
            p.drawLine(r.left() + 1, r.top() + 1, r.right() - 1, r.top() + 1);
            p.drawLine(r.left() + 1, r.top() + 1, r.left() + 1, r.bottom() - 1);

            p.setPen(active ? c.highlight.darker(130) : c.dark);
            p.drawLine(r.right() - 1, r.top() + 1, r.right() - 1, r.bottom() - 1);

            if (active) {
                p.fillRect(r.left() + 1, r.bottom(), r.width() - 2, 1, c.highlight);
            }

            if (!m_pageIcon.isNull())
                p.drawPixmap(r.left() + 5, r.center().y() - m_pageIcon.height() / 2, m_pageIcon);

            const QColor tabTextColor = active ? palette().color(QPalette::HighlightedText) : c.text;
            QFont f = font();
            f.setBold(active);
            p.setFont(f);
            p.setPen(tabTextColor);
            const int textLeft = r.left() + 20;
            const int textRight = (m_tabs.size() > 1) ? cr.left() - 3 : r.right() - 4;
            const QString elided = p.fontMetrics().elidedText(
                m_tabs[i].title, Qt::ElideRight, qMax(10, textRight - textLeft));
            p.drawText(
                QRect(textLeft, r.top(), qMax(10, textRight - textLeft), r.height()),
                Qt::AlignVCenter | Qt::AlignLeft,
                elided);

            if (m_tabs.size() > 1) {
                if (m_hoveredClose == i) {
                    p.fillRect(cr, c.mid);
                    p.setPen(c.dark);
                    p.drawRect(cr.adjusted(0, 0, -1, -1));
                    p.setPen(c.text);
                } else {
                    p.setPen(tabTextColor);
                }
                p.drawLine(cr.left() + 3, cr.top() + 3, cr.right() - 3, cr.bottom() - 3);
                p.drawLine(cr.right() - 3, cr.top() + 3, cr.left() + 3, cr.bottom() - 3);
            }
        }

        if (m_dragging && m_dropIndex >= 0 && !geoms.isEmpty()) {
            const int x = (m_dropIndex < geoms.size())
                ? geoms[m_dropIndex].tabRect.left() - 1
                : geoms.last().tabRect.right() + 1;
            p.setPen(QPen(c.text, 2));
            p.drawLine(x, 2, x, height() - 2);
        }

        // "+" button
        drawPlatinumBevelButton(p, newTabRect, palette(), true, false, m_hoveredNewTab);
        p.setFont(font());
        p.setPen(c.text);
        p.drawText(newTabRect, Qt::AlignCenter, QStringLiteral("+"));
    }
}
