#include "netzacwindow.h"

#include "bookmarks.h"
#include "dialogs.h"
#include "i18n.h"
#include "platinum.h"
#include "webview.h"
#include "widgets.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QColor>
#include <QDateTime>
#include <QFile>
#include <QFont>
#include <QHBoxLayout>
#include <QIcon>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTextStream>
#include <QTimer>
#include <QVBoxLayout>
#include <QWebEngineContextMenuRequest>
#include <QWebEngineHistory>
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QWebEngineScript>
#include <QWebEngineSettings>

namespace {

QString retroHtmlPage(const QString &title, const QString &subtitle, const QString &bodyHtml)
{
    const QString footer = ztr("Netzac Navigator 1.0 &mdash; ZacOS 9 Platinum Edition");
    return QStringLiteral(
        "<!DOCTYPE html>\n"
        "<html>\n"
        "<head>\n"
        "<meta charset=\"utf-8\">\n"
        "<title>%1</title>\n"
        "<style>\n"
        "  * { box-sizing: border-box; }\n"
        "  body {\n"
        "    margin: 0;\n"
        "    padding: 28px 20px 36px 20px;\n"
        "    background-color: #e8e8e8;\n"
        "    color: #000000;\n"
        "    font-family: 'ZacOS 9 System', sans-serif;\n"
        "    font-size: 13px;\n"
        "    line-height: 1.5;\n"
        "  }\n"
        "  .page {\n"
        "    max-width: 700px;\n"
        "    margin: 0 auto;\n"
        "  }\n"
        "  h1 {\n"
        "    margin: 0 0 4px 0;\n"
        "    font-size: 20px;\n"
        "    font-weight: bold;\n"
        "    color: #005a5a;\n"
        "  }\n"
        "  .subtitle {\n"
        "    margin: 0 0 14px 0;\n"
        "    color: #444444;\n"
        "    font-size: 12px;\n"
        "  }\n"
        "  hr {\n"
        "    border: none;\n"
        "    border-top: 1px solid #b0b0b0;\n"
        "    margin: 14px 0;\n"
        "  }\n"
        "  .shortcut-list {\n"
        "    list-style: none;\n"
        "    padding: 0;\n"
        "    margin: 8px 0;\n"
        "  }\n"
        "  .shortcut-list li {\n"
        "    display: flex;\n"
        "    justify-content: space-between;\n"
        "    align-items: center;\n"
        "    padding: 6px 4px;\n"
        "    border-bottom: 1px solid #d0d0d0;\n"
        "  }\n"
        "  .shortcut-list li:last-child { border-bottom: none; }\n"
        "  a { color: #0000ee; }\n"
        "  a:visited { color: #551a8b; }\n"
        "  code {\n"
        "    font-family: monospace;\n"
        "    font-size: 12px;\n"
        "    background: #dcdcdc;\n"
        "    padding: 1px 5px;\n"
        "    border: 1px solid #999999;\n"
        "  }\n"
        "  .footer {\n"
        "    margin-top: 18px;\n"
        "    font-size: 11px;\n"
        "    color: #555555;\n"
        "  }\n"
        "</style>\n"
        "</head>\n"
        "<body>\n"
        "  <div class=\"page\">\n"
        "    <h1>%1</h1>\n"
        "    <p class=\"subtitle\">%2</p>\n"
        "    <hr>\n"
        "    %3\n"
        "    <hr>\n"
        "    <div class=\"footer\">%4</div>\n"
        "  </div>\n"
        "</body>\n"
        "</html>")
        .arg(title.toHtmlEscaped(), subtitle.toHtmlEscaped(), bodyHtml, footer);
}

QString navigatorStartHtmlPage(const QString &title, const QString &searchPlaceholder, const QString &searchBtnText)
{
    return QStringLiteral(
        "<!DOCTYPE html>\n"
        "<html>\n"
        "<head>\n"
        "<meta charset=\"utf-8\">\n"
        "<title>%1</title>\n"
        "<style>\n"
        "  * { box-sizing: border-box; }\n"
        "  body {\n"
        "    margin: 0;\n"
        "    padding: 0 20px;\n"
        "    min-height: 85vh;\n"
        "    display: flex;\n"
        "    flex-direction: column;\n"
        "    align-items: center;\n"
        "    justify-content: center;\n"
        "    background-color: #e8e8e8;\n"
        "    color: #000000;\n"
        "    font-family: 'ZacOS 9 System', sans-serif;\n"
        "    font-size: 13px;\n"
        "  }\n"
        "  .brand {\n"
        "    margin: 0 0 22px 0;\n"
        "    font-size: 44px;\n"
        "    font-weight: bold;\n"
        "    color: #005a5a;\n"
        "    letter-spacing: 1px;\n"
        "    text-align: center;\n"
        "  }\n"
        "  .search-form {\n"
        "    display: flex;\n"
        "    gap: 8px;\n"
        "    width: 100%;\n"
        "    max-width: 460px;\n"
        "  }\n"
        "  .search-input {\n"
        "    flex: 1;\n"
        "    height: 30px;\n"
        "    padding: 4px 8px;\n"
        "    font-family: inherit;\n"
        "    font-size: 13px;\n"
        "    color: #000000;\n"
        "    background-color: #ffffff;\n"
        "    border: 1px solid #004444;\n"
        "  }\n"
        "  .search-btn {\n"
        "    height: 30px;\n"
        "    padding: 0 16px;\n"
        "    font-family: inherit;\n"
        "    font-size: 13px;\n"
        "    font-weight: bold;\n"
        "    color: #ffffff;\n"
        "    background-color: #006666;\n"
        "    border: 1px solid #003b3b;\n"
        "    cursor: pointer;\n"
        "  }\n"
        "  .search-btn:hover {\n"
        "    background-color: #007a7a;\n"
        "  }\n"
        "</style>\n"
        "</head>\n"
        "<body>\n"
        "  <h1 class=\"brand\">NetZac</h1>\n"
        "  <form class=\"search-form\" action=\"https://duckduckgo.com/\" method=\"get\">\n"
        "    <input class=\"search-input\" type=\"text\" name=\"q\" placeholder=\"%2\" autofocus>\n"
        "    <button class=\"search-btn\" type=\"submit\">%3</button>\n"
        "  </form>\n"
        "</body>\n"
        "</html>")
        .arg(title.toHtmlEscaped(), searchPlaceholder.toHtmlEscaped(), searchBtnText.toHtmlEscaped());
}

NetzacWindow::PopupOptions parsePopupFeatures(
    const QString &features,
    QWebEnginePage::WebWindowType type)
{
    NetzacWindow::PopupOptions opts;
    opts.isPopup = (type == QWebEnginePage::WebDialog);

    int w = -1;
    int h = -1;
    int x = -1;
    int y = -1;
    const QStringList parts = features.split(QRegularExpression(QStringLiteral("[,\\s]+")), Qt::SkipEmptyParts);
    for (const QString &part : parts) {
        const int eq = part.indexOf(QLatin1Char('='));
        const QString key = (eq >= 0 ? part.left(eq) : part).trimmed().toLower();
        const QString val = (eq >= 0 ? part.mid(eq + 1) : QStringLiteral("yes")).trimmed().toLower();
        const bool isNo = (val == QLatin1String("no") || val == QLatin1String("0") || val == QLatin1String("false"));

        if (key == QLatin1String("toolbar") || key == QLatin1String("menubar")) {
            if (isNo) {
                opts.hideToolbar = true;
                opts.isPopup = true;
            }
        } else if (key == QLatin1String("scrollbars")) {
            if (isNo) {
                opts.hideScrollbars = true;
                opts.isPopup = true;
            }
        } else if (key == QLatin1String("resizable")) {
            if (isNo) {
                opts.nonResizable = true;
                opts.isPopup = true;
            }
        } else if (key == QLatin1String("location") || key == QLatin1String("status") || key == QLatin1String("directories")) {
            if (isNo)
                opts.isPopup = true;
        } else if (key == QLatin1String("width") || key == QLatin1String("innerwidth")) {
            bool ok = false;
            const int parsed = val.toInt(&ok);
            if (ok && parsed > 0) {
                w = parsed;
                opts.isPopup = true;
            }
        } else if (key == QLatin1String("height") || key == QLatin1String("innerheight")) {
            bool ok = false;
            const int parsed = val.toInt(&ok);
            if (ok && parsed > 0) {
                h = parsed;
                opts.isPopup = true;
            }
        } else if (key == QLatin1String("left") || key == QLatin1String("screenx")) {
            bool ok = false;
            const int parsed = val.toInt(&ok);
            if (ok)
                x = parsed;
        } else if (key == QLatin1String("top") || key == QLatin1String("screeny")) {
            bool ok = false;
            const int parsed = val.toInt(&ok);
            if (ok)
                y = parsed;
        }
    }

    if (opts.isPopup && !opts.hideToolbar && type == QWebEnginePage::WebDialog)
        opts.hideToolbar = true;

    if (w > 0 || h > 0)
        opts.geometry = QRect(x >= 0 ? x : 0, y >= 0 ? y : 0, w > 0 ? w : 780, h > 0 ? h : 560);

    return opts;
}

}  // namespace

NetzacWindow::NetzacWindow(
    const QString &initialUrl,
    QWidget *parent,
    bool loadInitial)
    : NetzacWindow(initialUrl, parent, loadInitial, PopupOptions())
{
}

NetzacWindow::NetzacWindow(
    const QString &initialUrl,
    QWidget *parent,
    bool loadInitial,
    const PopupOptions &popupOpts)
    : QMainWindow(parent)
    , m_prefs(NetzacPreferences::load())
    , m_popupOpts(popupOpts)
{
    setWindowTitle(ztr("Netzac"));
    setMinimumSize(m_popupOpts.isPopup ? QSize(240, 180) : QSize(640, 400));
    resize(m_popupOpts.isPopup ? QSize(780, 560) : QSize(1024, 768));

    m_bookmarks = BookmarkStorage::load();
    m_history = HistoryStorage::load();
    for (const HistoryEntry &h : m_history) {
        if (!m_visitedUrls.contains(h.url))
            m_visitedUrls.append(h.url);
    }
    for (const auto &entry : m_bookmarks) {
        if (!m_visitedUrls.contains(entry.second))
            m_visitedUrls.append(entry.second);
    }

    m_tabStack = new QStackedWidget(this);
    m_tabBar = new PlatinumTabBar(this);
    connect(m_tabBar, &PlatinumTabBar::currentChanged, this, &NetzacWindow::switchToTab);
    connect(m_tabBar, &PlatinumTabBar::tabCloseRequested, this, &NetzacWindow::closeTab);
    connect(m_tabBar, &PlatinumTabBar::newTabRequested, this, [this]() {
        newTab(m_prefs.homeUrl, true);
    });
    connect(m_tabBar, &PlatinumTabBar::tabMoveRequested, this, &NetzacWindow::moveTab);
    connect(m_tabBar, &PlatinumTabBar::tabDetachRequested, this, &NetzacWindow::detachTab);

    buildMenus();
    buildToolbars();
    buildCentralArea();
    buildStatusBar();

    if (loadInitial && initialUrl.isEmpty() && !m_popupOpts.isPopup
        && m_prefs.startupMode == NetzacPreferences::StartupMode::RestoreLastPage
        && !m_prefs.lastTabs.isEmpty()) {
        for (int i = 0; i < m_prefs.lastTabs.size(); ++i)
            createTabInternal(m_prefs.lastTabs.at(i), i == 0, true);
        const int targetTab = qBound(0, m_prefs.lastActiveTab, int(m_tabs.size()) - 1);
        if (targetTab != m_currentTabIndex)
            switchToTab(targetTab);
    } else {
        QString startTarget = initialUrl;
        if (loadInitial && startTarget.isEmpty()) {
            if (m_prefs.startupMode == NetzacPreferences::StartupMode::RestoreLastPage
                && !m_prefs.lastUrl.isEmpty()) {
                startTarget = m_prefs.lastUrl;
            } else {
                startTarget = m_prefs.homeUrl;
            }
        }
        createTabInternal(startTarget, true, loadInitial);
    }
    updateTabBarPlacement();
    applyPopupRestrictions();

    if (!m_popupOpts.isPopup && !m_prefs.defaultBrowserPrompted) {
        QTimer::singleShot(350, this, &NetzacWindow::promptDefaultBrowserIfNeeded);
    }
}

NetzacWebView *NetzacWindow::webView() const
{
    const BrowserTab *tab = currentTab();
    return tab ? tab->view : nullptr;
}

NetzacWebPage *NetzacWindow::webPage() const
{
    const BrowserTab *tab = currentTab();
    return tab ? tab->page : nullptr;
}

CalmModeController *NetzacWindow::calmController() const
{
    const BrowserTab *tab = currentTab();
    return tab ? tab->calm : nullptr;
}

NetzacWindow::BrowserTab *NetzacWindow::currentTab()
{
    if (m_currentTabIndex < 0 || m_currentTabIndex >= m_tabs.size())
        return nullptr;
    return &m_tabs[m_currentTabIndex];
}

const NetzacWindow::BrowserTab *NetzacWindow::currentTab() const
{
    if (m_currentTabIndex < 0 || m_currentTabIndex >= m_tabs.size())
        return nullptr;
    return &m_tabs[m_currentTabIndex];
}

int NetzacWindow::indexOfView(const NetzacWebView *view) const
{
    for (int i = 0; i < m_tabs.size(); ++i) {
        if (m_tabs[i].view == view)
            return i;
    }
    return -1;
}

void NetzacWindow::closeEvent(QCloseEvent *event)
{
    HistoryStorage::save(m_history);
    if (!m_popupOpts.isPopup) {
        QStringList tabUrls;
        tabUrls.reserve(m_tabs.size());
        for (const BrowserTab &tab : m_tabs)
            tabUrls.append(tab.currentUrl);
        NetzacPreferences::saveLastTabs(tabUrls, m_currentTabIndex);
    }
    QMainWindow::closeEvent(event);
}

QAction *NetzacWindow::addMenuAction(QMenu *menu, const QString &text, const QKeySequence &shortcut)
{
    auto *act = new QAction(text, this);
    if (!shortcut.isEmpty())
        act->setShortcut(shortcut);
    menu->addAction(act);
    return act;
}

QAction *NetzacWindow::addCheckableAction(QMenu *menu, const QString &text, bool checked)
{
    auto *act = new QAction(text, this);
    act->setCheckable(true);
    act->setChecked(checked);
    menu->addAction(act);
    return act;
}

void NetzacWindow::buildMenus()
{
    QMenuBar *mb = menuBar();

    // File
    QMenu *fileMenu = mb->addMenu(ztr("File"));
    connect(addMenuAction(fileMenu, ztr("New Navigator Window"), QKeySequence::New),
            &QAction::triggered, this, &NetzacWindow::newWindow);
    m_actNewTab = addMenuAction(fileMenu, ztr("New Tab"), QKeySequence(QStringLiteral("Ctrl+T")));
    connect(m_actNewTab, &QAction::triggered, this, [this]() {
        newTab(m_prefs.homeUrl, true);
    });
    m_actCloseTab = addMenuAction(fileMenu, ztr("Close Tab"), QKeySequence(QStringLiteral("Ctrl+W")));
    connect(m_actCloseTab, &QAction::triggered, this, [this]() {
        if (m_tabs.size() > 1)
            closeTab(m_currentTabIndex);
        else
            close();
    });
    connect(addMenuAction(fileMenu, ztr("Open Page..."), QKeySequence::Open),
            &QAction::triggered, this, &NetzacWindow::openLocationDialog);
    connect(addMenuAction(fileMenu, ztr("Save As..."), QKeySequence::Save),
            &QAction::triggered, this, &NetzacWindow::savePage);
    fileMenu->addSeparator();
    connect(addMenuAction(fileMenu, ztr("Print..."), QKeySequence::Print),
            &QAction::triggered, this, &NetzacWindow::printPage);
    fileMenu->addSeparator();
    connect(addMenuAction(fileMenu, ztr("Close"), QKeySequence(QStringLiteral("Ctrl+Shift+W"))),
            &QAction::triggered, this, &QWidget::close);
    connect(addMenuAction(fileMenu, ztr("Quit"), QKeySequence::Quit),
            &QAction::triggered, QApplication::instance(), &QApplication::closeAllWindows);

    // Edit
    QMenu *editMenu = mb->addMenu(ztr("Edit"));
    connect(addMenuAction(editMenu, ztr("Undo"), QKeySequence::Undo),
            &QAction::triggered, this, [this]() {
                if (auto *p = webPage()) p->triggerAction(QWebEnginePage::Undo);
            });
    editMenu->addSeparator();
    connect(addMenuAction(editMenu, ztr("Cut"), QKeySequence::Cut),
            &QAction::triggered, this, [this]() {
                if (auto *p = webPage()) p->triggerAction(QWebEnginePage::Cut);
            });
    connect(addMenuAction(editMenu, ztr("Copy"), QKeySequence::Copy),
            &QAction::triggered, this, [this]() {
                if (auto *p = webPage()) p->triggerAction(QWebEnginePage::Copy);
            });
    connect(addMenuAction(editMenu, ztr("Paste"), QKeySequence::Paste),
            &QAction::triggered, this, [this]() {
                if (auto *p = webPage()) p->triggerAction(QWebEnginePage::Paste);
            });
    connect(addMenuAction(editMenu, ztr("Select All"), QKeySequence::SelectAll),
            &QAction::triggered, this, [this]() {
                if (auto *p = webPage()) p->triggerAction(QWebEnginePage::SelectAll);
            });
    editMenu->addSeparator();
    QAction *findAct = addMenuAction(editMenu, ztr("Find in Page..."));
    findAct->setShortcuts({QKeySequence::Find, QKeySequence(QStringLiteral("F4"))});
    connect(findAct, &QAction::triggered, this, &NetzacWindow::showFindBar);
    connect(addMenuAction(editMenu, ztr("Find Again"), QKeySequence(QStringLiteral("Ctrl+G"))),
            &QAction::triggered, this, &NetzacWindow::findNext);
    editMenu->addSeparator();
    connect(addMenuAction(editMenu, ztr("Preferences...")),
            &QAction::triggered, this, &NetzacWindow::openPreferences);

    // View
    QMenu *viewMenu = mb->addMenu(ztr("View"));
    QMenu *showHideMenu = viewMenu->addMenu(ztr("Show/Hide"));
    m_actShowNav = addCheckableAction(showHideMenu, ztr("Navigation Toolbar"), true);
    m_actShowNavText = addCheckableAction(
        showHideMenu, ztr("Navigation Button Labels"), m_prefs.showNavButtonText);
    m_actShowLoc = addCheckableAction(showHideMenu, ztr("Location Toolbar"), true);
    m_actShowPers = addCheckableAction(showHideMenu, ztr("Personal Toolbar"), true);
    m_actShowStatus = addCheckableAction(showHideMenu, ztr("Status Bar"), true);

    QMenu *tabsMenu = viewMenu->addMenu(ztr("Tabs & Links"));
    auto *linkModeGroup = new QActionGroup(this);
    m_actLinkModeTabs = addCheckableAction(
        tabsMenu,
        ztr("Open New Links in Tabs"),
        m_prefs.linkOpenMode == NetzacPreferences::LinkOpenMode::Tabs);
    m_actLinkModeWindows = addCheckableAction(
        tabsMenu,
        ztr("Open New Links in New Windows"),
        m_prefs.linkOpenMode == NetzacPreferences::LinkOpenMode::Windows);
    linkModeGroup->addAction(m_actLinkModeTabs);
    linkModeGroup->addAction(m_actLinkModeWindows);

    tabsMenu->addSeparator();
    auto *tabStyleGroup = new QActionGroup(this);
    m_actTabStyleLeft = addCheckableAction(
        tabsMenu,
        ztr("Classic Left Tabs"),
        m_prefs.tabBarStyle == NetzacPreferences::TabBarStyle::IE45Left);
    m_actTabStyleTop = addCheckableAction(
        tabsMenu,
        ztr("Top Tabs (Below Personal Toolbar)"),
        m_prefs.tabBarStyle == NetzacPreferences::TabBarStyle::ChromeTop);
    tabStyleGroup->addAction(m_actTabStyleLeft);
    tabStyleGroup->addAction(m_actTabStyleTop);

    connect(m_actLinkModeTabs, &QAction::triggered, this, [this]() {
        m_prefs.linkOpenMode = NetzacPreferences::LinkOpenMode::Tabs;
        m_prefs.save();
        updateTabBarPlacement();
    });
    connect(m_actLinkModeWindows, &QAction::triggered, this, [this]() {
        m_prefs.linkOpenMode = NetzacPreferences::LinkOpenMode::Windows;
        m_prefs.save();
        updateTabBarPlacement();
    });
    connect(m_actTabStyleLeft, &QAction::triggered, this, [this]() {
        m_prefs.tabBarStyle = NetzacPreferences::TabBarStyle::IE45Left;
        m_prefs.save();
        updateTabBarPlacement();
    });
    connect(m_actTabStyleTop, &QAction::triggered, this, [this]() {
        m_prefs.tabBarStyle = NetzacPreferences::TabBarStyle::ChromeTop;
        m_prefs.save();
        updateTabBarPlacement();
    });

    viewMenu->addSeparator();
    m_actStopMenu = addMenuAction(viewMenu, ztr("Stop"), QKeySequence(QStringLiteral("Esc")));
    connect(m_actStopMenu, &QAction::triggered, this, [this]() {
        if (auto *v = webView()) v->stop();
    });
    QAction *reloadAct = addMenuAction(viewMenu, ztr("Reload"));
    reloadAct->setShortcuts({QKeySequence(QStringLiteral("Ctrl+R")), QKeySequence(QStringLiteral("F5"))});
    connect(reloadAct, &QAction::triggered, this, [this]() {
        if (auto *v = webView()) v->reload();
    });
    m_actFullScreen = addCheckableAction(viewMenu, ztr("Full Screen"), false);
    m_actFullScreen->setShortcut(QKeySequence(QStringLiteral("F11")));
    connect(m_actFullScreen, &QAction::triggered, this, &NetzacWindow::toggleFullScreen);

    QMenu *zoomMenu = viewMenu->addMenu(ztr("Zoom"));
    QAction *zoomInAct = addMenuAction(zoomMenu, ztr("Zoom In"));
    zoomInAct->setShortcuts({QKeySequence::ZoomIn, QKeySequence(QStringLiteral("Ctrl+="))});
    connect(zoomInAct, &QAction::triggered, this, &NetzacWindow::zoomIn);
    QAction *zoomOutAct = addMenuAction(zoomMenu, ztr("Zoom Out"), QKeySequence::ZoomOut);
    connect(zoomOutAct, &QAction::triggered, this, &NetzacWindow::zoomOut);
    zoomMenu->addSeparator();
    QAction *zoomResetAct = addMenuAction(zoomMenu, ztr("Reset Zoom"), QKeySequence(QStringLiteral("Ctrl+0")));
    connect(zoomResetAct, &QAction::triggered, this, &NetzacWindow::resetZoom);

    viewMenu->addSeparator();
    m_actCalmMode = addCheckableAction(viewMenu, ztr("Calm Mode"), false);
    m_actCalmMode->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+C")));
    connect(m_actCalmMode, &QAction::toggled, this, [this](bool enabled) {
        if (auto *c = calmController()) c->setEnabled(enabled);
    });
    viewMenu->addSeparator();
    connect(addMenuAction(viewMenu, ztr("Page Source"), QKeySequence(QStringLiteral("Ctrl+U"))),
            &QAction::triggered, this, &NetzacWindow::viewPageSource);
    connect(addMenuAction(viewMenu, ztr("Page Info"), QKeySequence(QStringLiteral("Ctrl+I"))),
            &QAction::triggered, this, &NetzacWindow::showPageInfo);

    // Go
    m_goMenu = mb->addMenu(ztr("Go"));
    connect(m_goMenu, &QMenu::aboutToShow, this, &NetzacWindow::populateGoMenu);
    populateGoMenu();

    // Navigator
    QMenu *commMenu = mb->addMenu(ztr("Navigator"));
    connect(addMenuAction(commMenu, ztr("Navigator"), QKeySequence(QStringLiteral("Ctrl+1"))),
            &QAction::triggered, this, [this]() { navigateTo(QStringLiteral("about:netzac")); });
    connect(addMenuAction(commMenu, ztr("Mail"), QKeySequence(QStringLiteral("Ctrl+2"))),
            &QAction::triggered, this, [this]() {
                navigateTo(m_prefs.mailUrl.isEmpty() ? QStringLiteral("about:mail") : m_prefs.mailUrl);
            });
    connect(addMenuAction(commMenu, ztr("Chat"), QKeySequence(QStringLiteral("Ctrl+3"))),
            &QAction::triggered, this, [this]() {
                navigateTo(m_prefs.chatUrl.isEmpty() ? QStringLiteral("about:chat") : m_prefs.chatUrl);
            });
    connect(addMenuAction(commMenu, ztr("Git"), QKeySequence(QStringLiteral("Ctrl+4"))),
            &QAction::triggered, this, [this]() {
                navigateTo(m_prefs.gitUrl.isEmpty() ? QStringLiteral("about:git") : m_prefs.gitUrl);
            });
    commMenu->addSeparator();
    m_bookmarksMenu = commMenu->addMenu(ztr("Bookmarks"));
    connect(m_bookmarksMenu, &QMenu::aboutToShow, this, [this]() { populateBookmarksMenu(nullptr); });
    populateBookmarksMenu(nullptr);
    connect(addMenuAction(commMenu, ztr("History"), QKeySequence(QStringLiteral("Ctrl+H"))),
            &QAction::triggered, this, &NetzacWindow::showHistoryDialog);
    connect(addMenuAction(commMenu, ztr("Security Info")),
            &QAction::triggered, this, &NetzacWindow::showPageInfo);

    // Help
    QMenu *helpMenu = mb->addMenu(ztr("Help"));
    connect(addMenuAction(helpMenu, ztr("Help Contents"), QKeySequence::HelpContents),
            &QAction::triggered, this, [this]() { navigateTo(QStringLiteral("about:help")); });
    helpMenu->addSeparator();
    connect(addMenuAction(helpMenu, ztr("About Netzac...")),
            &QAction::triggered, this, &NetzacWindow::showAbout);
}

void NetzacWindow::populateGoMenu()
{
    m_goMenu->clear();
    NetzacWebView *view = webView();
    QWebEngineHistory *hist = view ? view->history() : nullptr;

    QAction *backAct = addMenuAction(m_goMenu, ztr("Back"), QKeySequence(QStringLiteral("Alt+Left")));
    if (view)
        connect(backAct, &QAction::triggered, view, &QWebEngineView::back);
    backAct->setEnabled(hist && hist->canGoBack());

    QAction *fwdAct = addMenuAction(m_goMenu, ztr("Forward"), QKeySequence(QStringLiteral("Alt+Right")));
    if (view)
        connect(fwdAct, &QAction::triggered, view, &QWebEngineView::forward);
    fwdAct->setEnabled(hist && hist->canGoForward());

    connect(addMenuAction(m_goMenu, ztr("Home"), QKeySequence(QStringLiteral("Alt+Home"))),
            &QAction::triggered, this, [this]() { navigateTo(m_prefs.homeUrl); });
    m_goMenu->addSeparator();
    connect(addMenuAction(m_goMenu, ztr("History..."), QKeySequence(QStringLiteral("Ctrl+H"))),
            &QAction::triggered, this, &NetzacWindow::showHistoryDialog);

    if (!hist)
        return;
    const QList<QWebEngineHistoryItem> items = hist->items();
    if (!items.isEmpty()) {
        m_goMenu->addSeparator();
        const int cur = hist->currentItemIndex();
        const int total = int(items.size());
        const int start = qMax(0, total - 10);
        for (int realIdx = total - 1; realIdx >= start; --realIdx) {
            const QWebEngineHistoryItem &item = items.at(realIdx);
            QString label = item.title().isEmpty() ? item.url().toString() : item.title();
            if (label.size() > 42)
                label = label.left(39) + QStringLiteral("...");
            QAction *act = addCheckableAction(m_goMenu, label, realIdx == cur);
            connect(act, &QAction::triggered, this, [hist, item]() { hist->goToItem(item); });
        }
    }
}

void NetzacWindow::populateBookmarksMenu(QMenu *targetMenu)
{
    QMenu *menu = targetMenu ? targetMenu : m_bookmarksMenu;
    menu->clear();
    connect(addMenuAction(menu, ztr("Add Bookmark"), QKeySequence(QStringLiteral("Ctrl+D"))),
            &QAction::triggered, this, &NetzacWindow::addCurrentBookmark);
    connect(addMenuAction(menu, ztr("Edit Bookmarks..."), QKeySequence(QStringLiteral("Ctrl+B"))),
            &QAction::triggered, this, &NetzacWindow::manageBookmarks);
    menu->addSeparator();
    for (const auto &entry : m_bookmarks) {
        QAction *act = menu->addAction(entry.first);
        act->setToolTip(entry.second);
        const QString url = entry.second;
        connect(act, &QAction::triggered, this, [this, url]() { navigateTo(url); });
    }
}

void NetzacWindow::buildToolbars()
{
    // 1. Navigation Toolbar: [Back][Forward][Home]  [Reload][Print][Find]  [Stop] ... [Z]
    auto *navBody = new QWidget();
    auto *navLay = new QHBoxLayout(navBody);
    navLay->setContentsMargins(0, 0, 0, 0);
    navLay->setSpacing(2);

    m_btnBack = new PlatinumNavButton(ztr("Back"), QStringLiteral("back"));
    m_btnBack->setToolTip(ztr("Go back one page (hold for history)"));
    connect(m_btnBack, &QPushButton::clicked, this, [this]() {
        if (auto *v = webView()) v->back();
    });
    m_btnBack->setHistoryMenuProvider([this](QMenu &menu) { buildNavHistoryMenu(menu, true); });

    m_btnForward = new PlatinumNavButton(ztr("Forward"), QStringLiteral("forward"));
    m_btnForward->setToolTip(ztr("Go forward one page (hold for history)"));
    connect(m_btnForward, &QPushButton::clicked, this, [this]() {
        if (auto *v = webView()) v->forward();
    });
    m_btnForward->setHistoryMenuProvider([this](QMenu &menu) { buildNavHistoryMenu(menu, false); });

    m_btnHome = new PlatinumNavButton(ztr("Home"), QStringLiteral("home"));
    m_btnHome->setToolTip(ztr("Go to Home page"));
    connect(m_btnHome, &QPushButton::clicked, this, [this]() { navigateTo(m_prefs.homeUrl); });

    m_btnReload = new PlatinumNavButton(ztr("Reload"), QStringLiteral("reload"));
    m_btnReload->setToolTip(ztr("Reload current page"));
    connect(m_btnReload, &QPushButton::clicked, this, [this]() {
        if (auto *v = webView()) v->reload();
    });

    m_btnPrint = new PlatinumNavButton(ztr("Print"), QStringLiteral("print"));
    m_btnPrint->setToolTip(ztr("Print this page"));
    connect(m_btnPrint, &QPushButton::clicked, this, &NetzacWindow::printPage);

    m_btnFind = new PlatinumNavButton(ztr("Find"), QStringLiteral("find"));
    m_btnFind->setToolTip(ztr("Find in current page"));
    connect(m_btnFind, &QPushButton::clicked, this, &NetzacWindow::showFindBar);

    m_btnStop = new PlatinumNavButton(ztr("Stop"), QStringLiteral("stop"));
    m_btnStop->setToolTip(ztr("Stop loading"));
    connect(m_btnStop, &QPushButton::clicked, this, [this]() {
        if (auto *v = webView()) v->stop();
    });

    m_btnCalm = new PlatinumNavButton(ztr("Calm"), QStringLiteral("calm"));
    m_btnCalm->setCheckable(true);
    m_btnCalm->setToolTip(ztr("Toggle Calm reading mode (configure filters in Edit > Preferences > Calm Mode)"));
    connect(m_btnCalm, &QPushButton::toggled, this, [this](bool enabled) {
        if (auto *c = calmController()) c->setEnabled(enabled);
    });

    navLay->addWidget(m_btnBack);
    navLay->addWidget(m_btnForward);
    navLay->addWidget(m_btnHome);
    navLay->addSpacing(12);
    navLay->addWidget(m_btnReload);
    navLay->addWidget(m_btnPrint);
    navLay->addWidget(m_btnFind);
    navLay->addSpacing(12);
    navLay->addWidget(m_btnStop);
    navLay->addSpacing(12);
    navLay->addWidget(m_btnCalm);

    navLay->addStretch(1);

    m_throbber = new NetzacThrobber();
    connect(m_throbber, &QPushButton::clicked, this, [this]() {
        navigateTo(QStringLiteral("about:netzac"));
    });
    navLay->addWidget(m_throbber);

    m_navToolbar = new CollapsibleToolbar(ztr("Navigation Toolbar"), navBody, this);

    // 2. Location Toolbar
    auto *locBody = new QWidget();
    auto *locLay = new QHBoxLayout(locBody);
    locLay->setContentsMargins(0, 1, 0, 1);
    locLay->setSpacing(6);

    m_btnBookmarks = new PlatinumCompactButton(ztr("Bookmarks"), QStringLiteral("bookmarks"), true);
    connect(m_btnBookmarks, &QPushButton::clicked, this, &NetzacWindow::showBookmarksPopup);
    locLay->addWidget(m_btnBookmarks);

    m_locationLabel = new QLabel(ztr("Netsite:"));
    m_locationLabel->setFont(zacosFont(QStringLiteral("ZacOS 9 System"), 12));
    locLay->addWidget(m_locationLabel);

    m_pageProxy = new PlatinumMiniIconButton(
        QStringLiteral("page"), ztr("Current page URL (click to bookmark)"));
    connect(m_pageProxy, &QPushButton::clicked, this, &NetzacWindow::addCurrentBookmark);
    locLay->addWidget(m_pageProxy);

    m_urlBar = new PlatinumLocationBar(m_visitedUrls);
    connect(m_urlBar, &PlatinumLocationBar::urlSubmitted, this, &NetzacWindow::navigateTo);
    locLay->addWidget(m_urlBar, 1);

    m_locToolbar = new CollapsibleToolbar(ztr("Location Toolbar"), locBody, this);

    // 3. Personal Toolbar
    m_persBody = new QWidget();
    m_persLay = new QHBoxLayout(m_persBody);
    m_persLay->setContentsMargins(0, 1, 0, 1);
    m_persLay->setSpacing(4);

    rebuildPersonalToolbar();

    m_persToolbar = new CollapsibleToolbar(ztr("Personal Toolbar"), m_persBody, this);

    connect(m_actShowNav, &QAction::toggled, m_navToolbar, &QWidget::setVisible);
    connect(m_actShowNavText, &QAction::toggled, this, [this](bool showText) {
        m_prefs.showNavButtonText = showText;
        m_prefs.save();
        applyNavToolbarCompact(showText);
    });
    connect(m_actShowLoc, &QAction::toggled, m_locToolbar, &QWidget::setVisible);
    connect(m_actShowPers, &QAction::toggled, m_persToolbar, &QWidget::setVisible);
    applyNavToolbarCompact(m_prefs.showNavButtonText);
}

void NetzacWindow::rebuildPersonalToolbar()
{
    if (!m_persLay)
        return;

    while (QLayoutItem *child = m_persLay->takeAt(0)) {
        if (QWidget *w = child->widget())
            w->deleteLater();
        delete child;
    }

    for (const auto &link : m_prefs.personalToolbar) {
        const bool isFolder = link.isFolder || link.icon == QLatin1String("folder");
        const QString iconName = isFolder
            ? QStringLiteral("folder")
            : (link.icon.isEmpty() ? QStringLiteral("page") : link.icon);
        auto *btn = new PlatinumCompactButton(link.title, iconName, isFolder);
        if (isFolder) {
            btn->setToolTip(ztr("%1 (%2 item(s))").arg(link.title).arg(link.children.size()));
            const QVector<NetzacPreferences::PersonalToolbarSubItem> children = link.children;
            connect(btn, &QPushButton::clicked, this, [this, btn, children]() {
                QMenu menu(this);
                if (children.isEmpty()) {
                    QAction *emptyAct = menu.addAction(ztr("(Empty folder)"));
                    emptyAct->setEnabled(false);
                } else {
                    for (const auto &sub : children) {
                        QAction *act = menu.addAction(sub.title);
                        act->setToolTip(sub.url);
                        const QString targetUrl = sub.url;
                        connect(act, &QAction::triggered, this, [this, targetUrl]() {
                            navigateTo(targetUrl);
                        });
                    }
                }
                menu.exec(btn->mapToGlobal(QPoint(0, btn->height())));
            });
        } else {
            btn->setToolTip(link.url);
            const QString targetUrl = link.url;
            connect(btn, &QPushButton::clicked, this, [this, targetUrl]() { navigateTo(targetUrl); });
        }
        m_persLay->addWidget(btn);
    }

    m_persLay->addStretch(1);
}

void NetzacWindow::buildCentralArea()
{
    auto *central = new QWidget(this);
    m_rootLayout = new QVBoxLayout(central);
    m_rootLayout->setContentsMargins(0, 0, 0, 0);
    m_rootLayout->setSpacing(0);

    m_rootLayout->addWidget(m_navToolbar);
    m_rootLayout->addWidget(m_locToolbar);
    m_rootLayout->addWidget(m_persToolbar);

    auto *workspace = new QWidget(central);
    m_workspaceLayout = new QHBoxLayout(workspace);
    m_workspaceLayout->setContentsMargins(0, 0, 0, 0);
    m_workspaceLayout->setSpacing(0);

    m_findBar = new QWidget(central);
    auto *findLay = new QHBoxLayout(m_findBar);
    findLay->setContentsMargins(6, 3, 6, 3);
    findLay->setSpacing(6);
    findLay->addWidget(new QLabel(ztr("Find:"), m_findBar));
    m_findInput = new QLineEdit(m_findBar);
    m_findInput->setFixedWidth(220);
    connect(m_findInput, &QLineEdit::returnPressed, this, &NetzacWindow::findNext);
    findLay->addWidget(m_findInput);
    auto *findNextBtn = new QPushButton(ztr("Find Next"), m_findBar);
    findNextBtn->setIcon(QIcon());
    connect(findNextBtn, &QPushButton::clicked, this, &NetzacWindow::findNext);
    auto *findCloseBtn = new QPushButton(ztr("Done"), m_findBar);
    findCloseBtn->setIcon(QIcon());
    connect(findCloseBtn, &QPushButton::clicked, this, &NetzacWindow::hideFindBar);
    findLay->addWidget(findNextBtn);
    findLay->addWidget(findCloseBtn);
    findLay->addStretch(1);
    m_findBar->hide();

    m_viewportFrame = new PlatinumViewportFrame(m_tabStack, workspace);
    m_workspaceLayout->addWidget(m_viewportFrame, 1);

    m_rootLayout->addWidget(workspace, 1);
    m_rootLayout->addWidget(m_findBar);

    setCentralWidget(central);
}

void NetzacWindow::buildStatusBar()
{
    m_status = new QStatusBar(this);
    m_status->setSizeGripEnabled(true);
    m_status->setFont(zacosFont(QStringLiteral("ZacOS 9 Views"), 9));
    setStatusBar(m_status);

    m_statusSecurity = new PlatinumMiniIconButton(
        QStringLiteral("security_open"), ztr("Security Info"), m_status);
    connect(m_statusSecurity, &QPushButton::clicked, this, &NetzacWindow::showPageInfo);
    m_status->addWidget(m_statusSecurity);

    m_progressBar = new PlatinumProgressBar(m_status);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_status->addWidget(m_progressBar);

    m_statusLabel = new ElidingStatusLabel(ztr("Document: Done"), m_status);
    m_statusLabel->setFont(zacosFont(QStringLiteral("ZacOS 9 Views"), 9));
    m_status->addWidget(m_statusLabel, 1);

    auto *compBox = new QWidget(m_status);
    auto *compLay = new QHBoxLayout(compBox);
    compLay->setContentsMargins(2, 0, 2, 0);
    compLay->setSpacing(2);

    auto *bNav = new PlatinumMiniIconButton(QStringLiteral("navigator"), ztr("Navigator (Ctrl+1)"), compBox);
    connect(bNav, &QPushButton::clicked, this, [this]() { navigateTo(QStringLiteral("about:netzac")); });
    compLay->addWidget(bNav);

    auto *bMail = new PlatinumMiniIconButton(QStringLiteral("mail"), ztr("Mail (Ctrl+2)"), compBox);
    connect(bMail, &QPushButton::clicked, this, [this]() {
        navigateTo(m_prefs.mailUrl.isEmpty() ? QStringLiteral("about:mail") : m_prefs.mailUrl);
    });
    compLay->addWidget(bMail);

    auto *bChat = new PlatinumMiniIconButton(QStringLiteral("collabra"), ztr("Chat (Ctrl+3)"), compBox);
    connect(bChat, &QPushButton::clicked, this, [this]() {
        navigateTo(m_prefs.chatUrl.isEmpty() ? QStringLiteral("about:chat") : m_prefs.chatUrl);
    });
    compLay->addWidget(bChat);

    auto *bGit = new PlatinumMiniIconButton(QStringLiteral("composer"), ztr("Git (Ctrl+4)"), compBox);
    connect(bGit, &QPushButton::clicked, this, [this]() {
        navigateTo(m_prefs.gitUrl.isEmpty() ? QStringLiteral("about:git") : m_prefs.gitUrl);
    });
    compLay->addWidget(bGit);

    m_status->addPermanentWidget(compBox);

    connect(m_actShowStatus, &QAction::toggled, m_status, &QWidget::setVisible);
    updateNavStates();
}

int NetzacWindow::newTab(const QString &url, bool switchTo)
{
    return createTabInternal(url, switchTo, !url.isEmpty());
}

int NetzacWindow::createTabInternal(const QString &url, bool switchTo, bool loadInitial)
{
    auto *view = new NetzacWebView(m_tabStack);
    auto *page = new NetzacWebPage(netzacProfile(), view);
    view->setPage(page);
    auto *calm = new CalmModeController(page, m_prefs.calm, view);

    view->setMinimumSize(240, 160);
    page->setBackgroundColor(QColor(QStringLiteral("#dddddd")));
    page->settings()->setAttribute(QWebEngineSettings::AutoLoadImages, m_prefs.autoloadImages);
    page->settings()->setAttribute(QWebEngineSettings::JavascriptEnabled, m_prefs.enableJavaScript);
    page->settings()->setAttribute(QWebEngineSettings::LocalStorageEnabled, true);
    if (m_popupOpts.hideScrollbars)
        page->settings()->setAttribute(QWebEngineSettings::ShowScrollBars, false);

    view->setWindowCreator([this, view](QWebEnginePage::WebWindowType type) {
        return createBrowserWindow(view, type);
    });

    BrowserTab tab;
    tab.view = view;
    tab.page = page;
    tab.calm = calm;
    tab.currentUrl = url.isEmpty() ? m_prefs.homeUrl : url;
    tab.title = ztr("New Tab");

    m_tabs.append(tab);
    const int idx = int(m_tabs.size()) - 1;
    m_tabStack->addWidget(view);
    m_tabBar->addTab(tab.title);
    connectTabSignals(view, page, calm);
    connect(page, &NetzacWebPage::credentialSubmitted, this, &NetzacWindow::handleCredentialSubmitted);

    if (switchTo || m_currentTabIndex < 0)
        switchToTab(idx);

    if (loadInitial && !url.isEmpty()) {
        const int prev = m_currentTabIndex;
        m_currentTabIndex = idx;
        navigateTo(url);
        if (!switchTo && prev >= 0 && prev < m_tabs.size())
            m_currentTabIndex = prev;
    }

    return idx;
}

void NetzacWindow::moveTab(int from, int to)
{
    if (from == to || from < 0 || to < 0 || from >= m_tabs.size() || to >= m_tabs.size())
        return;

    const BrowserTab tab = m_tabs.takeAt(from);
    m_tabs.insert(to, tab);

    if (m_currentTabIndex == from) {
        m_currentTabIndex = to;
    } else if (from < m_currentTabIndex && to >= m_currentTabIndex) {
        --m_currentTabIndex;
    } else if (from > m_currentTabIndex && to <= m_currentTabIndex) {
        ++m_currentTabIndex;
    }
    m_tabBar->moveTab(from, to);
}

void NetzacWindow::detachTab(int index, const QPoint &globalPos)
{
    if (index < 0 || index >= m_tabs.size() || m_popupOpts.isPopup)
        return;

    BrowserTab tab = m_tabs.at(index);
    disconnect(tab.view, nullptr, this, nullptr);
    disconnect(tab.page, nullptr, this, nullptr);
    disconnect(tab.calm, nullptr, this, nullptr);

    m_tabs.removeAt(index);
    m_tabStack->removeWidget(tab.view);
    m_tabBar->removeTab(index);

    int next = m_currentTabIndex;
    if (next == index)
        next = qMin(index, int(m_tabs.size()) - 1);
    else if (next > index)
        --next;
    if (next < 0 && !m_tabs.isEmpty())
        next = 0;
    m_currentTabIndex = -1;
    if (next >= 0 && next < m_tabs.size())
        switchToTab(next);
    else if (m_tabs.isEmpty() && !m_popupOpts.isPopup)
        newTab(m_prefs.homeUrl, true);

    auto *win = new NetzacWindow(QString(), nullptr, false);
    win->setAttribute(Qt::WA_DeleteOnClose);
    win->clearTabsForAdoption();
    win->adoptTab(tab);
    win->resize(size());
    win->move(globalPos - QPoint(60, 16));
    win->show();
    win->raise();
    win->activateWindow();
}

void NetzacWindow::adoptTab(const BrowserTab &tab)
{
    NetzacWebView *view = tab.view;
    view->setParent(m_tabStack);
    m_tabStack->addWidget(view);
    view->setWindowCreator([this, view](QWebEnginePage::WebWindowType type) {
        return createBrowserWindow(view, type);
    });

    m_tabs.append(tab);
    const int idx = int(m_tabs.size()) - 1;
    m_tabBar->addTab(tab.title);
    m_tabBar->setTabUrl(idx, tab.currentUrl);
    m_tabBar->setTabLoading(idx, tab.isLoading);

    connectTabSignals(view, tab.page, tab.calm);
    connect(tab.page, &NetzacWebPage::credentialSubmitted, this, &NetzacWindow::handleCredentialSubmitted);

    m_currentTabIndex = -1;
    switchToTab(idx);
    updateTabBarPlacement();
}

void NetzacWindow::clearTabsForAdoption()
{
    while (!m_tabs.isEmpty()) {
        BrowserTab tab = m_tabs.takeLast();
        disconnect(tab.view, nullptr, this, nullptr);
        disconnect(tab.page, nullptr, this, nullptr);
        disconnect(tab.calm, nullptr, this, nullptr);
        m_tabStack->removeWidget(tab.view);
        tab.view->deleteLater();
    }
    while (m_tabBar->count() > 0)
        m_tabBar->removeTab(m_tabBar->count() - 1);
    m_currentTabIndex = -1;
}

void NetzacWindow::closeTab(int index)
{
    if (index < 0 || index >= m_tabs.size())
        return;
    if (m_tabs.size() <= 1) {
        close();
        return;
    }

    BrowserTab removed = m_tabs.takeAt(index);
    m_tabStack->removeWidget(removed.view);
    m_tabBar->removeTab(index);
    removed.view->deleteLater();

    int nextIdx = m_currentTabIndex;
    if (nextIdx == index) {
        nextIdx = qMin(index, int(m_tabs.size()) - 1);
    } else if (nextIdx > index) {
        --nextIdx;
    }
    m_currentTabIndex = -1;
    switchToTab(nextIdx);
}

void NetzacWindow::switchToTab(int index)
{
    if (index < 0 || index >= m_tabs.size() || index == m_currentTabIndex)
        return;
    m_currentTabIndex = index;
    m_tabStack->setCurrentWidget(m_tabs[index].view);
    m_tabBar->setCurrentIndex(index);
    syncUiToCurrentTab();
}

void NetzacWindow::syncUiToCurrentTab()
{
    const BrowserTab *tab = currentTab();
    if (!tab)
        return;

    const QString &urlStr = tab->currentUrl;
    m_urlBar->setText(urlStr);
    const bool isWeb = urlStr.startsWith(QLatin1String("http://"))
        || urlStr.startsWith(QLatin1String("https://"));
    m_locationLabel->setText(isWeb ? ztr("Netsite:") : ztr("Location:"));
    const bool isHttps = urlStr.startsWith(QLatin1String("https://"));
    m_statusSecurity->setIconName(isHttps ? QStringLiteral("security_locked") : QStringLiteral("security_open"));

    if (!tab->title.isEmpty())
        setWindowTitle(ztr("Netzac: %1").arg(tab->title));
    else
        setWindowTitle(ztr("Netzac"));

    m_throbber->setLoading(tab->isLoading);
    m_progressBar->setIndeterminate(tab->isLoading && tab->loadProgress <= 0);
    m_progressBar->setValue(tab->isLoading ? tab->loadProgress : 0);

    if (tab->isLoading) {
        m_statusLabel->setStatusText(
            tab->loadProgress > 0
                ? ztr("Loading: %1%").arg(tab->loadProgress)
                : ztr("Connecting to %1...").arg(urlStr));
    } else {
        m_statusLabel->setStatusText(
            (tab->calm && tab->calm->isAppliedOnCurrentPage())
                ? ztr("Document: Done — Calm Mode active")
                : ztr("Document: Done"));
    }

    if (m_btnCalm && tab->calm) {
        const QSignalBlocker blocker(m_btnCalm);
        m_btnCalm->setChecked(tab->calm->isEnabled());
        m_btnCalm->update();
    }
    if (m_actCalmMode && tab->calm) {
        const QSignalBlocker blocker(m_actCalmMode);
        m_actCalmMode->setChecked(tab->calm->isEnabled());
    }

    updateNavStates();
}

void NetzacWindow::updateTabBarPlacement()
{
    if (!m_tabBar || !m_rootLayout || !m_workspaceLayout)
        return;

    const bool showTabs = !m_popupOpts.isPopup
        && !m_popupOpts.hideToolbar
        && (m_prefs.linkOpenMode == NetzacPreferences::LinkOpenMode::Tabs);

    if (m_actTabStyleLeft)
        m_actTabStyleLeft->setEnabled(showTabs);
    if (m_actTabStyleTop)
        m_actTabStyleTop->setEnabled(showTabs);
    if (m_actNewTab)
        m_actNewTab->setEnabled(!m_popupOpts.isPopup && !m_popupOpts.hideToolbar);
    if (m_actCloseTab)
        m_actCloseTab->setEnabled(!m_popupOpts.isPopup && !m_popupOpts.hideToolbar);

    m_rootLayout->removeWidget(m_tabBar);
    m_workspaceLayout->removeWidget(m_tabBar);

    if (!showTabs) {
        m_tabBar->hide();
        return;
    }

    if (m_prefs.tabBarStyle == NetzacPreferences::TabBarStyle::IE45Left) {
        m_tabBar->setTabStyle(PlatinumTabBar::TabStyle::IE45Left);
        m_workspaceLayout->insertWidget(0, m_tabBar);
    } else {
        m_tabBar->setTabStyle(PlatinumTabBar::TabStyle::ChromeTop);
        const int persIdx = m_rootLayout->indexOf(m_persToolbar);
        m_rootLayout->insertWidget(persIdx >= 0 ? persIdx + 1 : 3, m_tabBar);
    }
    m_tabBar->show();
}

void NetzacWindow::applyPopupRestrictions()
{
    if (m_popupOpts.isPopup || m_popupOpts.hideToolbar) {
        if (m_navToolbar)
            m_navToolbar->hide();
        if (m_persToolbar)
            m_persToolbar->hide();
        if (m_tabBar)
            m_tabBar->hide();
        if (m_locToolbar)
            m_locToolbar->show();
        if (m_urlBar)
            m_urlBar->setReadOnly(true);
        if (m_btnBookmarks)
            m_btnBookmarks->hide();
        if (m_actShowNav) {
            m_actShowNav->setChecked(false);
            m_actShowNav->setEnabled(false);
        }
        if (m_actShowPers) {
            m_actShowPers->setChecked(false);
            m_actShowPers->setEnabled(false);
        }
        if (m_actShowLoc)
            m_actShowLoc->setEnabled(false);
    }

    if (m_popupOpts.geometry.isValid()) {
        const int w = qMax(240, m_popupOpts.geometry.width());
        const int h = qMax(180, m_popupOpts.geometry.height());
        if (m_popupOpts.nonResizable)
            setFixedSize(w, h);
        else
            resize(w, h);
    } else if (m_popupOpts.nonResizable) {
        setFixedSize(size());
    }

    if (m_popupOpts.nonResizable && m_status)
        m_status->setSizeGripEnabled(false);
}

void NetzacWindow::applyNavToolbarCompact(bool showText)
{
    const QVector<PlatinumNavButton *> navButtons = {
        m_btnBack, m_btnForward, m_btnHome, m_btnReload,
        m_btnPrint, m_btnFind, m_btnStop, m_btnCalm,
    };
    for (PlatinumNavButton *btn : navButtons) {
        if (btn)
            btn->setShowText(showText);
    }
    if (m_throbber)
        m_throbber->setCompact(!showText);
}

void NetzacWindow::toggleFullScreen()
{
    if (isFullScreen()) {
        showNormal();
        if (m_actFullScreen) {
            const QSignalBlocker b(m_actFullScreen);
            m_actFullScreen->setChecked(false);
        }
        if (m_actShowPers)
            m_actShowPers->setChecked(m_preFsShowPers);
        applyNavToolbarCompact(m_preFsShowNavText);
        if (m_actShowNavText) {
            const QSignalBlocker b(m_actShowNavText);
            m_actShowNavText->setChecked(m_preFsShowNavText);
        }
    } else {
        m_preFsShowPers = m_actShowPers ? m_actShowPers->isChecked() : true;
        m_preFsShowNavText = m_prefs.showNavButtonText;
        showFullScreen();
        if (m_actFullScreen) {
            const QSignalBlocker b(m_actFullScreen);
            m_actFullScreen->setChecked(true);
        }
        if (m_actShowPers)
            m_actShowPers->setChecked(false);
        applyNavToolbarCompact(false);
        if (m_actShowNavText) {
            const QSignalBlocker b(m_actShowNavText);
            m_actShowNavText->setChecked(false);
        }
    }
}

void NetzacWindow::zoomIn()
{
    if (NetzacWebView *view = webView()) {
        const qreal next = qMin<qreal>(3.0, view->zoomFactor() + 0.1);
        view->setZoomFactor(next);
        if (m_statusLabel)
            m_statusLabel->setStatusText(ztr("Zoom: %1%").arg(qRound(next * 100.0)));
    }
}

void NetzacWindow::zoomOut()
{
    if (NetzacWebView *view = webView()) {
        const qreal next = qMax<qreal>(0.25, view->zoomFactor() - 0.1);
        view->setZoomFactor(next);
        if (m_statusLabel)
            m_statusLabel->setStatusText(ztr("Zoom: %1%").arg(qRound(next * 100.0)));
    }
}

void NetzacWindow::resetZoom()
{
    if (NetzacWebView *view = webView()) {
        view->setZoomFactor(1.0);
        if (m_statusLabel)
            m_statusLabel->setStatusText(ztr("Zoom: 100%"));
    }
}

void NetzacWindow::promptDefaultBrowserIfNeeded()
{
    if (m_prefs.defaultBrowserPrompted)
        return;
    m_prefs.defaultBrowserPrompted = true;
    m_prefs.save();

    if (NetzacPreferences::isDefaultBrowser())
        return;

    PlatinumDialog dlg(this);
    dlg.setWindowTitle(ztr("Default Browser"));
    dlg.setFixedWidth(400);

    auto *lay = new QVBoxLayout(&dlg);
    lay->setContentsMargins(14, 14, 14, 14);
    lay->setSpacing(10);

    auto *topRow = new QHBoxLayout();
    auto *iconLbl = new QLabel(&dlg);
    iconLbl->setPixmap(makeAlertIcon(QStringLiteral("info"), 32));
    iconLbl->setAlignment(Qt::AlignTop);
    auto *msgLbl = new QLabel(
        ztr("Netzac Navigator is not currently set as your default web browser.\n\n"
            "Would you like to set Netzac as your default browser for web links?"),
        &dlg);
    msgLbl->setWordWrap(true);
    topRow->addWidget(iconLbl);
    topRow->addWidget(msgLbl, 1);
    lay->addLayout(topRow);

    auto *btnRow = new QHBoxLayout();
    btnRow->addStretch(1);
    QPushButton *noBtn = makeDialogButton(ztr("Not Now"), false, &dlg);
    connect(noBtn, &QPushButton::clicked, &dlg, &QDialog::reject);
    QPushButton *yesBtn = makeDialogButton(ztr("Set as Default"), true, &dlg);
    connect(yesBtn, &QPushButton::clicked, &dlg, &QDialog::accept);
    btnRow->addWidget(noBtn);
    btnRow->addWidget(yesBtn);
    lay->addLayout(btnRow);

    if (dlg.exec() == QDialog::Accepted) {
        if (NetzacPreferences::setAsDefaultBrowser()) {
            if (m_statusLabel)
                m_statusLabel->setStatusText(ztr("Netzac set as default browser"));
        }
    }
}

void NetzacWindow::connectTabSignals(NetzacWebView *view, NetzacWebPage *page, CalmModeController *calm)
{
    connect(view, &QWebEngineView::urlChanged, this, [this, view](const QUrl &qurl) {
        onTabUrlChanged(view, qurl);
    });
    connect(view, &QWebEngineView::titleChanged, this, [this, view](const QString &title) {
        onTabTitleChanged(view, title);
    });
    connect(view, &QWebEngineView::loadStarted, this, [this, view]() {
        onTabLoadStarted(view);
    });
    connect(view, &QWebEngineView::loadProgress, this, [this, view](int progress) {
        onTabLoadProgress(view, progress);
    });
    connect(view, &QWebEngineView::loadFinished, this, [this, view](bool ok) {
        onTabLoadFinished(view, ok);
    });
    connect(page, &QWebEnginePage::linkHovered, this, [this, view](const QString &url) {
        onTabLinkHovered(view, url);
    });
    connect(page, &QWebEnginePage::windowCloseRequested, this, [this, view]() {
        const int idx = indexOfView(view);
        if (idx >= 0)
            closeTab(idx);
    });
    connect(page, &QWebEnginePage::geometryChangeRequested, this, [this](const QRect &geom) {
        if (!m_popupOpts.isPopup || !geom.isValid())
            return;
        const int w = qMax(240, geom.width());
        const int h = qMax(180, geom.height());
        if (m_popupOpts.nonResizable)
            setFixedSize(w, h);
        else
            resize(w, h);
    });
    connect(view, &NetzacWebView::contextMenuRequested, this, &NetzacWindow::showWebContextMenu);
    connect(calm, &CalmModeController::enabledChanged, this, [this, view](bool enabled) {
        onCalmEnabledChanged(view, enabled);
    });
    connect(
        calm,
        &CalmModeController::evaluationFinished,
        this,
        [this, view](const CalmModeController::EvaluationReport &report) {
            onCalmEvaluationFinished(view, report);
        },
        Qt::QueuedConnection);
}

void NetzacWindow::setCalmModeEnabled(bool enabled)
{
    if (CalmModeController *c = calmController())
        c->setEnabled(enabled);
}

void NetzacWindow::onCalmEnabledChanged(NetzacWebView *view, bool enabled)
{
    if (indexOfView(view) != m_currentTabIndex)
        return;
    if (m_btnCalm) {
        const QSignalBlocker blocker(m_btnCalm);
        m_btnCalm->setChecked(enabled);
        m_btnCalm->update();
    }
    if (m_actCalmMode) {
        const QSignalBlocker blocker(m_actCalmMode);
        m_actCalmMode->setChecked(enabled);
    }
}

void NetzacWindow::onCalmEvaluationFinished(NetzacWebView *view, const CalmModeController::EvaluationReport &report)
{
    const int idx = indexOfView(view);
    if (idx < 0 || idx != m_currentTabIndex || !m_statusLabel || m_tabs[idx].isLoading)
        return;

    switch (report.status) {
    case CalmModeController::Status::Applied:
    case CalmModeController::Status::AlreadyApplied:
        m_statusLabel->setStatusText(
            report.usedReaderFocus
                ? ztr("Document: Done — Calm Reader active")
                : ztr("Document: Done — Calm Mode active"));
        break;
    case CalmModeController::Status::ExcludedHost:
        m_statusLabel->setStatusText(ztr("Document: Done — Calm Mode skipped (excluded host)"));
        break;
    case CalmModeController::Status::SkippedBuiltIn:
        m_statusLabel->setStatusText(ztr("Document: Done — Calm Mode skipped (built-in page)"));
        break;
    case CalmModeController::Status::SkippedJavaScript:
        m_statusLabel->setStatusText(ztr("Document: Done — Calm Mode skipped (JavaScript page)"));
        break;
    case CalmModeController::Status::SkippedWebApp:
        m_statusLabel->setStatusText(ztr("Document: Done — Calm Mode skipped (interactive web app)"));
        break;
    case CalmModeController::Status::Disabled:
        m_statusLabel->setStatusText(ztr("Document: Done"));
        break;
    case CalmModeController::Status::Error:
        m_statusLabel->setStatusText(ztr("Document: Done"));
        break;
    }
}

void NetzacWindow::onTabUrlChanged(NetzacWebView *view, const QUrl &qurl)
{
    const int idx = indexOfView(view);
    if (idx < 0)
        return;
    BrowserTab &tab = m_tabs[idx];

    QString urlStr = qurl.toString();
    if (tab.showingBuiltin
        && (urlStr == QLatin1String("https://localhost/") || urlStr == QLatin1String("about:blank"))) {
        urlStr = tab.currentUrl;
    } else if (urlStr.startsWith(QLatin1String("data:")) || urlStr == QLatin1String("about:blank")) {
        urlStr = tab.currentUrl;
    } else {
        tab.showingBuiltin = false;
        tab.currentUrl = urlStr;
    }

    m_tabBar->setTabUrl(idx, urlStr);
    if (!urlStr.isEmpty() && !m_visitedUrls.contains(urlStr)) {
        m_visitedUrls.prepend(urlStr);
        m_urlBar->recordUrl(urlStr);
    }
    HistoryStorage::recordVisit(m_history, urlStr, view->title());
    if (!m_popupOpts.isPopup)
        NetzacPreferences::saveLastUrl(urlStr);

    if (idx == m_currentTabIndex) {
        m_urlBar->setText(urlStr);
        const bool isWeb = urlStr.startsWith(QLatin1String("http://"))
            || urlStr.startsWith(QLatin1String("https://"));
        m_locationLabel->setText(isWeb ? ztr("Netsite:") : ztr("Location:"));
        const bool isHttps = urlStr.startsWith(QLatin1String("https://"));
        m_statusSecurity->setIconName(isHttps ? QStringLiteral("security_locked") : QStringLiteral("security_open"));
        updateNavStates();
    }
}

void NetzacWindow::onTabTitleChanged(NetzacWebView *view, const QString &title)
{
    const int idx = indexOfView(view);
    if (idx < 0)
        return;
    m_tabs[idx].title = title;
    m_tabBar->setTabTitle(idx, title.isEmpty() ? m_tabs[idx].currentUrl : title);

    if (!title.isEmpty())
        HistoryStorage::recordVisit(m_history, m_tabs[idx].currentUrl, title);

    if (idx == m_currentTabIndex) {
        if (!title.isEmpty())
            setWindowTitle(ztr("Netzac: %1").arg(title));
        else
            setWindowTitle(ztr("Netzac"));
    }
}

void NetzacWindow::onTabLoadStarted(NetzacWebView *view)
{
    const int idx = indexOfView(view);
    if (idx < 0)
        return;
    m_tabs[idx].isLoading = true;
    m_tabs[idx].loadProgress = 0;
    m_tabBar->setTabLoading(idx, true);

    if (idx == m_currentTabIndex) {
        m_throbber->setLoading(true);
        m_progressBar->setIndeterminate(true);
        m_progressBar->setValue(0);
        m_statusLabel->setStatusText(ztr("Connecting to %1...").arg(m_tabs[idx].currentUrl));
        updateNavStates();
    }
}

void NetzacWindow::onTabLoadProgress(NetzacWebView *view, int progress)
{
    const int idx = indexOfView(view);
    if (idx < 0)
        return;
    m_tabs[idx].loadProgress = progress;

    if (idx == m_currentTabIndex) {
        if (progress > 0 && progress < 100) {
            m_progressBar->setIndeterminate(false);
            m_progressBar->setValue(progress);
            m_statusLabel->setStatusText(ztr("Loading: %1%").arg(progress));
        } else if (progress >= 100) {
            m_progressBar->setIndeterminate(false);
            m_progressBar->setValue(100);
            m_statusLabel->setStatusText(ztr("Document: Done"));
        }
    }
}

void NetzacWindow::onTabLoadFinished(NetzacWebView *view, bool ok)
{
    const int idx = indexOfView(view);
    if (idx < 0)
        return;
    m_tabs[idx].isLoading = false;
    m_tabs[idx].loadProgress = 0;
    m_tabBar->setTabLoading(idx, false);

    if (ok) {
        HistoryStorage::recordVisit(m_history, m_tabs[idx].currentUrl, view->title());
        HistoryStorage::save(m_history);
        if (m_prefs.rememberPasswords && m_prefs.enableJavaScript && !m_tabs[idx].showingBuiltin)
            installPasswordHelpers(m_tabs[idx].page, QUrl(m_tabs[idx].currentUrl));
    }

    if (idx == m_currentTabIndex) {
        m_throbber->setLoading(false);
        m_progressBar->setIndeterminate(false);
        m_progressBar->setValue(0);
        m_statusLabel->setStatusText(ok ? ztr("Document: Done") : ztr("Transfer interrupted"));
        updateNavStates();
    }
}

void NetzacWindow::onTabLinkHovered(NetzacWebView *view, const QString &url)
{
    const int idx = indexOfView(view);
    if (idx < 0 || idx != m_currentTabIndex)
        return;
    if (!url.isEmpty())
        m_statusLabel->setStatusText(url);
    else if (!m_tabs[idx].isLoading)
        m_statusLabel->setStatusText(
            (m_tabs[idx].calm && m_tabs[idx].calm->isAppliedOnCurrentPage())
                ? ztr("Document: Done — Calm Mode active")
                : ztr("Document: Done"));
}

QWebEngineView *NetzacWindow::createBrowserWindow(NetzacWebView *opener, QWebEnginePage::WebWindowType type)
{
    QString features;
    if (opener) {
        if (auto *openerPage = qobject_cast<NetzacWebPage *>(opener->page()))
            features = openerPage->takePendingPopupFeatures();
    }

    const PopupOptions opts = parsePopupFeatures(features, type);
    if (opts.isPopup) {
        auto *win = new NetzacWindow(QString(), nullptr, false, opts);
        win->setAttribute(Qt::WA_DeleteOnClose);
        if (CalmModeController *c = calmController())
            win->setCalmModeEnabled(c->isEnabled());
        win->show();
        win->raise();
        win->activateWindow();
        return win->webView();
    }

    if (m_prefs.linkOpenMode == NetzacPreferences::LinkOpenMode::Tabs && !m_popupOpts.isPopup) {
        const bool switchTo = (type != QWebEnginePage::WebBrowserBackgroundTab);
        const int idx = createTabInternal(QString(), switchTo, false);
        return m_tabs[idx].view;
    }

    auto *win = new NetzacWindow(QString(), nullptr, false);
    win->setAttribute(Qt::WA_DeleteOnClose);
    if (CalmModeController *c = calmController())
        win->setCalmModeEnabled(c->isEnabled());
    win->show();
    win->raise();
    win->activateWindow();
    return win->webView();
}

void NetzacWindow::showWebContextMenu(const QPoint &globalPos, QWebEngineContextMenuRequest *request)
{
    NetzacWebView *view = webView();
    NetzacWebPage *page = webPage();
    if (!view || !page)
        return;

    QMenu menu(this);
    menu.setFont(zacosFont(QStringLiteral("ZacOS 9 System"), 12));

    if (request && request->linkUrl().isValid()) {
        const QUrl linkUrl = request->linkUrl();
        const QString linkStr = linkUrl.toString();
        if (m_prefs.linkOpenMode == NetzacPreferences::LinkOpenMode::Tabs && !m_popupOpts.isPopup) {
            QAction *openTabAct = menu.addAction(ztr("Open Link in New Tab"));
            connect(openTabAct, &QAction::triggered, this, [this, linkStr]() {
                newTab(linkStr, true);
            });
        }
        QAction *openWin = menu.addAction(ztr("Open Link in New Window"));
        connect(openWin, &QAction::triggered, this, [linkStr]() {
            auto *win = new NetzacWindow(linkStr, nullptr, true);
            win->setAttribute(Qt::WA_DeleteOnClose);
            win->show();
        });
        QAction *copyLink = menu.addAction(ztr("Copy Link Location"));
        connect(copyLink, &QAction::triggered, this, [linkStr]() {
            if (QClipboard *cb = QApplication::clipboard())
                cb->setText(linkStr);
        });
        QAction *bmLink = menu.addAction(ztr("Add Bookmark for Link"));
        connect(bmLink, &QAction::triggered, this, [this, linkStr]() {
            bool exists = false;
            for (const auto &entry : m_bookmarks) {
                if (entry.second == linkStr) {
                    exists = true;
                    break;
                }
            }
            if (!exists) {
                m_bookmarks.append({linkStr, linkStr});
                BookmarkStorage::save(m_bookmarks);
            }
            m_statusLabel->setStatusText(ztr("Bookmarked: %1").arg(linkStr));
        });
        menu.addSeparator();
    }

    if (request
        && request->mediaType() == QWebEngineContextMenuRequest::MediaTypeImage
        && request->mediaUrl().isValid()) {
        const QUrl mediaUrl = request->mediaUrl();
        const QString mediaStr = mediaUrl.toString();
        QAction *viewImg = menu.addAction(ztr("View Image"));
        connect(viewImg, &QAction::triggered, this, [this, mediaStr]() {
            navigateTo(mediaStr);
        });
        QAction *copyImg = menu.addAction(ztr("Copy Image Location"));
        connect(copyImg, &QAction::triggered, this, [mediaStr]() {
            if (QClipboard *cb = QApplication::clipboard())
                cb->setText(mediaStr);
        });
        QAction *saveImg = menu.addAction(ztr("Save Image As..."));
        connect(saveImg, &QAction::triggered, this, [page, mediaUrl]() {
            page->download(mediaUrl);
        });
        menu.addSeparator();
    }

    if (request && request->isContentEditable()) {
        QAction *undoAct = menu.addAction(ztr("Undo"));
        connect(undoAct, &QAction::triggered, this, [page]() { page->triggerAction(QWebEnginePage::Undo); });
        QAction *cutAct = menu.addAction(ztr("Cut"));
        connect(cutAct, &QAction::triggered, this, [page]() { page->triggerAction(QWebEnginePage::Cut); });
        QAction *copyAct = menu.addAction(ztr("Copy"));
        connect(copyAct, &QAction::triggered, this, [page]() { page->triggerAction(QWebEnginePage::Copy); });
        QAction *pasteAct = menu.addAction(ztr("Paste"));
        connect(pasteAct, &QAction::triggered, this, [page]() { page->triggerAction(QWebEnginePage::Paste); });
        QAction *selAllAct = menu.addAction(ztr("Select All"));
        connect(selAllAct, &QAction::triggered, this, [page]() { page->triggerAction(QWebEnginePage::SelectAll); });
        menu.addSeparator();
    } else if (request && !request->selectedText().isEmpty()) {
        QAction *copySel = menu.addAction(ztr("Copy"));
        connect(copySel, &QAction::triggered, this, [page]() { page->triggerAction(QWebEnginePage::Copy); });
        menu.addSeparator();
    }

    QAction *backAct = menu.addAction(ztr("Back"));
    backAct->setEnabled(view->history()->canGoBack());
    connect(backAct, &QAction::triggered, view, &QWebEngineView::back);

    QAction *fwdAct = menu.addAction(ztr("Forward"));
    fwdAct->setEnabled(view->history()->canGoForward());
    connect(fwdAct, &QAction::triggered, view, &QWebEngineView::forward);

    QAction *reloadAct = menu.addAction(ztr("Reload"));
    connect(reloadAct, &QAction::triggered, view, &QWebEngineView::reload);

    QAction *stopAct = menu.addAction(ztr("Stop"));
    const BrowserTab *tab = currentTab();
    stopAct->setEnabled(tab && tab->isLoading);
    connect(stopAct, &QAction::triggered, view, &QWebEngineView::stop);

    menu.addSeparator();
    QAction *bmPage = menu.addAction(ztr("Add Bookmark"));
    connect(bmPage, &QAction::triggered, this, &NetzacWindow::addCurrentBookmark);
    QAction *saveAs = menu.addAction(ztr("Save As..."));
    connect(saveAs, &QAction::triggered, this, &NetzacWindow::savePage);
    QAction *srcAct = menu.addAction(ztr("Page Source"));
    connect(srcAct, &QAction::triggered, this, &NetzacWindow::viewPageSource);
    QAction *infoAct = menu.addAction(ztr("Page Info"));
    connect(infoAct, &QAction::triggered, this, &NetzacWindow::showPageInfo);

    menu.exec(globalPos);
}

void NetzacWindow::updateNavStates()
{
    const BrowserTab *tab = currentTab();
    QWebEngineHistory *hist = (tab && tab->view) ? tab->view->history() : nullptr;
    const bool loading = tab && tab->isLoading;
    m_btnBack->setEnabled(hist && hist->canGoBack());
    m_btnForward->setEnabled(hist && hist->canGoForward());
    m_btnStop->setEnabled(loading);
    if (m_actStopMenu)
        m_actStopMenu->setEnabled(loading);
}

void NetzacWindow::navigateTo(const QString &url)
{
    BrowserTab *tab = currentTab();
    if (!tab || !tab->view || !tab->page)
        return;

    QString raw = url.trimmed();
    if (raw.isEmpty())
        return;
    if (raw.startsWith(QLatin1String("about:"))) {
        loadBuiltinAbout(tab, raw);
        return;
    }
    if (!raw.startsWith(QLatin1String("http://"))
        && !raw.startsWith(QLatin1String("https://"))
        && !raw.startsWith(QLatin1String("file://"))) {
        const bool isLocalhost = raw.compare(QLatin1String("localhost"), Qt::CaseInsensitive) == 0
            || raw.startsWith(QLatin1String("localhost/"), Qt::CaseInsensitive)
            || raw.startsWith(QLatin1String("localhost:"), Qt::CaseInsensitive);
        if (raw.contains(QLatin1Char(' '))
            || (!raw.contains(QLatin1Char('.')) && !raw.contains(QLatin1Char(':')) && !isLocalhost)) {
            raw = m_prefs.searchUrlFor(raw);
        } else {
            raw.prepend(QStringLiteral("https://"));
        }
    }
    tab->showingBuiltin = false;
    tab->page->setBuiltInPage(false);
    tab->currentUrl = raw;
    tab->view->setUrl(QUrl(raw));
}

void NetzacWindow::buildNavHistoryMenu(QMenu &menu, bool back)
{
    NetzacWebView *view = webView();
    if (!view)
        return;
    QWebEngineHistory *hist = view->history();
    const QList<QWebEngineHistoryItem> items = back ? hist->backItems(15) : hist->forwardItems(15);
    if (back) {
        for (int i = int(items.size()) - 1; i >= 0; --i) {
            const QWebEngineHistoryItem &item = items.at(i);
            QString label = item.title().isEmpty() ? item.url().toString() : item.title();
            if (label.size() > 48)
                label = label.left(45) + QStringLiteral("...");
            QAction *act = menu.addAction(label);
            act->setToolTip(item.url().toString());
            connect(act, &QAction::triggered, this, [hist, item]() { hist->goToItem(item); });
        }
    } else {
        for (const QWebEngineHistoryItem &item : items) {
            QString label = item.title().isEmpty() ? item.url().toString() : item.title();
            if (label.size() > 48)
                label = label.left(45) + QStringLiteral("...");
            QAction *act = menu.addAction(label);
            act->setToolTip(item.url().toString());
            connect(act, &QAction::triggered, this, [hist, item]() { hist->goToItem(item); });
        }
    }
}

void NetzacWindow::loadBuiltinAbout(BrowserTab *tab, const QString &aboutUrlInput)
{
    if (!tab || !tab->view || !tab->page)
        return;

    QString aboutUrl = aboutUrlInput;
    if (aboutUrl == QLatin1String("about:netscape"))
        aboutUrl = QStringLiteral("about:navigator");

    tab->showingBuiltin = true;
    tab->page->setBuiltInPage(true);
    tab->currentUrl = aboutUrl;
    if (tab == currentTab()) {
        m_urlBar->setText(aboutUrl);
        m_locationLabel->setText(ztr("Location:"));
        m_statusSecurity->setIconName(QStringLiteral("security_open"));
    }

    if (aboutUrl == QLatin1String("about:navigator") || aboutUrl == QLatin1String("about:netzac")) {
        const QString title = QStringLiteral("NetZac");
        tab->title = title;
        const int idx = indexOfView(tab->view);
        if (idx >= 0) {
            m_tabBar->setTabTitle(idx, title);
            m_tabBar->setTabUrl(idx, aboutUrl);
        }
        if (tab == currentTab())
            setWindowTitle(ztr("Netzac: %1").arg(title));
        tab->view->setHtml(
            navigatorStartHtmlPage(
                title,
                ztr("Search with DuckDuckGo..."),
                ztr("Search")),
            QUrl(QStringLiteral("https://localhost/")));
        return;
    }

    QString title;
    QString subtitle;
    QString body;

    if (aboutUrl == QLatin1String("about:mail")) {
        title = ztr("Netzac - Mail");
        subtitle = ztr("Navigator Mail Quick Link");
        body = ztr(
            "<p>To configure this button to open your mail link (for example Gmail or webmail), "
            "open <b>Edit &rsaquo; Preferences &rsaquo; Quick Links</b> and set the <b>Mail URL</b>.</p>");
    } else if (aboutUrl == QLatin1String("about:chat") || aboutUrl == QLatin1String("about:news")) {
        aboutUrl = QStringLiteral("about:chat");
        tab->currentUrl = aboutUrl;
        if (tab == currentTab())
            m_urlBar->setText(aboutUrl);
        title = ztr("Netzac - Chat");
        subtitle = ztr("Navigator Chat Quick Link");
        body = ztr(
            "<p>To configure this button to open your chat service, "
            "open <b>Edit &rsaquo; Preferences &rsaquo; Quick Links</b> and set the <b>Chat URL</b>.</p>");
    } else if (aboutUrl == QLatin1String("about:git") || aboutUrl == QLatin1String("about:composer")) {
        aboutUrl = QStringLiteral("about:git");
        tab->currentUrl = aboutUrl;
        if (tab == currentTab())
            m_urlBar->setText(aboutUrl);
        title = ztr("Netzac - Git");
        subtitle = ztr("Navigator Git Quick Link");
        body = ztr(
            "<p>To configure this button to open your Git forge (for example GitHub, GitLab or Gitea), "
            "open <b>Edit &rsaquo; Preferences &rsaquo; Quick Links</b> and set the <b>Git URL</b>.</p>");
    } else if (aboutUrl == QLatin1String("about:help")) {
        title = ztr("Netzac Navigator Help");
        subtitle = ztr("Keyboard Shortcuts &amp; Toolbar Guide");
        body = ztr(
            "<ul class=\"shortcut-list\">"
            "<li><span><b>Collapse Toolbars</b></span><span>Click the ribbed grip on the left of any toolbar</span></li>"
            "<li><span><b>Open Page</b></span><span><code>Ctrl+O</code></span></li>"
            "<li><span><b>New / Close Tab</b></span><span><code>Ctrl+T</code> / <code>Ctrl+W</code></span></li>"
            "<li><span><b>Find in Page / Find Again</b></span><span><code>Ctrl+F</code> or <code>F4</code> / <code>Ctrl+G</code></span></li>"
            "<li><span><b>Zoom In / Out / Reset</b></span><span><code>Ctrl++</code> / <code>Ctrl+-</code> / <code>Ctrl+0</code></span></li>"
            "<li><span><b>Add / Manage Bookmarks</b></span><span><code>Ctrl+D</code> / <code>Ctrl+B</code></span></li>"
            "<li><span><b>Reload / Stop</b></span><span><code>Ctrl+R</code> or <code>F5</code> / <code>Esc</code></span></li>"
            "<li><span><b>Full Screen</b></span><span><code>F11</code></span></li>"
            "<li><span><b>Calm Mode</b></span><span><code>Ctrl+Shift+C</code></span></li>"
            "<li><span><b>Page Source / Info</b></span><span><code>Ctrl+U</code> / <code>Ctrl+I</code></span></li>"
            "</ul>");
    } else {
        title = ztr("About Netzac");
        subtitle = aboutUrl;
        body = ztr("<p>Unknown built-in Navigator location.</p>");
    }

    tab->title = title;
    const int idx = indexOfView(tab->view);
    if (idx >= 0) {
        m_tabBar->setTabTitle(idx, title);
        m_tabBar->setTabUrl(idx, aboutUrl);
    }
    if (tab == currentTab())
        setWindowTitle(ztr("Netzac: %1").arg(title));
    tab->view->setHtml(retroHtmlPage(title, subtitle, body), QUrl(QStringLiteral("https://localhost/")));
}

void NetzacWindow::showBookmarksPopup()
{
    QMenu menu(this);
    populateBookmarksMenu(&menu);
    menu.exec(m_btnBookmarks->mapToGlobal(QPoint(0, m_btnBookmarks->height())));
}

void NetzacWindow::addCurrentBookmark()
{
    const BrowserTab *tab = currentTab();
    if (!tab || !tab->view)
        return;
    const QString url = tab->currentUrl;
    const QString title = tab->view->title().isEmpty() ? url : tab->view->title();
    bool exists = false;
    for (const auto &entry : m_bookmarks) {
        if (entry.second == url) {
            exists = true;
            break;
        }
    }
    if (!exists) {
        m_bookmarks.append({title, url});
        BookmarkStorage::save(m_bookmarks);
    }
    m_statusLabel->setStatusText(ztr("Bookmarked: %1").arg(title));
}

void NetzacWindow::manageBookmarks()
{
    BookmarksDialog dlg(&m_bookmarks, this);
    connect(&dlg, &BookmarksDialog::openRequested, this, &NetzacWindow::navigateTo);
    connect(&dlg, &BookmarksDialog::bookmarksModified, this, [this]() {
        BookmarkStorage::save(m_bookmarks);
    });
    dlg.exec();
}

void NetzacWindow::newWindow()
{
    auto *win = new NetzacWindow();
    win->setAttribute(Qt::WA_DeleteOnClose);
    win->show();
}

void NetzacWindow::openLocationDialog()
{
    PlatinumDialog dlg(this);
    dlg.setWindowTitle(ztr("Open Page"));
    dlg.setFixedWidth(400);

    auto *lay = new QVBoxLayout(&dlg);
    lay->setContentsMargins(12, 12, 12, 12);
    lay->setSpacing(8);
    lay->addWidget(new QLabel(ztr("Type the Internet location (URL) or choose a local file:"), &dlg));

    const BrowserTab *tab = currentTab();
    auto *edit = new QLineEdit(tab ? tab->currentUrl : QString(), &dlg);
    edit->selectAll();
    lay->addWidget(edit);

    auto *row = new QHBoxLayout();
    QPushButton *chooseBtn = makeDialogButton(ztr("Choose File..."), false, &dlg);
    connect(chooseBtn, &QPushButton::clicked, &dlg, [&dlg, edit]() {
        const QString path = platinumOpenFileName(
            &dlg,
            ztr("Open Local HTML File"),
            QString(),
            ztr("HTML Files (*.html *.htm);;All Files (*)"));
        if (!path.isEmpty())
            edit->setText(QUrl::fromLocalFile(path).toString());
    });
    QPushButton *cancelBtn = makeDialogButton(ztr("Cancel"), false, &dlg);
    connect(cancelBtn, &QPushButton::clicked, &dlg, &QDialog::reject);
    QPushButton *okBtn = makeDialogButton(ztr("OK"), true, &dlg);
    connect(okBtn, &QPushButton::clicked, &dlg, &QDialog::accept);
    row->addWidget(chooseBtn);
    row->addStretch(1);
    row->addWidget(cancelBtn);
    row->addWidget(okBtn);
    lay->addLayout(row);

    if (dlg.exec() == QDialog::Accepted)
        navigateTo(edit->text());
}

void NetzacWindow::savePage()
{
    NetzacWebPage *page = webPage();
    if (!page)
        return;
    const QString path = platinumSaveFileName(
        this,
        ztr("Save Page As"),
        QStringLiteral("page.html"),
        ztr("HTML Files (*.html *.htm);;All Files (*)"));
    if (!path.isEmpty()) {
        page->toHtml([this, path](const QString &htmlText) {
            writeSavedHtml(path, htmlText);
        });
    }
}

void NetzacWindow::writeSavedHtml(const QString &path, const QString &htmlText)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        PlatinumAlertDialog(ztr("Save Error"), f.errorString(), QStringLiteral("warning"), this).exec();
        return;
    }
    QTextStream out(&f);
    out << htmlText;
    f.close();
    m_statusLabel->setStatusText(ztr("Saved: %1").arg(path));
}

void NetzacWindow::printPage()
{
    NetzacWebPage *page = webPage();
    if (!page)
        return;
    const QString path = platinumSaveFileName(
        this,
        ztr("Print Page to PDF"),
        QStringLiteral("netzac-page.pdf"),
        ztr("PDF Files (*.pdf)"));
    if (!path.isEmpty()) {
        page->printToPdf(path);
        m_statusLabel->setStatusText(ztr("Printed PDF: %1").arg(path));
    }
}

void NetzacWindow::showFindBar()
{
    m_findBar->show();
    m_findInput->setFocus();
    m_findInput->selectAll();
}

void NetzacWindow::hideFindBar()
{
    m_findBar->hide();
    if (NetzacWebView *view = webView())
        view->findText(QString());
}

void NetzacWindow::findNext()
{
    NetzacWebView *view = webView();
    if (!view)
        return;
    const QString query = m_findInput->text();
    if (!query.isEmpty()) {
        m_lastSearch = query;
        view->findText(query);
    } else if (!m_lastSearch.isEmpty()) {
        view->findText(m_lastSearch);
    }
}

void NetzacWindow::viewPageSource()
{
    const BrowserTab *tab = currentTab();
    if (!tab || !tab->page)
        return;
    const QString sourceUrl = tab->currentUrl;
    tab->page->toHtml([this, sourceUrl](const QString &sourceText) {
        PlatinumDialog dlg(this);
        dlg.setWindowTitle(ztr("Source of: %1").arg(sourceUrl));
        dlg.resize(680, 480);

        auto *lay = new QVBoxLayout(&dlg);
        lay->setContentsMargins(10, 10, 10, 10);
        lay->setSpacing(8);

        auto *view = new QPlainTextEdit(&dlg);
        view->setReadOnly(true);
        view->setFont(QFont(QStringLiteral("Nimbus Mono PS"), 11));
        view->setPlainText(sourceText);
        lay->addWidget(view, 1);

        auto *btnRow = new QHBoxLayout();
        btnRow->addStretch(1);
        QPushButton *closeBtn = makeDialogButton(ztr("Close"), true, &dlg);
        connect(closeBtn, &QPushButton::clicked, &dlg, &QDialog::accept);
        btnRow->addWidget(closeBtn);
        lay->addLayout(btnRow);
        dlg.exec();
    });
}

void NetzacWindow::showPageInfo()
{
    const BrowserTab *tab = currentTab();
    if (!tab || !tab->view)
        return;
    const QString url = tab->currentUrl;
    const QString title = tab->view->title().isEmpty() ? ztr("(Untitled)") : tab->view->title();
    QString sec;
    if (url.startsWith(QLatin1String("https://")))
        sec = ztr("Encrypted connection (HTTPS) — Security padlock is locked.");
    else if (url.startsWith(QLatin1String("about:")))
        sec = ztr("Built-in Netzac Navigator local document.");
    else
        sec = ztr("Unencrypted connection (HTTP) — Information sent over this connection is not encrypted.");

    PlatinumAlertDialog(
        ztr("Page & Security Info"),
        ztr("Title: %1\nLocation: %2\n\nSecurity: %3").arg(title, url, sec),
        QStringLiteral("info"),
        this)
        .exec();
}

void NetzacWindow::showHistoryDialog()
{
    PlatinumDialog dlg(this);
    dlg.setWindowTitle(ztr("History"));
    dlg.resize(560, 380);

    auto *lay = new QVBoxLayout(&dlg);
    lay->setContentsMargins(10, 10, 10, 10);
    lay->setSpacing(8);

    auto *searchRow = new QHBoxLayout();
    searchRow->addWidget(new QLabel(ztr("Search:"), &dlg));
    auto *filterEdit = new QLineEdit(&dlg);
    filterEdit->setPlaceholderText(ztr("Filter by title or URL..."));
    filterEdit->setFont(zacosFont(QStringLiteral("ZacOS 9 System"), 12));
    searchRow->addWidget(filterEdit, 1);
    lay->addLayout(searchRow);

    auto *lst = new QListWidget(&dlg);
    const auto populateList = [this, lst](const QString &query) {
        lst->clear();
        const QString q = query.trimmed();
        for (const HistoryEntry &entry : m_history) {
            const QString title = entry.title.isEmpty() ? ztr("(Untitled)") : entry.title;
            if (!q.isEmpty()
                && !title.contains(q, Qt::CaseInsensitive)
                && !entry.url.contains(q, Qt::CaseInsensitive)) {
                continue;
            }
            QString dateStr;
            if (entry.visitedAt > 0) {
                dateStr = QDateTime::fromSecsSinceEpoch(entry.visitedAt)
                              .toString(QStringLiteral("yyyy-MM-dd hh:mm"));
            }
            const QString label = dateStr.isEmpty()
                ? QStringLiteral("%1  —  %2").arg(title, entry.url)
                : QStringLiteral("[%1]  %2  —  %3").arg(dateStr, title, entry.url);
            auto *rowItem = new QListWidgetItem(label);
            rowItem->setData(Qt::UserRole, entry.url);
            lst->addItem(rowItem);
        }
    };
    populateList(QString());

    connect(filterEdit, &QLineEdit::textChanged, &dlg, populateList);
    connect(lst, &QListWidget::itemDoubleClicked, &dlg, [this, &dlg](QListWidgetItem *item) {
        if (item) {
            navigateTo(item->data(Qt::UserRole).toString());
            dlg.accept();
        }
    });
    lay->addWidget(lst, 1);

    auto *btnRow = new QHBoxLayout();
    QPushButton *openBtn = makeDialogButton(ztr("Open"), false, &dlg);
    connect(openBtn, &QPushButton::clicked, &dlg, [this, &dlg, lst]() {
        if (QListWidgetItem *item = lst->currentItem()) {
            navigateTo(item->data(Qt::UserRole).toString());
            dlg.accept();
        }
    });
    QPushButton *clearBtn = makeDialogButton(ztr("Clear History"), false, &dlg);
    connect(clearBtn, &QPushButton::clicked, &dlg, [this, populateList, filterEdit]() {
        m_history.clear();
        HistoryStorage::save(m_history);
        populateList(filterEdit->text());
    });
    btnRow->addWidget(openBtn);
    btnRow->addWidget(clearBtn);
    btnRow->addStretch(1);
    QPushButton *closeBtn = makeDialogButton(ztr("Close"), true, &dlg);
    connect(closeBtn, &QPushButton::clicked, &dlg, &QDialog::accept);
    btnRow->addWidget(closeBtn);
    lay->addLayout(btnRow);
    dlg.exec();
}

void NetzacWindow::openPreferences()
{
    const BrowserTab *cur = currentTab();
    PreferencesDialog dlg(m_prefs, cur ? cur->currentUrl : QString(), this);
    if (dlg.exec() == QDialog::Accepted) {
        m_prefs = dlg.preferences();
        m_prefs.save();
        m_prefs.apply(netzacProfile());
        if (dlg.didResetAll()) {
            m_bookmarks = BookmarkStorage::load();
            m_history = HistoryStorage::load();
            m_visitedUrls.clear();
            for (const auto &entry : m_bookmarks) {
                if (!m_visitedUrls.contains(entry.second))
                    m_visitedUrls.append(entry.second);
            }
            if (m_actLinkModeTabs)
                m_actLinkModeTabs->setChecked(m_prefs.linkOpenMode == NetzacPreferences::LinkOpenMode::Tabs);
            if (m_actLinkModeWindows)
                m_actLinkModeWindows->setChecked(m_prefs.linkOpenMode == NetzacPreferences::LinkOpenMode::Windows);
            if (m_actTabStyleLeft)
                m_actTabStyleLeft->setChecked(m_prefs.tabBarStyle == NetzacPreferences::TabBarStyle::IE45Left);
            if (m_actTabStyleTop)
                m_actTabStyleTop->setChecked(m_prefs.tabBarStyle == NetzacPreferences::TabBarStyle::ChromeTop);
            if (m_actShowNav) m_actShowNav->setChecked(true);
            if (m_actShowLoc) m_actShowLoc->setChecked(true);
            if (m_actShowPers) m_actShowPers->setChecked(true);
            if (m_actShowStatus) m_actShowStatus->setChecked(true);
            while (m_tabs.size() > 1)
                closeTab(int(m_tabs.size()) - 1);
            updateTabBarPlacement();
            navigateTo(m_prefs.homeUrl);
        }
        for (BrowserTab &tab : m_tabs) {
            if (tab.page) {
                tab.page->settings()->setAttribute(
                    QWebEngineSettings::AutoLoadImages, m_prefs.autoloadImages);
                tab.page->settings()->setAttribute(
                    QWebEngineSettings::JavascriptEnabled, m_prefs.enableJavaScript);
            }
            if (tab.calm)
                tab.calm->setConfig(m_prefs.calm);
        }
        applyNavToolbarCompact(m_prefs.showNavButtonText);
        if (m_actShowNavText) {
            const QSignalBlocker b(m_actShowNavText);
            m_actShowNavText->setChecked(m_prefs.showNavButtonText);
        }
        rebuildPersonalToolbar();
    }
}

void NetzacWindow::installPasswordHelpers(NetzacWebPage *page, const QUrl &url)
{
    if (!page || !url.isValid())
        return;
    const QString scheme = url.scheme().toLower();
    if (scheme != QLatin1String("http") && scheme != QLatin1String("https"))
        return;
    const QString host = url.host().trimmed().toLower();
    if (host.isEmpty() || !PasswordStorage::isAvailable())
        return;

    PasswordStorage::Credential cred;
    QString fillJson = QStringLiteral("null");
    if (PasswordStorage::lookupCredential(host, &cred)) {
        QJsonObject obj;
        obj.insert(QStringLiteral("user"), cred.username);
        obj.insert(QStringLiteral("pass"), cred.password);
        fillJson = QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact));
    }

    const QString hostJson = QString::fromUtf8(
        QJsonDocument(QJsonObject{{QStringLiteral("host"), host}}).toJson(QJsonDocument::Compact));

    const QString script = QStringLiteral(
        "(function(){"
        "var creds = %1;"
        "var host = (%2).host;"
        "function fillForm(f) {"
        "  if (!creds) return;"
        "  var pw = f.querySelector('input[type=\"password\"]');"
        "  if (!pw) return;"
        "  var inputs = Array.prototype.slice.call(f.querySelectorAll('input'));"
        "  var userEl = null;"
        "  for (var i = 0; i < inputs.length; i++) {"
        "    if (inputs[i] === pw) break;"
        "    var t = (inputs[i].type || 'text').toLowerCase();"
        "    if (t === 'text' || t === 'email' || t === 'tel') userEl = inputs[i];"
        "  }"
        "  if (userEl && !userEl.value) {"
        "    userEl.value = creds.user;"
        "    userEl.dispatchEvent(new Event('input', {bubbles:true}));"
        "  }"
        "  if (!pw.value) {"
        "    pw.value = creds.pass;"
        "    pw.dispatchEvent(new Event('input', {bubbles:true}));"
        "  }"
        "}"
        "var forms = document.querySelectorAll('form');"
        "for (var i = 0; i < forms.length; i++) {"
        "  var f = forms[i];"
        "  fillForm(f);"
        "  if (f.dataset.netzacPwHooked) continue;"
        "  if (!f.querySelector('input[type=\"password\"]')) continue;"
        "  f.dataset.netzacPwHooked = '1';"
        "  f.addEventListener('submit', function() {"
        "    var pw = this.querySelector('input[type=\"password\"]');"
        "    if (!pw || !pw.value) return;"
        "    var inputs = Array.prototype.slice.call(this.querySelectorAll('input'));"
        "    var userVal = '';"
        "    for (var j = 0; j < inputs.length; j++) {"
        "      if (inputs[j] === pw) break;"
        "      var t = (inputs[j].type || 'text').toLowerCase();"
        "      if ((t === 'text' || t === 'email' || t === 'tel') && inputs[j].value) userVal = inputs[j].value;"
        "    }"
        "    if (userVal) {"
        "      console.debug('__NETZAC_CRED__:' + JSON.stringify({host: host, user: userVal, pass: pw.value}));"
        "    }"
        "  }, true);"
        "}"
        "})();").arg(fillJson, hostJson);

    page->runJavaScript(script, QWebEngineScript::ApplicationWorld);
}

void NetzacWindow::handleCredentialSubmitted(const QString &host, const QString &username, const QString &password)
{
    if (!m_prefs.rememberPasswords || !PasswordStorage::isAvailable())
        return;

    PasswordStorage::Credential existing;
    if (PasswordStorage::lookupCredential(host, &existing)
        && existing.username == username
        && existing.password == password) {
        return;
    }

    PlatinumDialog dlg(this);
    dlg.setWindowTitle(ztr("Save Password"));
    dlg.setFixedWidth(380);
    auto *lay = new QVBoxLayout(&dlg);
    lay->setContentsMargins(12, 12, 12, 12);
    lay->setSpacing(10);

    auto *top = new QHBoxLayout();
    auto *iconLbl = new QLabel(&dlg);
    iconLbl->setPixmap(makeAlertIcon(QStringLiteral("info"), 32));
    iconLbl->setAlignment(Qt::AlignTop);
    auto *msgLbl = new QLabel(
        ztr("Do you want Netzac to save the password for \"%1\" on %2 in ZacOS Keychain?")
            .arg(username, host),
        &dlg);
    msgLbl->setWordWrap(true);
    top->addWidget(iconLbl);
    top->addWidget(msgLbl, 1);
    lay->addLayout(top);

    auto *btnRow = new QHBoxLayout();
    btnRow->addStretch(1);
    QPushButton *noBtn = makeDialogButton(ztr("Not Now"), false, &dlg);
    QPushButton *yesBtn = makeDialogButton(ztr("Save Password"), true, &dlg);
    yesBtn->setMinimumWidth(130);
    connect(noBtn, &QPushButton::clicked, &dlg, &QDialog::reject);
    connect(yesBtn, &QPushButton::clicked, &dlg, &QDialog::accept);
    btnRow->addWidget(noBtn);
    btnRow->addWidget(yesBtn);
    lay->addLayout(btnRow);

    if (dlg.exec() == QDialog::Accepted) {
        PasswordStorage::saveCredential(host, username, password);
        if (m_statusLabel)
            m_statusLabel->setStatusText(ztr("Saved password for %1 in ZacOS Keychain").arg(host));
    }
}

void NetzacWindow::showAbout()
{
    AboutDialog(this).exec();
}
