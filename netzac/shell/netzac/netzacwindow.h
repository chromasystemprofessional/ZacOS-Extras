#pragma once

#include "calmmode.h"
#include "history.h"
#include "preferences.h"

#include <QMainWindow>
#include <QPair>
#include <QPoint>
#include <QRect>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QVector>
#include <QWebEnginePage>

class QAction;
class QCloseEvent;
class QHBoxLayout;
class QLabel;
class QLineEdit;
class QMenu;
class QStackedWidget;
class QStatusBar;
class QVBoxLayout;
class QWebEngineContextMenuRequest;
class QWebEngineView;

class CollapsibleToolbar;
class ElidingStatusLabel;
class NetzacThrobber;
class NetzacWebPage;
class NetzacWebView;
class PlatinumCompactButton;
class PlatinumLocationBar;
class PlatinumMiniIconButton;
class PlatinumNavButton;
class PlatinumProgressBar;
class PlatinumTabBar;
class PlatinumViewportFrame;

class NetzacWindow : public QMainWindow {
    Q_OBJECT

public:
    struct PopupOptions {
        bool isPopup = false;
        bool hideToolbar = false;
        bool hideScrollbars = false;
        bool nonResizable = false;
        QRect geometry;
    };

    explicit NetzacWindow(
        const QString &initialUrl = QString(),
        QWidget *parent = nullptr,
        bool loadInitial = true);
    NetzacWindow(
        const QString &initialUrl,
        QWidget *parent,
        bool loadInitial,
        const PopupOptions &popupOpts);

    NetzacWebView *webView() const;
    NetzacWebPage *webPage() const;
    CalmModeController *calmController() const;
    void setCalmModeEnabled(bool enabled);

public slots:
    void navigateTo(const QString &url);
    int newTab(const QString &url = QString(), bool switchTo = true);
    void closeTab(int index);
    void switchToTab(int index);

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void populateGoMenu();
    void populateBookmarksMenu(QMenu *targetMenu = nullptr);

    void onTabUrlChanged(NetzacWebView *view, const QUrl &qurl);
    void onTabTitleChanged(NetzacWebView *view, const QString &title);
    void onTabLoadStarted(NetzacWebView *view);
    void onTabLoadProgress(NetzacWebView *view, int progress);
    void onTabLoadFinished(NetzacWebView *view, bool ok);
    void onTabLinkHovered(NetzacWebView *view, const QString &url);
    void onCalmEnabledChanged(NetzacWebView *view, bool enabled);
    void onCalmEvaluationFinished(NetzacWebView *view, const CalmModeController::EvaluationReport &report);
    void showWebContextMenu(const QPoint &globalPos, QWebEngineContextMenuRequest *request);

    void showBookmarksPopup();
    void addCurrentBookmark();
    void manageBookmarks();
    void newWindow();
    void openLocationDialog();
    void savePage();
    void printPage();
    void showFindBar();
    void hideFindBar();
    void findNext();
    void toggleFullScreen();
    void zoomIn();
    void zoomOut();
    void resetZoom();
    void promptDefaultBrowserIfNeeded();
    void viewPageSource();
    void showPageInfo();
    void showHistoryDialog();
    void openPreferences();
    void showAbout();

private:
    struct BrowserTab {
        NetzacWebView *view = nullptr;
        NetzacWebPage *page = nullptr;
        CalmModeController *calm = nullptr;
        QString currentUrl;
        QString title;
        bool showingBuiltin = false;
        bool isLoading = false;
        int loadProgress = 0;
    };

    int createTabInternal(const QString &url, bool switchTo, bool loadInitial);
    void moveTab(int from, int to);
    void detachTab(int index, const QPoint &globalPos);
    void adoptTab(const BrowserTab &tab);
    void clearTabsForAdoption();
    int indexOfView(const NetzacWebView *view) const;
    BrowserTab *currentTab();
    const BrowserTab *currentTab() const;
    void syncUiToCurrentTab();
    void updateTabBarPlacement();
    void applyPopupRestrictions();

    void buildMenus();
    void buildToolbars();
    void buildCentralArea();
    void rebuildPersonalToolbar();
    void buildStatusBar();
    void connectTabSignals(NetzacWebView *view, NetzacWebPage *page, CalmModeController *calm);
    void updateNavStates();
    void loadBuiltinAbout(BrowserTab *tab, const QString &aboutUrl);
    void buildNavHistoryMenu(QMenu &menu, bool back);
    void applyNavToolbarCompact(bool showText);
    void handleCredentialSubmitted(const QString &host, const QString &username, const QString &password);
    void installPasswordHelpers(NetzacWebPage *page, const QUrl &url);
    void writeSavedHtml(const QString &path, const QString &htmlText);
    QWebEngineView *createBrowserWindow(NetzacWebView *opener, QWebEnginePage::WebWindowType type);

    QAction *addMenuAction(QMenu *menu, const QString &text, const QKeySequence &shortcut = QKeySequence());
    QAction *addCheckableAction(QMenu *menu, const QString &text, bool checked);

    NetzacPreferences m_prefs;
    PopupOptions m_popupOpts;
    QVector<QPair<QString, QString>> m_bookmarks;
    QVector<HistoryEntry> m_history;
    QStringList m_visitedUrls;
    QString m_lastSearch;

    QVector<BrowserTab> m_tabs;
    int m_currentTabIndex = -1;
    QStackedWidget *m_tabStack = nullptr;
    PlatinumTabBar *m_tabBar = nullptr;
    QVBoxLayout *m_rootLayout = nullptr;
    QHBoxLayout *m_workspaceLayout = nullptr;

    QMenu *m_goMenu = nullptr;
    QMenu *m_bookmarksMenu = nullptr;
    QAction *m_actShowNav = nullptr;
    QAction *m_actShowNavText = nullptr;
    QAction *m_actShowLoc = nullptr;
    QAction *m_actShowPers = nullptr;
    QAction *m_actShowStatus = nullptr;
    bool m_preFsShowPers = true;
    bool m_preFsShowNavText = true;
    QAction *m_actCalmMode = nullptr;
    QAction *m_actStopMenu = nullptr;
    QAction *m_actFullScreen = nullptr;
    QAction *m_actNewTab = nullptr;
    QAction *m_actCloseTab = nullptr;
    QAction *m_actLinkModeTabs = nullptr;
    QAction *m_actLinkModeWindows = nullptr;
    QAction *m_actTabStyleLeft = nullptr;
    QAction *m_actTabStyleTop = nullptr;

    PlatinumNavButton *m_btnBack = nullptr;
    PlatinumNavButton *m_btnForward = nullptr;
    PlatinumNavButton *m_btnHome = nullptr;
    PlatinumNavButton *m_btnReload = nullptr;
    PlatinumNavButton *m_btnPrint = nullptr;
    PlatinumNavButton *m_btnFind = nullptr;
    PlatinumNavButton *m_btnStop = nullptr;
    PlatinumNavButton *m_btnCalm = nullptr;
    NetzacThrobber *m_throbber = nullptr;

    PlatinumCompactButton *m_btnBookmarks = nullptr;
    QLabel *m_locationLabel = nullptr;
    PlatinumMiniIconButton *m_pageProxy = nullptr;
    PlatinumLocationBar *m_urlBar = nullptr;

    CollapsibleToolbar *m_navToolbar = nullptr;
    CollapsibleToolbar *m_locToolbar = nullptr;
    CollapsibleToolbar *m_persToolbar = nullptr;
    QWidget *m_persBody = nullptr;
    QHBoxLayout *m_persLay = nullptr;

    QWidget *m_findBar = nullptr;
    QLineEdit *m_findInput = nullptr;
    PlatinumViewportFrame *m_viewportFrame = nullptr;

    QStatusBar *m_status = nullptr;
    PlatinumMiniIconButton *m_statusSecurity = nullptr;
    PlatinumProgressBar *m_progressBar = nullptr;
    ElidingStatusLabel *m_statusLabel = nullptr;
};
