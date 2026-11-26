#pragma once

#include <functional>

#include <QFrame>
#include <QLabel>
#include <QPixmap>
#include <QPoint>
#include <QPushButton>
#include <QStringList>
#include <QTimer>
#include <QVariant>
#include <QVector>
#include <QWidget>

class QEnterEvent;
class QLineEdit;
class QMenu;
class QMouseEvent;
class QPaintEvent;

class PlatinumNavButton : public QPushButton {
    Q_OBJECT

public:
    using HistoryMenuProvider = std::function<void(QMenu &)>;

    PlatinumNavButton(const QString &label, const QString &iconName, QWidget *parent = nullptr);

    void setHistoryMenuProvider(HistoryMenuProvider provider);
    void setIconName(const QString &iconName);
    void setShowText(bool show);
    bool showText() const { return m_showText; }

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private slots:
    void onHoldTimeout();

private:
    void showHistoryPopup();

    QString m_label;
    bool m_hovered = false;
    bool m_showText = true;
    QPixmap m_iconPm;
    QPixmap m_disabledPm;
    HistoryMenuProvider m_historyMenuProvider;
    bool m_holdTriggered = false;
    QTimer m_holdTimer;
};

class PlatinumCompactButton : public QPushButton {
    Q_OBJECT

public:
    PlatinumCompactButton(
        const QString &label,
        const QString &iconName,
        bool hasArrow = false,
        QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QString m_label;
    QPixmap m_iconPm;
    bool m_hasArrow = false;
};

class PlatinumMiniIconButton : public QPushButton {
    Q_OBJECT

public:
    PlatinumMiniIconButton(const QString &iconName, const QString &tooltip, QWidget *parent = nullptr);

    void setIconName(const QString &iconName);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QPixmap m_iconPm;
};

class NetzacThrobber : public QPushButton {
    Q_OBJECT

public:
    explicit NetzacThrobber(QWidget *parent = nullptr);

    void setLoading(bool loading);
    void setCompact(bool compact);

protected:
    void paintEvent(QPaintEvent *event) override;

private slots:
    void advanceFrame();

private:
    bool m_animating = false;
    bool m_compact = false;
    int m_frame = 0;
    QTimer m_timer;
};

class PlatinumDropArrowButton : public QPushButton {
    Q_OBJECT

public:
    explicit PlatinumDropArrowButton(QWidget *parent = nullptr);

protected:
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    bool m_hovered = false;
};

class PlatinumPopupButton : public QPushButton {
    Q_OBJECT

public:
    explicit PlatinumPopupButton(QWidget *parent = nullptr);

    void addItem(const QString &label, const QVariant &data);
    void setCurrentData(const QVariant &data);
    void setCurrentIndex(int index);
    QVariant currentData() const;
    int currentIndex() const { return m_currentIndex; }

signals:
    void currentIndexChanged(int index);

protected:
    void paintEvent(QPaintEvent *event) override;

private slots:
    void showPopup();

private:
    struct Item {
        QString label;
        QVariant data;
    };

    QVector<Item> m_items;
    int m_currentIndex = -1;
};

class PlatinumLocationBar : public QWidget {
    Q_OBJECT

public:
    explicit PlatinumLocationBar(const QStringList &initialUrls, QWidget *parent = nullptr);

    void setText(const QString &text);
    QString text() const;
    void recordUrl(const QString &url);
    void setReadOnly(bool readOnly);
    bool isReadOnly() const;

signals:
    void urlSubmitted(const QString &url);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void openDropdown();

private:
    void pickUrl(const QString &url);

    QStringList m_urls;
    QLineEdit *m_edit = nullptr;
    PlatinumDropArrowButton *m_dropBtn = nullptr;
    bool m_selectAllOnMouseUp = false;
};

class ToolbarGrip : public QWidget {
    Q_OBJECT

public:
    explicit ToolbarGrip(const QString &tooltip, QWidget *parent = nullptr);

    void setCollapsed(bool collapsed);

signals:
    void toggled();

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    bool m_collapsed = false;
};

class CollapsedToolbarStrip : public QWidget {
    Q_OBJECT

public:
    explicit CollapsedToolbarStrip(const QString &title, QWidget *parent = nullptr);

signals:
    void clicked();

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    QString m_title;
};

class CollapsibleToolbar : public QWidget {
    Q_OBJECT

public:
    CollapsibleToolbar(const QString &title, QWidget *body, QWidget *parent = nullptr);

public slots:
    void toggleCollapsed();
    void setCollapsed(bool collapsed);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QString m_title;
    QWidget *m_body = nullptr;
    QWidget *m_expandedRow = nullptr;
    ToolbarGrip *m_grip = nullptr;
    CollapsedToolbarStrip *m_collapsedBar = nullptr;
    bool m_collapsed = false;
};

class PlatinumViewportFrame : public QFrame {
    Q_OBJECT

public:
    explicit PlatinumViewportFrame(QWidget *child, QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *event) override;
};

class PlatinumProgressBar : public QWidget {
    Q_OBJECT

public:
    explicit PlatinumProgressBar(QWidget *parent = nullptr);

    void setRange(int minimum, int maximum);
    void setValue(int value);
    void setIndeterminate(bool indeterminate);
    int value() const { return m_value; }

protected:
    void paintEvent(QPaintEvent *event) override;

private slots:
    void advanceStripe();

private:
    int m_min = 0;
    int m_max = 100;
    int m_value = 0;
    bool m_indeterminate = false;
    int m_stripeOffset = 0;
    QTimer m_indeterminateTimer;
};

class ElidingStatusLabel : public QLabel {
    Q_OBJECT

public:
    explicit ElidingStatusLabel(const QString &text = QString(), QWidget *parent = nullptr);

    void setStatusText(const QString &text);
    QSize minimumSizeHint() const override;
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QString m_fullText;
};

class PlatinumTabBar : public QWidget {
    Q_OBJECT

public:
    enum class TabStyle {
        IE45Left,
        ChromeTop,
    };

    explicit PlatinumTabBar(QWidget *parent = nullptr);

    void setTabStyle(TabStyle style);
    TabStyle tabStyle() const { return m_style; }

    int addTab(const QString &title);
    void removeTab(int index);
    void moveTab(int from, int to);
    void setCurrentIndex(int index);
    int currentIndex() const { return m_currentIndex; }
    int count() const { return int(m_tabs.size()); }

    void setTabTitle(int index, const QString &title);
    void setTabUrl(int index, const QString &url);
    void setTabLoading(int index, bool loading);

signals:
    void currentChanged(int index);
    void tabCloseRequested(int index);
    void newTabRequested();
    void tabMoveRequested(int from, int to);
    void tabDetachRequested(int index, const QPoint &globalPos);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    struct TabItem {
        QString title;
        QString url;
        bool loading = false;
    };

    struct TabGeometry {
        QRect tabRect;
        QRect closeRect;
    };

    QVector<TabGeometry> computeTabGeometries(QRect *outNewTabRect) const;
    int insertionIndexAt(const QVector<TabGeometry> &geoms, const QPoint &pos) const;
    void updateGeometryConstraints();

    TabStyle m_style = TabStyle::IE45Left;
    QVector<TabItem> m_tabs;
    int m_currentIndex = -1;
    int m_hoveredTab = -1;
    int m_hoveredClose = -1;
    bool m_hoveredNewTab = false;
    QPixmap m_pageIcon;

    int m_pressTab = -1;
    QPoint m_pressPos;
    bool m_dragging = false;
    int m_dropIndex = -1;
};
