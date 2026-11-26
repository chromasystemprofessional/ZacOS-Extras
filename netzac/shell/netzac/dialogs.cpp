#include "dialogs.h"

#include "adblocker.h"
#include "bookmarks.h"
#include "i18n.h"
#include "netzacwindow.h"
#include "platinum.h"
#include "widgets.h"

#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QDateTime>
#include <QDesktopServices>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QRegularExpression>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QRadioButton>
#include <QSizePolicy>
#include <QSpinBox>
#include <QStackedWidget>
#include <QUrl>
#include <QVBoxLayout>
#include <QWebEngineCookieStore>
#include <QWebEngineProfile>

QPushButton *makeDialogButton(const QString &text, bool isDefault, QWidget *parent)
{
    auto *btn = new QPushButton(text, parent);
    btn->setIcon(QIcon());
    btn->setFont(zacosFont(QStringLiteral("ZacOS 9 System"), 12));
    btn->setMinimumWidth(78);
    btn->setFixedHeight(isDefault ? 28 : 24);
    btn->setAutoDefault(isDefault);
    btn->setDefault(isDefault);
    return btn;
}

namespace {

QString runPlatinumFileDialog(
    QWidget *parent,
    const QString &caption,
    const QString &dirOrFile,
    const QString &filter,
    bool saveMode)
{
    QFileDialog dlg(parent, caption);
    dlg.setOption(QFileDialog::DontUseNativeDialog, true);
    dlg.setAcceptMode(saveMode ? QFileDialog::AcceptSave : QFileDialog::AcceptOpen);
    dlg.setFileMode(saveMode ? QFileDialog::AnyFile : QFileDialog::ExistingFile);
    if (!filter.isEmpty())
        dlg.setNameFilter(filter);
    if (!dirOrFile.isEmpty()) {
        const QFileInfo fi(dirOrFile);
        if (fi.isDir()) {
            dlg.setDirectory(fi.absoluteFilePath());
        } else {
            if (!fi.path().isEmpty() && fi.path() != QLatin1String("."))
                dlg.setDirectory(fi.path());
            dlg.selectFile(fi.fileName());
        }
    }
    if (QApplication *app = qobject_cast<QApplication *>(QApplication::instance())) {
        dlg.setStyle(app->style());
        dlg.setPalette(app->palette());
    }
    dlg.setFont(zacosFont(QStringLiteral("ZacOS 9 System"), 12));
    if (dlg.exec() != QDialog::Accepted)
        return QString();
    const QStringList files = dlg.selectedFiles();
    return files.isEmpty() ? QString() : files.first();
}

}  // namespace

QString platinumOpenFileName(
    QWidget *parent,
    const QString &caption,
    const QString &dir,
    const QString &filter)
{
    return runPlatinumFileDialog(parent, caption, dir, filter, false);
}

QString platinumSaveFileName(
    QWidget *parent,
    const QString &caption,
    const QString &dirOrFile,
    const QString &filter)
{
    return runPlatinumFileDialog(parent, caption, dirOrFile, filter, true);
}

PlatinumDialog::PlatinumDialog(QWidget *parent)
    : QDialog(parent)
{
    if (QApplication *app = qobject_cast<QApplication *>(QApplication::instance())) {
        setStyle(app->style());
        setPalette(app->palette());
    }
    setFont(zacosFont(QStringLiteral("ZacOS 9 System"), 12));
}

PlatinumAlertDialog::PlatinumAlertDialog(
    const QString &title,
    const QString &message,
    const QString &kind,
    QWidget *parent)
    : PlatinumDialog(parent)
{
    setWindowTitle(title);
    setMinimumWidth(400);

    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(14, 14, 14, 12);
    lay->setSpacing(12);

    auto *top = new QHBoxLayout();
    top->setSpacing(12);
    auto *iconLbl = new QLabel(this);
    iconLbl->setPixmap(makeAlertIcon(kind, 32));
    iconLbl->setFixedSize(32, 32);
    top->addWidget(iconLbl, 0, Qt::AlignTop);

    auto *msgLbl = new QLabel(message, this);
    msgLbl->setFont(zacosFont(QStringLiteral("ZacOS 9 System"), 12));
    msgLbl->setWordWrap(true);
    msgLbl->setMinimumWidth(320);
    msgLbl->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    top->addWidget(msgLbl, 1);
    lay->addLayout(top);

    auto *btnRow = new QHBoxLayout();
    btnRow->addStretch(1);
    QPushButton *okBtn = makeDialogButton(ztr("OK"), true, this);
    connect(okBtn, &QPushButton::clicked, this, &QDialog::accept);
    btnRow->addWidget(okBtn);
    lay->addLayout(btnRow);
    adjustSize();
}

AboutDialog::AboutDialog(QWidget *parent)
    : PlatinumDialog(parent)
{
    setWindowTitle(ztr("About Netzac"));
    setFixedSize(460, 310);

    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(14, 14, 14, 12);
    lay->setSpacing(10);

    auto *top = new QHBoxLayout();
    top->setSpacing(12);
    auto *badge = new NetzacThrobber(this);
    badge->setEnabled(false);
    badge->setFixedSize(56, 56);
    top->addWidget(badge, 0, Qt::AlignTop);

    auto *info = new QVBoxLayout();
    auto *title = new QLabel(ztr("Netzac Navigator"), this);
    title->setFont(zacosFont(QStringLiteral("ZacOS 9 System"), 12, true));
    const QString repoUrl = QStringLiteral("https://github.com/guimaraeslucas/netzac");
    const QString rawAbout = ztr(
        "Netzac Navigator (ZacOS 9)\n\n"
        "Developed by Lucas Guimarães - G3Pix (@guimaraeslucas). We count on the support of the "
        "ZacOS 9 community for contributions at:\n"
        "%1\n\n"
        "Special thanks to Adam Lawson (@chromasystemprofessional) for creating "
        "the incredible ZacOS 9 project.");
    const QString htmlAbout = rawAbout.toHtmlEscaped()
        .replace(QLatin1Char('\n'), QStringLiteral("<br>"))
        .arg(QStringLiteral("<a href=\"%1\">%1</a>").arg(repoUrl));
    auto *sub = new QLabel(htmlAbout, this);
    sub->setFont(zacosFont(QStringLiteral("ZacOS 9 Views"), 9));
    sub->setTextFormat(Qt::RichText);
    sub->setWordWrap(true);
    sub->setTextInteractionFlags(Qt::TextBrowserInteraction);
    sub->setOpenExternalLinks(false);
    connect(sub, &QLabel::linkActivated, this, [this](const QString &url) {
        if (auto *win = qobject_cast<NetzacWindow *>(parentWidget())) {
            win->newTab(url, true);
            accept();
        } else {
            QDesktopServices::openUrl(QUrl(url));
        }
    });
    info->addWidget(title);
    info->addWidget(sub);
    top->addLayout(info, 1);
    lay->addLayout(top);

    auto *line = new QFrame(this);
    line->setFrameShape(QFrame::HLine);
    lay->addWidget(line);

    auto *btnRow = new QHBoxLayout();
    btnRow->addStretch(1);
    QPushButton *okBtn = makeDialogButton(ztr("OK"), true, this);
    connect(okBtn, &QPushButton::clicked, this, &QDialog::accept);
    btnRow->addWidget(okBtn);
    lay->addLayout(btnRow);
}

BookmarksDialog::BookmarksDialog(QVector<QPair<QString, QString>> *bookmarks, QWidget *parent)
    : PlatinumDialog(parent)
    , m_bookmarks(bookmarks)
{
    setWindowTitle(ztr("Bookmarks"));
    resize(440, 300);

    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(10, 10, 10, 10);
    lay->setSpacing(8);

    m_list = new QListWidget(this);
    refresh();
    connect(m_list, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *) {
        openSelected();
    });
    lay->addWidget(m_list, 1);

    auto *row = new QHBoxLayout();
    QPushButton *openBtn = makeDialogButton(ztr("Open"), false, this);
    connect(openBtn, &QPushButton::clicked, this, &BookmarksDialog::openSelected);
    QPushButton *removeBtn = makeDialogButton(ztr("Delete"), false, this);
    connect(removeBtn, &QPushButton::clicked, this, &BookmarksDialog::deleteSelected);
    QPushButton *importBtn = makeDialogButton(ztr("Import..."), false, this);
    connect(importBtn, &QPushButton::clicked, this, &BookmarksDialog::importBookmarks);
    QPushButton *exportBtn = makeDialogButton(ztr("Export..."), false, this);
    connect(exportBtn, &QPushButton::clicked, this, &BookmarksDialog::exportBookmarks);
    QPushButton *closeBtn = makeDialogButton(ztr("Close"), true, this);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    row->addWidget(openBtn);
    row->addWidget(removeBtn);
    row->addWidget(importBtn);
    row->addWidget(exportBtn);
    row->addStretch(1);
    row->addWidget(closeBtn);
    lay->addLayout(row);
}

void BookmarksDialog::refresh()
{
    m_list->clear();
    if (!m_bookmarks)
        return;
    for (const auto &entry : *m_bookmarks) {
        auto *item = new QListWidgetItem(
            QStringLiteral("%1  —  %2").arg(entry.first, entry.second));
        item->setData(Qt::UserRole, entry.second);
        m_list->addItem(item);
    }
}

void BookmarksDialog::openSelected()
{
    QListWidgetItem *item = m_list->currentItem();
    if (item) {
        emit openRequested(item->data(Qt::UserRole).toString());
        accept();
    }
}

void BookmarksDialog::deleteSelected()
{
    if (!m_bookmarks)
        return;
    const int idx = m_list->currentRow();
    if (idx >= 0 && idx < m_bookmarks->size()) {
        m_bookmarks->removeAt(idx);
        refresh();
        emit bookmarksModified();
    }
}

void BookmarksDialog::importBookmarks()
{
    if (!m_bookmarks)
        return;
    const QString path = platinumOpenFileName(
        this,
        ztr("Import Bookmarks (Chrome / Firefox)"),
        QString(),
        ztr("Bookmark Files (*.html *.htm Bookmarks *.json);;All Files (*)"));
    if (path.isEmpty())
        return;

    bool ok = false;
    const BookmarkStorage::BookmarkList imported = BookmarkStorage::importFromFile(path, &ok);
    if (!ok || imported.isEmpty()) {
        PlatinumAlertDialog(
            ztr("Import Bookmarks"),
            ztr("No bookmarks could be imported from the selected file."),
            QStringLiteral("warning"),
            this)
            .exec();
        return;
    }

    int added = 0;
    for (const auto &entry : imported) {
        bool exists = false;
        for (const auto &cur : *m_bookmarks) {
            if (cur.second == entry.second) {
                exists = true;
                break;
            }
        }
        if (!exists) {
            m_bookmarks->append(entry);
            ++added;
        }
    }
    refresh();
    if (added > 0)
        emit bookmarksModified();

    PlatinumAlertDialog(
        ztr("Import Bookmarks"),
        ztr("Imported %1 new bookmark(s).").arg(added),
        QStringLiteral("info"),
        this)
        .exec();
}

void BookmarksDialog::exportBookmarks()
{
    if (!m_bookmarks)
        return;
    const QString path = platinumSaveFileName(
        this,
        ztr("Export Bookmarks (Chrome / Firefox HTML)"),
        QStringLiteral("netzac-bookmarks.html"),
        ztr("HTML Bookmarks (*.html *.htm);;All Files (*)"));
    if (path.isEmpty())
        return;

    if (!BookmarkStorage::exportToHtmlFile(*m_bookmarks, path)) {
        PlatinumAlertDialog(
            ztr("Export Bookmarks"),
            ztr("Could not write bookmarks to the selected file."),
            QStringLiteral("warning"),
            this)
            .exec();
    } else {
        PlatinumAlertDialog(
            ztr("Export Bookmarks"),
            ztr("Bookmarks exported in Chrome/Firefox HTML format."),
            QStringLiteral("info"),
            this)
            .exec();
    }
}

PreferencesDialog::PreferencesDialog(
    const NetzacPreferences &prefs,
    const QString &currentUrl,
    QWidget *parent)
    : PlatinumDialog(parent)
    , m_prefs(prefs)
{
    setWindowTitle(ztr("Preferences"));
    setMinimumSize(620, 520);
    resize(660, 550);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(12, 12, 12, 12);
    root->setSpacing(10);

    auto *mainRow = new QHBoxLayout();
    mainRow->setSpacing(10);

    // Left Category pane (classic Netscape Navigator 4.0)
    auto *leftCol = new QVBoxLayout();
    leftCol->setSpacing(4);
    auto *catLbl = new QLabel(ztr("Category:"), this);
    catLbl->setFont(zacosFont(QStringLiteral("ZacOS 9 System"), 12, true));
    leftCol->addWidget(catLbl);

    m_categoryList = new QListWidget(this);
    m_categoryList->setFixedWidth(155);
    m_categoryList->addItem(ztr("Navigator"));
    m_categoryList->addItem(ztr("Calm Mode"));
    m_categoryList->addItem(ztr("Personal Toolbar"));
    m_categoryList->addItem(ztr("Quick Links"));
    m_categoryList->addItem(ztr("Cookies"));
    m_categoryList->addItem(ztr("Cache"));
    m_categoryList->addItem(ztr("Proxies"));
    m_categoryList->addItem(ztr("User Agent"));
    m_categoryList->addItem(ztr("Ad Blocker"));
    leftCol->addWidget(m_categoryList, 1);
    mainRow->addLayout(leftCol);

    // Right Category Detail pane
    auto *rightCol = new QVBoxLayout();
    rightCol->setSpacing(8);

    auto *hdrFrame = new QFrame(this);
    hdrFrame->setFrameStyle(QFrame::Box | QFrame::Sunken);
    auto *hdrLay = new QHBoxLayout(hdrFrame);
    hdrLay->setContentsMargins(8, 4, 8, 4);
    m_pageHeader = new QLabel(ztr("Navigator"), hdrFrame);
    m_pageHeader->setFont(zacosFont(QStringLiteral("ZacOS 9 System"), 12, true));
    hdrLay->addWidget(m_pageHeader);
    rightCol->addWidget(hdrFrame);

    m_stack = new QStackedWidget(this);
    m_stack->addWidget(buildNavigatorPage(currentUrl));
    m_stack->addWidget(buildCalmModePage());
    m_stack->addWidget(buildPersonalToolbarPage());
    m_stack->addWidget(buildQuickLinksPage());
    m_stack->addWidget(buildCookiesPage());
    m_stack->addWidget(buildCachePage());
    m_stack->addWidget(buildProxiesPage());
    m_stack->addWidget(buildUserAgentPage());
    m_stack->addWidget(buildAdBlockerPage());
    rightCol->addWidget(m_stack, 1);
    mainRow->addLayout(rightCol, 1);

    root->addLayout(mainRow, 1);

    auto *sep = new QFrame(this);
    sep->setFrameShape(QFrame::HLine);
    root->addWidget(sep);

    auto *btnRow = new QHBoxLayout();
    btnRow->addStretch(1);
    QPushButton *cancelBtn = makeDialogButton(ztr("Cancel"), false, this);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    QPushButton *okBtn = makeDialogButton(ztr("OK"), true, this);
    connect(okBtn, &QPushButton::clicked, this, &QDialog::accept);
    btnRow->addWidget(cancelBtn);
    btnRow->addWidget(okBtn);
    root->addLayout(btnRow);

    connect(m_categoryList, &QListWidget::currentRowChanged, this, [this](int row) {
        if (row >= 0 && row < m_stack->count()) {
            m_stack->setCurrentIndex(row);
            if (QListWidgetItem *item = m_categoryList->item(row))
                m_pageHeader->setText(item->text());
        }
    });
    m_categoryList->setCurrentRow(0);
}

void PreferencesDialog::selectCategory(int index)
{
    if (index >= 0 && index < m_categoryList->count())
        m_categoryList->setCurrentRow(index);
}

QWidget *PreferencesDialog::buildNavigatorPage(const QString &currentUrl)
{
    auto *page = new QWidget(this);
    auto *lay = new QVBoxLayout(page);
    lay->setContentsMargins(6, 4, 6, 4);
    lay->setSpacing(8);

    auto *homeLbl = new QLabel(ztr("Navigator Home Page Location:"), page);
    lay->addWidget(homeLbl);

    m_homeEdit = new QLineEdit(m_prefs.homeUrl, page);
    m_homeEdit->setFont(zacosFont(QStringLiteral("ZacOS 9 System"), 12));
    lay->addWidget(m_homeEdit);

    auto *homeBtnRow = new QHBoxLayout();
    QPushButton *useCurBtn = makeDialogButton(ztr("Use Current Page"), false, page);
    useCurBtn->setEnabled(!currentUrl.isEmpty());
    connect(useCurBtn, &QPushButton::clicked, this, [this, currentUrl]() {
        if (!currentUrl.isEmpty())
            m_homeEdit->setText(currentUrl);
    });
    homeBtnRow->addWidget(useCurBtn);
    homeBtnRow->addStretch(1);
    lay->addLayout(homeBtnRow);

    lay->addSpacing(4);
    lay->addWidget(new QLabel(ztr("When Netzac Starts:"), page));
    m_startupPopup = new PlatinumPopupButton(page);
    m_startupPopup->addItem(ztr("Open Home Page"), int(NetzacPreferences::StartupMode::HomePage));
    m_startupPopup->addItem(
        ztr("Continue where I left off"),
        int(NetzacPreferences::StartupMode::RestoreLastPage));
    m_startupPopup->setCurrentData(int(m_prefs.startupMode));
    lay->addWidget(m_startupPopup);

    lay->addSpacing(4);
    auto *searchLbl = new QLabel(ztr("Default Search Engine:"), page);
    lay->addWidget(searchLbl);

    m_searchPopup = new PlatinumPopupButton(page);
    for (const auto &entry : NetzacPreferences::searchEnginePresets())
        m_searchPopup->addItem(entry.label, QString::fromLatin1(entry.id));
    m_searchPopup->setCurrentData(m_prefs.searchEngineId);
    lay->addWidget(m_searchPopup);

    lay->addSpacing(6);
    m_imagesChk = new QCheckBox(ztr("Automatically load images"), page);
    m_imagesChk->setChecked(m_prefs.autoloadImages);
    lay->addWidget(m_imagesChk);

    m_jsChk = new QCheckBox(ztr("Enable JavaScript"), page);
    m_jsChk->setChecked(m_prefs.enableJavaScript);
    lay->addWidget(m_jsChk);

    m_rememberPasswordsChk = new QCheckBox(ztr("Remember passwords (ZacOS Keychain)"), page);
    m_rememberPasswordsChk->setChecked(m_prefs.rememberPasswords);
    m_rememberPasswordsChk->setEnabled(PasswordStorage::isAvailable());
    lay->addWidget(m_rememberPasswordsChk);

    lay->addSpacing(8);
    lay->addWidget(new QLabel(ztr("Default Browser:"), page));
    m_defaultBrowserLbl = new QLabel(page);
    lay->addWidget(m_defaultBrowserLbl);

    auto *defBtnRow = new QHBoxLayout();
    m_setDefaultBrowserBtn = makeDialogButton(ztr("Set as Default Browser"), false, page);
    m_setDefaultBrowserBtn->setMinimumWidth(240);
    const auto refreshDefaultStatus = [this]() {
        const bool isDef = NetzacPreferences::isDefaultBrowser();
        m_defaultBrowserLbl->setText(
            isDef
                ? ztr("Netzac is currently your default browser.")
                : ztr("Netzac is not your default browser."));
        m_setDefaultBrowserBtn->setEnabled(!isDef);
    };
    refreshDefaultStatus();
    connect(m_setDefaultBrowserBtn, &QPushButton::clicked, this, [refreshDefaultStatus]() {
        NetzacPreferences::setAsDefaultBrowser();
        refreshDefaultStatus();
    });
    defBtnRow->addWidget(m_setDefaultBrowserBtn);
    defBtnRow->addStretch(1);
    lay->addLayout(defBtnRow);

    lay->addSpacing(10);
    auto *resetRow = new QHBoxLayout();
    QPushButton *resetBtn = makeDialogButton(ztr("Reset All Settings..."), false, page);
    resetBtn->setMinimumWidth(200);
    connect(resetBtn, &QPushButton::clicked, this, [this]() {
        PlatinumDialog confirm(this);
        confirm.setWindowTitle(ztr("Reset All Settings"));
        confirm.setFixedWidth(400);
        auto *cLay = new QVBoxLayout(&confirm);
        cLay->setContentsMargins(12, 12, 12, 12);
        cLay->setSpacing(10);

        auto *top = new QHBoxLayout();
        auto *iconLbl = new QLabel(&confirm);
        iconLbl->setPixmap(makeAlertIcon(QStringLiteral("warning"), 32));
        iconLbl->setAlignment(Qt::AlignTop);
        auto *msgLbl = new QLabel(
            ztr("This will reset all preferences, bookmarks, history, Personal Toolbar, "
                "and saved Netzac passwords back to their defaults.\n\n"
                "Do you want to continue?"),
            &confirm);
        msgLbl->setWordWrap(true);
        top->addWidget(iconLbl);
        top->addWidget(msgLbl, 1);
        cLay->addLayout(top);

        auto *btnRow = new QHBoxLayout();
        btnRow->addStretch(1);
        QPushButton *cancelBtn = makeDialogButton(ztr("Cancel"), true, &confirm);
        QPushButton *doResetBtn = makeDialogButton(ztr("Reset Everything"), false, &confirm);
        doResetBtn->setMinimumWidth(140);
        connect(cancelBtn, &QPushButton::clicked, &confirm, &QDialog::reject);
        connect(doResetBtn, &QPushButton::clicked, &confirm, &QDialog::accept);
        btnRow->addWidget(cancelBtn);
        btnRow->addWidget(doResetBtn);
        cLay->addLayout(btnRow);

        if (confirm.exec() == QDialog::Accepted) {
            NetzacPreferences::resetAll();
            m_prefs = NetzacPreferences::defaults();
            m_resetRequested = true;
            accept();
        }
    });
    resetRow->addWidget(resetBtn);
    resetRow->addStretch(1);
    lay->addLayout(resetRow);

    lay->addStretch(1);
    return page;
}

QWidget *PreferencesDialog::buildCalmModePage()
{
    auto *page = new QWidget(this);
    auto *lay = new QVBoxLayout(page);
    lay->setContentsMargins(6, 4, 6, 4);
    lay->setSpacing(6);

    lay->addWidget(new QLabel(ztr("JavaScript & Page Detection:"), page));
    m_calmJsPopup = new PlatinumPopupButton(page);
    m_calmJsPopup->addItem(
        ztr("Skip interactive JS web apps only (Recommended)"),
        int(CalmJsDetection::SkipWebApps));
    m_calmJsPopup->addItem(
        ztr("Apply on all non-excluded sites"),
        int(CalmJsDetection::IgnoreScripts));
    m_calmJsPopup->addItem(
        ztr("Strict: skip any page with <script> tags"),
        int(CalmJsDetection::SkipAnyScript));
    m_calmJsPopup->setCurrentData(int(m_prefs.calm.jsDetection));
    lay->addWidget(m_calmJsPopup);

    lay->addSpacing(4);
    lay->addWidget(new QLabel(ztr("Reading Layout:"), page));
    auto *layoutGroup = new QButtonGroup(page);
    m_calmLayoutClean = new QRadioButton(
        ztr("Clean full page (keep layout, remove distractions)"), page);
    m_calmLayoutReader = new QRadioButton(
        ztr("Focus on main text only (Firefox-style Reader Mode)"), page);
    layoutGroup->addButton(m_calmLayoutClean, int(CalmLayoutMode::CleanPage));
    layoutGroup->addButton(m_calmLayoutReader, int(CalmLayoutMode::ReaderFocus));
    m_calmLayoutClean->setChecked(m_prefs.calm.layoutMode == CalmLayoutMode::CleanPage);
    m_calmLayoutReader->setChecked(m_prefs.calm.layoutMode == CalmLayoutMode::ReaderFocus);
    lay->addWidget(m_calmLayoutClean);
    lay->addWidget(m_calmLayoutReader);

    lay->addSpacing(4);
    lay->addWidget(new QLabel(ztr("Elements to Remove & Clean:"), page));
    auto *chkGrid = new QGridLayout();
    chkGrid->setContentsMargins(0, 0, 0, 0);
    chkGrid->setHorizontalSpacing(14);
    chkGrid->setVerticalSpacing(4);

    m_calmRemoveScriptsChk = new QCheckBox(ztr("Remove <script> tags"), page);
    m_calmRemoveScriptsChk->setChecked(m_prefs.calm.removeScripts);
    m_calmRemoveIframesChk = new QCheckBox(ztr("Remove <iframe> embeds"), page);
    m_calmRemoveIframesChk->setChecked(m_prefs.calm.removeIframes);
    m_calmRemoveVideosChk = new QCheckBox(ztr("Remove <video> elements"), page);
    m_calmRemoveVideosChk->setChecked(m_prefs.calm.removeVideos);
    m_calmRemoveAdsChk = new QCheckBox(ztr("Remove ads, popups and hidden noise"), page);
    m_calmRemoveAdsChk->setChecked(m_prefs.calm.removeAds);
    m_calmNormalizeTablesChk = new QCheckBox(ztr("Normalize tables"), page);
    m_calmNormalizeTablesChk->setChecked(m_prefs.calm.normalizeTables);

    chkGrid->addWidget(m_calmRemoveScriptsChk, 0, 0);
    chkGrid->addWidget(m_calmRemoveIframesChk, 0, 1);
    chkGrid->addWidget(m_calmRemoveVideosChk, 1, 0);
    chkGrid->addWidget(m_calmNormalizeTablesChk, 1, 1);
    chkGrid->addWidget(m_calmRemoveAdsChk, 2, 0, 1, 2);
    lay->addLayout(chkGrid);

    lay->addSpacing(4);
    auto *imgRow = new QHBoxLayout();
    imgRow->addWidget(new QLabel(ztr("Maximum image width (px):"), page));
    m_calmMaxImgSpin = new QSpinBox(page);
    m_calmMaxImgSpin->setRange(240, 1920);
    m_calmMaxImgSpin->setSingleStep(20);
    m_calmMaxImgSpin->setValue(m_prefs.calm.maxImageWidth);
    m_calmMaxImgSpin->setFixedWidth(90);
    imgRow->addWidget(m_calmMaxImgSpin);
    imgRow->addStretch(1);
    lay->addLayout(imgRow);

    lay->addWidget(new QLabel(ztr("Excluded hosts (comma-separated, includes subdomains):"), page));
    m_calmExcludedHostsEdit = new QLineEdit(
        m_prefs.calm.excludedHosts.join(QStringLiteral(", ")), page);
    m_calmExcludedHostsEdit->setFont(zacosFont(QStringLiteral("ZacOS 9 System"), 12));
    lay->addWidget(m_calmExcludedHostsEdit);

    lay->addStretch(1);
    return page;
}

namespace {

bool editFolderSubItemDialog(
    NetzacPreferences::PersonalToolbarSubItem *sub,
    const QString &dialogTitle,
    QWidget *parent)
{
    PlatinumDialog dlg(parent);
    dlg.setWindowTitle(dialogTitle);
    dlg.setFixedWidth(360);

    auto *lay = new QVBoxLayout(&dlg);
    lay->setContentsMargins(12, 12, 12, 12);
    lay->setSpacing(6);

    lay->addWidget(new QLabel(ztr("Site Title:"), &dlg));
    auto *titleEdit = new QLineEdit(sub->title, &dlg);
    titleEdit->setFont(zacosFont(QStringLiteral("ZacOS 9 System"), 12));
    lay->addWidget(titleEdit);

    lay->addWidget(new QLabel(ztr("Target URL:"), &dlg));
    auto *urlEdit = new QLineEdit(sub->url, &dlg);
    urlEdit->setFont(zacosFont(QStringLiteral("ZacOS 9 System"), 12));
    lay->addWidget(urlEdit);

    auto *btnRow = new QHBoxLayout();
    btnRow->addStretch(1);
    QPushButton *cancelBtn = makeDialogButton(ztr("Cancel"), false, &dlg);
    QObject::connect(cancelBtn, &QPushButton::clicked, &dlg, &QDialog::reject);
    QPushButton *okBtn = makeDialogButton(ztr("OK"), true, &dlg);
    QObject::connect(okBtn, &QPushButton::clicked, &dlg, &QDialog::accept);
    btnRow->addWidget(cancelBtn);
    btnRow->addWidget(okBtn);
    lay->addLayout(btnRow);

    if (dlg.exec() != QDialog::Accepted)
        return false;

    const QString t = titleEdit->text().trimmed();
    const QString u = urlEdit->text().trimmed();
    if (t.isEmpty() || u.isEmpty())
        return false;
    sub->title = t;
    sub->url = u;
    return true;
}

bool editPersonalToolbarItemDialog(
    NetzacPreferences::PersonalToolbarItem *item,
    const QString &dialogTitle,
    QWidget *parent)
{
    PlatinumDialog dlg(parent);
    dlg.setWindowTitle(dialogTitle);
    dlg.setFixedWidth(430);

    auto *lay = new QVBoxLayout(&dlg);
    lay->setContentsMargins(12, 12, 12, 12);
    lay->setSpacing(6);

    lay->addWidget(new QLabel(ztr("Button Label:"), &dlg));
    auto *titleEdit = new QLineEdit(item->title, &dlg);
    titleEdit->setFont(zacosFont(QStringLiteral("ZacOS 9 System"), 12));
    lay->addWidget(titleEdit);

    lay->addWidget(new QLabel(ztr("Icon:"), &dlg));
    auto *iconPopup = new PlatinumPopupButton(&dlg);
    iconPopup->addItem(ztr("Page"), QStringLiteral("page"));
    iconPopup->addItem(ztr("Home"), QStringLiteral("home"));
    iconPopup->addItem(ztr("Search"), QStringLiteral("search"));
    iconPopup->addItem(ztr("Guide"), QStringLiteral("guide"));
    iconPopup->addItem(ztr("Folder"), QStringLiteral("folder"));
    const QString initIcon = item->isFolder
        ? QStringLiteral("folder")
        : (item->icon.isEmpty() ? QStringLiteral("page") : item->icon);
    iconPopup->setCurrentData(initIcon);
    lay->addWidget(iconPopup);

    auto *urlBox = new QWidget(&dlg);
    auto *urlLay = new QVBoxLayout(urlBox);
    urlLay->setContentsMargins(0, 0, 0, 0);
    urlLay->setSpacing(4);
    urlLay->addWidget(new QLabel(ztr("Target URL:"), urlBox));
    auto *urlEdit = new QLineEdit(item->url, urlBox);
    urlEdit->setFont(zacosFont(QStringLiteral("ZacOS 9 System"), 12));
    urlLay->addWidget(urlEdit);
    lay->addWidget(urlBox);

    QVector<NetzacPreferences::PersonalToolbarSubItem> subItems = item->children;
    auto *folderBox = new QWidget(&dlg);
    auto *folderLay = new QVBoxLayout(folderBox);
    folderLay->setContentsMargins(0, 0, 0, 0);
    folderLay->setSpacing(4);
    folderLay->addWidget(new QLabel(ztr("Sites inside folder:"), folderBox));
    auto *subList = new QListWidget(folderBox);
    subList->setFixedHeight(140);
    const auto refreshSubList = [subList, &subItems](int selectRow) {
        subList->clear();
        for (const auto &s : subItems)
            subList->addItem(QStringLiteral("%1  —  %2").arg(s.title, s.url));
        if (selectRow >= 0 && selectRow < subList->count())
            subList->setCurrentRow(selectRow);
    };
    refreshSubList(0);
    folderLay->addWidget(subList);

    auto *subBtnRow = new QHBoxLayout();
    subBtnRow->setSpacing(4);
    QPushButton *subAdd = makeDialogButton(ztr("Add"), false, folderBox);
    QPushButton *subEdit = makeDialogButton(ztr("Edit"), false, folderBox);
    QPushButton *subRem = makeDialogButton(ztr("Remove"), false, folderBox);
    QPushButton *subUp = makeDialogButton(ztr("Up"), false, folderBox);
    QPushButton *subDown = makeDialogButton(ztr("Down"), false, folderBox);
    for (QPushButton *b : {subAdd, subEdit, subRem, subUp, subDown})
        b->setMinimumWidth(56);

    QObject::connect(subAdd, &QPushButton::clicked, &dlg, [&dlg, &subItems, refreshSubList]() {
        NetzacPreferences::PersonalToolbarSubItem s{QString(), QStringLiteral("https://")};
        if (editFolderSubItemDialog(&s, ztr("Add Site to Folder"), &dlg)) {
            subItems.append(s);
            refreshSubList(int(subItems.size()) - 1);
        }
    });
    const auto editSelectedSub = [&dlg, subList, &subItems, refreshSubList]() {
        const int idx = subList->currentRow();
        if (idx >= 0 && idx < subItems.size()) {
            NetzacPreferences::PersonalToolbarSubItem s = subItems.at(idx);
            if (editFolderSubItemDialog(&s, ztr("Edit Folder Site"), &dlg)) {
                subItems[idx] = s;
                refreshSubList(idx);
            }
        }
    };
    QObject::connect(subEdit, &QPushButton::clicked, &dlg, editSelectedSub);
    QObject::connect(subList, &QListWidget::itemDoubleClicked, &dlg, [editSelectedSub](QListWidgetItem *) {
        editSelectedSub();
    });
    QObject::connect(subRem, &QPushButton::clicked, &dlg, [subList, &subItems, refreshSubList]() {
        const int idx = subList->currentRow();
        if (idx >= 0 && idx < subItems.size()) {
            subItems.removeAt(idx);
            refreshSubList(qMin(idx, int(subItems.size()) - 1));
        }
    });
    QObject::connect(subUp, &QPushButton::clicked, &dlg, [subList, &subItems, refreshSubList]() {
        const int idx = subList->currentRow();
        if (idx > 0 && idx < subItems.size()) {
            subItems.swapItemsAt(idx, idx - 1);
            refreshSubList(idx - 1);
        }
    });
    QObject::connect(subDown, &QPushButton::clicked, &dlg, [subList, &subItems, refreshSubList]() {
        const int idx = subList->currentRow();
        if (idx >= 0 && idx + 1 < subItems.size()) {
            subItems.swapItemsAt(idx, idx + 1);
            refreshSubList(idx + 1);
        }
    });

    subBtnRow->addWidget(subAdd);
    subBtnRow->addWidget(subEdit);
    subBtnRow->addWidget(subRem);
    subBtnRow->addWidget(subUp);
    subBtnRow->addWidget(subDown);
    subBtnRow->addStretch(1);
    folderLay->addLayout(subBtnRow);
    lay->addWidget(folderBox);

    const auto syncFolderVisibility = [&dlg, iconPopup, urlBox, folderBox]() {
        const bool isFolder = iconPopup->currentData().toString() == QLatin1String("folder");
        urlBox->setVisible(!isFolder);
        folderBox->setVisible(isFolder);
        dlg.adjustSize();
    };
    QObject::connect(iconPopup, &PlatinumPopupButton::currentIndexChanged, &dlg, syncFolderVisibility);
    syncFolderVisibility();

    auto *btnRow = new QHBoxLayout();
    btnRow->addStretch(1);
    QPushButton *cancelBtn = makeDialogButton(ztr("Cancel"), false, &dlg);
    QObject::connect(cancelBtn, &QPushButton::clicked, &dlg, &QDialog::reject);
    QPushButton *okBtn = makeDialogButton(ztr("OK"), true, &dlg);
    QObject::connect(okBtn, &QPushButton::clicked, &dlg, &QDialog::accept);
    btnRow->addWidget(cancelBtn);
    btnRow->addWidget(okBtn);
    lay->addLayout(btnRow);

    if (dlg.exec() != QDialog::Accepted)
        return false;

    const QString t = titleEdit->text().trimmed();
    const bool isFolder = iconPopup->currentData().toString() == QLatin1String("folder");
    const QString u = urlEdit->text().trimmed();
    if (t.isEmpty() || (!isFolder && u.isEmpty()))
        return false;

    item->title = t;
    item->isFolder = isFolder;
    item->icon = iconPopup->currentData().toString();
    item->url = isFolder ? QString() : u;
    item->children = isFolder ? subItems : QVector<NetzacPreferences::PersonalToolbarSubItem>{};
    return true;
}

}  // namespace

QWidget *PreferencesDialog::buildPersonalToolbarPage()
{
    m_personalItems = m_prefs.personalToolbar;

    auto *page = new QWidget(this);
    auto *lay = new QVBoxLayout(page);
    lay->setContentsMargins(6, 4, 6, 4);
    lay->setSpacing(6);

    lay->addWidget(new QLabel(ztr("Customize Personal Toolbar buttons & folders:"), page));

    m_ptList = new QListWidget(page);
    const auto refreshPtList = [this](int selectRow) {
        m_ptList->clear();
        for (const auto &entry : m_personalItems) {
            if (entry.isFolder) {
                m_ptList->addItem(
                    QStringLiteral("[%1: %2]  %3")
                        .arg(ztr("Folder"))
                        .arg(entry.children.size())
                        .arg(entry.title));
            } else {
                m_ptList->addItem(QStringLiteral("%1  —  %2").arg(entry.title, entry.url));
            }
        }
        if (selectRow >= 0 && selectRow < m_ptList->count())
            m_ptList->setCurrentRow(selectRow);
    };
    refreshPtList(0);
    lay->addWidget(m_ptList, 1);

    auto *row1 = new QHBoxLayout();
    row1->setSpacing(4);
    QPushButton *addBtn = makeDialogButton(ztr("Add"), false, page);
    QPushButton *editBtn = makeDialogButton(ztr("Edit"), false, page);
    QPushButton *remBtn = makeDialogButton(ztr("Remove"), false, page);
    QPushButton *upBtn = makeDialogButton(ztr("Up"), false, page);
    QPushButton *downBtn = makeDialogButton(ztr("Down"), false, page);
    QPushButton *resetBtn = makeDialogButton(ztr("Reset"), false, page);
    for (QPushButton *b : {addBtn, editBtn, remBtn, upBtn, downBtn, resetBtn})
        b->setMinimumWidth(56);

    connect(addBtn, &QPushButton::clicked, page, [this, page, refreshPtList]() {
        NetzacPreferences::PersonalToolbarItem item{QString(), QStringLiteral("https://"), QStringLiteral("page"), false, {}};
        if (editPersonalToolbarItemDialog(&item, ztr("Add Toolbar Button"), page)) {
            m_personalItems.append(item);
            refreshPtList(int(m_personalItems.size()) - 1);
        }
    });
    const auto editSelected = [this, page, refreshPtList]() {
        const int idx = m_ptList->currentRow();
        if (idx >= 0 && idx < m_personalItems.size()) {
            NetzacPreferences::PersonalToolbarItem item = m_personalItems.at(idx);
            if (editPersonalToolbarItemDialog(&item, ztr("Edit Toolbar Button"), page)) {
                m_personalItems[idx] = item;
                refreshPtList(idx);
            }
        }
    };
    connect(editBtn, &QPushButton::clicked, page, editSelected);
    connect(m_ptList, &QListWidget::itemDoubleClicked, page, [editSelected](QListWidgetItem *) {
        editSelected();
    });
    connect(remBtn, &QPushButton::clicked, page, [this, refreshPtList]() {
        const int idx = m_ptList->currentRow();
        if (idx >= 0 && idx < m_personalItems.size()) {
            m_personalItems.removeAt(idx);
            refreshPtList(qMin(idx, int(m_personalItems.size()) - 1));
        }
    });
    connect(upBtn, &QPushButton::clicked, page, [this, refreshPtList]() {
        const int idx = m_ptList->currentRow();
        if (idx > 0 && idx < m_personalItems.size()) {
            m_personalItems.swapItemsAt(idx, idx - 1);
            refreshPtList(idx - 1);
        }
    });
    connect(downBtn, &QPushButton::clicked, page, [this, refreshPtList]() {
        const int idx = m_ptList->currentRow();
        if (idx >= 0 && idx + 1 < m_personalItems.size()) {
            m_personalItems.swapItemsAt(idx, idx + 1);
            refreshPtList(idx + 1);
        }
    });
    connect(resetBtn, &QPushButton::clicked, page, [this, refreshPtList]() {
        m_personalItems = NetzacPreferences::defaultPersonalToolbar();
        refreshPtList(0);
    });

    row1->addWidget(addBtn);
    row1->addWidget(editBtn);
    row1->addWidget(remBtn);
    row1->addWidget(upBtn);
    row1->addWidget(downBtn);
    row1->addStretch(1);
    row1->addWidget(resetBtn);
    lay->addLayout(row1);

    return page;
}

QWidget *PreferencesDialog::buildQuickLinksPage()
{
    auto *page = new QWidget(this);
    auto *lay = new QVBoxLayout(page);
    lay->setContentsMargins(6, 4, 6, 4);
    lay->setSpacing(8);

    auto *infoLbl = new QLabel(
        ztr("Set custom URLs for the bottom-right Mail, Chat and Git buttons.\n"
            "Leave a field empty to open its built-in about: page."),
        page);
    infoLbl->setWordWrap(true);
    lay->addWidget(infoLbl);

    lay->addWidget(new QLabel(ztr("Mail URL (e.g. https://mail.google.com):"), page));
    m_mailUrlEdit = new QLineEdit(m_prefs.mailUrl, page);
    m_mailUrlEdit->setFont(zacosFont(QStringLiteral("ZacOS 9 System"), 12));
    lay->addWidget(m_mailUrlEdit);

    lay->addWidget(new QLabel(ztr("Chat URL (e.g. https://app.element.io):"), page));
    m_chatUrlEdit = new QLineEdit(m_prefs.chatUrl, page);
    m_chatUrlEdit->setFont(zacosFont(QStringLiteral("ZacOS 9 System"), 12));
    lay->addWidget(m_chatUrlEdit);

    lay->addWidget(new QLabel(ztr("Git URL (e.g. https://github.com):"), page));
    m_gitUrlEdit = new QLineEdit(m_prefs.gitUrl, page);
    m_gitUrlEdit->setFont(zacosFont(QStringLiteral("ZacOS 9 System"), 12));
    lay->addWidget(m_gitUrlEdit);

    lay->addStretch(1);
    return page;
}

QWidget *PreferencesDialog::buildCookiesPage()
{
    auto *page = new QWidget(this);
    auto *lay = new QVBoxLayout(page);
    lay->setContentsMargins(6, 4, 6, 4);
    lay->setSpacing(8);

    lay->addWidget(new QLabel(ztr("Cookie Permissions:"), page));

    auto *group = new QButtonGroup(page);
    m_cookieAllowAll = new QRadioButton(ztr("Accept all cookies"), page);
    m_cookieBlockThird = new QRadioButton(
        ztr("Accept only cookies from originating server"), page);
    m_cookieBlockAll = new QRadioButton(ztr("Disable cookies"), page);
    group->addButton(m_cookieAllowAll, int(NetzacPreferences::CookiePolicy::AllowAll));
    group->addButton(m_cookieBlockThird, int(NetzacPreferences::CookiePolicy::BlockThirdParty));
    group->addButton(m_cookieBlockAll, int(NetzacPreferences::CookiePolicy::BlockAll));

    m_cookieAllowAll->setChecked(m_prefs.cookiePolicy == NetzacPreferences::CookiePolicy::AllowAll);
    m_cookieBlockThird->setChecked(m_prefs.cookiePolicy == NetzacPreferences::CookiePolicy::BlockThirdParty);
    m_cookieBlockAll->setChecked(m_prefs.cookiePolicy == NetzacPreferences::CookiePolicy::BlockAll);

    lay->addWidget(m_cookieAllowAll);
    lay->addWidget(m_cookieBlockThird);
    lay->addWidget(m_cookieBlockAll);

    lay->addSpacing(4);
    m_cookieSessionChk = new QCheckBox(ztr("Keep cookies only until Netzac closes"), page);
    m_cookieSessionChk->setChecked(m_prefs.sessionCookiesOnly);
    lay->addWidget(m_cookieSessionChk);

    lay->addSpacing(6);
    auto *clearRow = new QHBoxLayout();
    QPushButton *clearBtn = makeDialogButton(ztr("Clear Cookies Now"), false, page);
    m_cookieStatusLbl = new QLabel(page);
    m_cookieStatusLbl->setFont(zacosFont(QStringLiteral("ZacOS 9 Views"), 9));
    connect(clearBtn, &QPushButton::clicked, this, [this]() {
        if (QWebEngineProfile *profile = netzacProfile()) {
            if (QWebEngineCookieStore *store = profile->cookieStore())
                store->deleteAllCookies();
        }
        m_cookieStatusLbl->setText(ztr("All cookies cleared."));
    });
    clearRow->addWidget(clearBtn);
    clearRow->addWidget(m_cookieStatusLbl, 1);
    lay->addLayout(clearRow);

    lay->addStretch(1);
    return page;
}

QWidget *PreferencesDialog::buildCachePage()
{
    auto *page = new QWidget(this);
    auto *lay = new QVBoxLayout(page);
    lay->setContentsMargins(6, 4, 6, 4);
    lay->setSpacing(8);

    lay->addWidget(new QLabel(ztr("Browser Cache Storage:"), page));
    m_cachePopup = new PlatinumPopupButton(page);
    m_cachePopup->addItem(ztr("Disk Cache"), int(NetzacPreferences::CacheMode::Disk));
    m_cachePopup->addItem(ztr("Memory Cache"), int(NetzacPreferences::CacheMode::Memory));
    m_cachePopup->addItem(ztr("Disable Cache"), int(NetzacPreferences::CacheMode::Disabled));
    m_cachePopup->setCurrentData(int(m_prefs.cacheMode));
    lay->addWidget(m_cachePopup);

    auto *sizeRow = new QHBoxLayout();
    sizeRow->addWidget(new QLabel(ztr("Maximum Cache Size (MB):"), page));
    m_cacheMaxSpin = new QSpinBox(page);
    m_cacheMaxSpin->setRange(1, 2048);
    m_cacheMaxSpin->setValue(m_prefs.cacheMaxMb);
    m_cacheMaxSpin->setFixedWidth(90);
    sizeRow->addWidget(m_cacheMaxSpin);
    sizeRow->addStretch(1);
    lay->addLayout(sizeRow);

    const auto updateCacheSpin = [this]() {
        const auto mode = static_cast<NetzacPreferences::CacheMode>(m_cachePopup->currentData().toInt());
        m_cacheMaxSpin->setEnabled(mode != NetzacPreferences::CacheMode::Disabled);
    };
    connect(m_cachePopup, &PlatinumPopupButton::currentIndexChanged, this, updateCacheSpin);
    updateCacheSpin();

    lay->addSpacing(6);
    auto *clearRow = new QHBoxLayout();
    QPushButton *clearBtn = makeDialogButton(ztr("Clear Cache Now"), false, page);
    m_cacheStatusLbl = new QLabel(page);
    m_cacheStatusLbl->setFont(zacosFont(QStringLiteral("ZacOS 9 Views"), 9));
    connect(clearBtn, &QPushButton::clicked, this, [this]() {
        if (QWebEngineProfile *profile = netzacProfile())
            profile->clearHttpCache();
        m_cacheStatusLbl->setText(ztr("HTTP cache cleared."));
    });
    clearRow->addWidget(clearBtn);
    clearRow->addWidget(m_cacheStatusLbl, 1);
    lay->addLayout(clearRow);

    lay->addStretch(1);
    return page;
}

QWidget *PreferencesDialog::buildProxiesPage()
{
    auto *page = new QWidget(this);
    auto *lay = new QVBoxLayout(page);
    lay->setContentsMargins(6, 4, 6, 4);
    lay->setSpacing(8);

    lay->addWidget(new QLabel(ztr("Configure Proxies to Access the Internet:"), page));

    auto *group = new QButtonGroup(page);
    m_proxyDirect = new QRadioButton(ztr("Direct connection to the Internet"), page);
    m_proxySystem = new QRadioButton(ztr("Use system proxy settings"), page);
    m_proxyManual = new QRadioButton(ztr("Manual proxy configuration"), page);
    group->addButton(m_proxyDirect, int(NetzacPreferences::ProxyMode::Direct));
    group->addButton(m_proxySystem, int(NetzacPreferences::ProxyMode::System));
    group->addButton(m_proxyManual, int(NetzacPreferences::ProxyMode::Manual));

    m_proxyDirect->setChecked(m_prefs.proxyMode == NetzacPreferences::ProxyMode::Direct);
    m_proxySystem->setChecked(m_prefs.proxyMode == NetzacPreferences::ProxyMode::System);
    m_proxyManual->setChecked(m_prefs.proxyMode == NetzacPreferences::ProxyMode::Manual);

    lay->addWidget(m_proxyDirect);
    lay->addWidget(m_proxySystem);
    lay->addWidget(m_proxyManual);

    m_proxyManualBox = new QWidget(page);
    auto *manLay = new QVBoxLayout(m_proxyManualBox);
    manLay->setContentsMargins(20, 4, 0, 0);
    manLay->setSpacing(6);

    auto *typeRow = new QHBoxLayout();
    typeRow->addWidget(new QLabel(ztr("Proxy Type:"), m_proxyManualBox));
    m_proxyTypePopup = new PlatinumPopupButton(m_proxyManualBox);
    m_proxyTypePopup->addItem(ztr("HTTP Proxy"), int(NetzacPreferences::ProxyType::Http));
    m_proxyTypePopup->addItem(ztr("SOCKS5 Proxy"), int(NetzacPreferences::ProxyType::Socks5));
    m_proxyTypePopup->setCurrentData(int(m_prefs.proxyType));
    typeRow->addWidget(m_proxyTypePopup, 1);
    manLay->addLayout(typeRow);

    auto *hostRow = new QHBoxLayout();
    hostRow->addWidget(new QLabel(ztr("Host:"), m_proxyManualBox));
    m_proxyHostEdit = new QLineEdit(m_prefs.proxyHost, m_proxyManualBox);
    m_proxyHostEdit->setPlaceholderText(QStringLiteral("127.0.0.1"));
    hostRow->addWidget(m_proxyHostEdit, 1);
    hostRow->addWidget(new QLabel(ztr("Port:"), m_proxyManualBox));
    m_proxyPortSpin = new QSpinBox(m_proxyManualBox);
    m_proxyPortSpin->setRange(1, 65535);
    m_proxyPortSpin->setValue(m_prefs.proxyPort);
    m_proxyPortSpin->setFixedWidth(78);
    hostRow->addWidget(m_proxyPortSpin);
    manLay->addLayout(hostRow);

    lay->addWidget(m_proxyManualBox);

    connect(m_proxyDirect, &QRadioButton::toggled, this, &PreferencesDialog::updateProxyControlsEnabled);
    connect(m_proxySystem, &QRadioButton::toggled, this, &PreferencesDialog::updateProxyControlsEnabled);
    connect(m_proxyManual, &QRadioButton::toggled, this, &PreferencesDialog::updateProxyControlsEnabled);
    updateProxyControlsEnabled();

    lay->addStretch(1);
    return page;
}

void PreferencesDialog::updateProxyControlsEnabled()
{
    m_proxyManualBox->setEnabled(m_proxyManual->isChecked());
}

QWidget *PreferencesDialog::buildUserAgentPage()
{
    auto *page = new QWidget(this);
    auto *lay = new QVBoxLayout(page);
    lay->setContentsMargins(6, 4, 6, 4);
    lay->setSpacing(8);

    lay->addWidget(new QLabel(ztr("Browser Identity Preset:"), page));
    m_uaPresetPopup = new PlatinumPopupButton(page);
    for (const auto &entry : NetzacPreferences::userAgentPresets())
        m_uaPresetPopup->addItem(entry.label, QString::fromLatin1(entry.id));
    m_uaPresetPopup->setCurrentData(m_prefs.userAgentPreset);
    lay->addWidget(m_uaPresetPopup);

    lay->addSpacing(4);
    lay->addWidget(new QLabel(ztr("HTTP User-Agent String:"), page));
    m_uaEdit = new QLineEdit(m_prefs.userAgent, page);
    m_uaEdit->setFont(zacosFont(QStringLiteral("ZacOS 9 Views"), 9));
    lay->addWidget(m_uaEdit);

    auto *resetRow = new QHBoxLayout();
    QPushButton *resetBtn = makeDialogButton(ztr("Reset to Chrome (Linux)"), false, page);
    connect(resetBtn, &QPushButton::clicked, this, [this]() {
        m_syncingUa = true;
        m_uaPresetPopup->setCurrentData(QStringLiteral("chrome_linux"));
        m_uaEdit->setText(NetzacPreferences::defaultChromeUserAgent());
        m_syncingUa = false;
    });
    resetRow->addWidget(resetBtn);
    resetRow->addStretch(1);
    lay->addLayout(resetRow);

    connect(m_uaPresetPopup, &PlatinumPopupButton::currentIndexChanged, this, [this]() {
        if (m_syncingUa)
            return;
        const QString preset = m_uaPresetPopup->currentData().toString();
        const QString ua = NetzacPreferences::userAgentForPreset(preset);
        if (!ua.isEmpty()) {
            m_syncingUa = true;
            m_uaEdit->setText(ua);
            m_syncingUa = false;
        }
    });

    connect(m_uaEdit, &QLineEdit::textEdited, this, [this](const QString &text) {
        if (m_syncingUa)
            return;
        m_syncingUa = true;
        QString matchedPreset = QStringLiteral("custom");
        for (const auto &entry : NetzacPreferences::userAgentPresets()) {
            if (*entry.value && text == QLatin1String(entry.value)) {
                matchedPreset = QString::fromLatin1(entry.id);
                break;
            }
        }
        m_uaPresetPopup->setCurrentData(matchedPreset);
        m_syncingUa = false;
    });

    lay->addStretch(1);
    return page;
}

QWidget *PreferencesDialog::buildAdBlockerPage()
{
    auto *page = new QWidget(this);
    auto *lay = new QVBoxLayout(page);
    lay->setContentsMargins(6, 4, 6, 4);
    lay->setSpacing(8);

    m_adBlockChk = new QCheckBox(ztr("Enable the Netzac Ad Blocker"), page);
    m_adBlockChk->setChecked(m_prefs.adBlockEnabled);
    lay->addWidget(m_adBlockChk);

    auto *desc = new QLabel(
        ztr("Netzac blocks advertisements, trackers and malicious addresses using filter "
            "lists such as EasyList. Lists are downloaded from the addresses below and "
            "stored locally on this computer."),
        page);
    desc->setFont(zacosFont(QStringLiteral("ZacOS 9 Views"), 9));
    desc->setWordWrap(true);
    lay->addWidget(desc);

    lay->addSpacing(4);
    lay->addWidget(new QLabel(ztr("Filter list addresses:"), page));

    m_adBlockUrls = m_prefs.adBlockSubscriptions;
    m_adBlockList = new QListWidget(page);
    m_adBlockList->setMinimumHeight(90);
    lay->addWidget(m_adBlockList, 1);
    refreshAdBlockList();

    auto *listButtons = new QHBoxLayout();
    m_adBlockAddBtn = makeDialogButton(ztr("Add List..."), false, page);
    connect(m_adBlockAddBtn, &QPushButton::clicked, this, &PreferencesDialog::addAdBlockListUrl);
    m_adBlockRemoveBtn = makeDialogButton(ztr("Remove List"), false, page);
    connect(m_adBlockRemoveBtn, &QPushButton::clicked, this, [this]() {
        const int row = m_adBlockList->currentRow();
        if (row < 0 || row >= m_adBlockUrls.size())
            return;
        m_adBlockUrls.removeAt(row);
        refreshAdBlockList();
        updateAdBlockStatus();
    });
    m_adBlockDefaultsBtn = makeDialogButton(ztr("Restore Default Lists"), false, page);
    connect(m_adBlockDefaultsBtn, &QPushButton::clicked, this, [this]() {
        m_adBlockUrls = NetzacPreferences::defaultAdBlockSubscriptions();
        refreshAdBlockList();
        updateAdBlockStatus();
    });
    listButtons->addWidget(m_adBlockAddBtn);
    listButtons->addWidget(m_adBlockRemoveBtn);
    listButtons->addWidget(m_adBlockDefaultsBtn);
    listButtons->addStretch(1);
    lay->addLayout(listButtons);

    lay->addSpacing(4);
    auto *updateRow = new QHBoxLayout();
    m_adBlockUpdateBtn = makeDialogButton(ztr("Update Filter Lists Now"), false, page);
    m_adBlockUpdateBtn->setMinimumWidth(190);
    connect(m_adBlockUpdateBtn, &QPushButton::clicked, this, [this]() {
        NetzacAdBlocker *blocker = NetzacAdBlocker::instance();
        blocker->configure(m_adBlockChk->isChecked(), m_adBlockUrls);
        blocker->refreshLists(true);
        updateAdBlockStatus();
    });
    updateRow->addWidget(m_adBlockUpdateBtn);
    updateRow->addStretch(1);
    lay->addLayout(updateRow);

    m_adBlockStatusLbl = new QLabel(page);
    m_adBlockStatusLbl->setFont(zacosFont(QStringLiteral("ZacOS 9 Views"), 9));
    m_adBlockStatusLbl->setWordWrap(true);
    lay->addWidget(m_adBlockStatusLbl);

    connect(m_adBlockChk, &QCheckBox::toggled, this, [this](bool enabled) {
        m_adBlockList->setEnabled(enabled);
        m_adBlockAddBtn->setEnabled(enabled);
        m_adBlockRemoveBtn->setEnabled(enabled);
        m_adBlockDefaultsBtn->setEnabled(enabled);
        m_adBlockUpdateBtn->setEnabled(enabled);
        updateAdBlockStatus();
    });

    const bool enabled = m_adBlockChk->isChecked();
    m_adBlockList->setEnabled(enabled);
    m_adBlockAddBtn->setEnabled(enabled);
    m_adBlockRemoveBtn->setEnabled(enabled);
    m_adBlockDefaultsBtn->setEnabled(enabled);
    m_adBlockUpdateBtn->setEnabled(enabled);

    connect(
        NetzacAdBlocker::instance(),
        &NetzacAdBlocker::stateChanged,
        this,
        &PreferencesDialog::updateAdBlockStatus);
    updateAdBlockStatus();

    return page;
}

void PreferencesDialog::refreshAdBlockList()
{
    if (!m_adBlockList)
        return;
    m_adBlockList->clear();
    for (const QString &url : m_adBlockUrls)
        m_adBlockList->addItem(url);
}

void PreferencesDialog::addAdBlockListUrl()
{
    PlatinumDialog dlg(this);
    dlg.setWindowTitle(ztr("Add Filter List"));
    dlg.setFixedWidth(420);

    auto *lay = new QVBoxLayout(&dlg);
    lay->setContentsMargins(12, 12, 12, 12);
    lay->setSpacing(8);
    lay->addWidget(new QLabel(ztr("Filter list address (URL):"), &dlg));
    auto *edit = new QLineEdit(QStringLiteral("https://"), &dlg);
    edit->setFont(zacosFont(QStringLiteral("ZacOS 9 System"), 12));
    lay->addWidget(edit);

    auto *row = new QHBoxLayout();
    row->addStretch(1);
    QPushButton *cancelBtn = makeDialogButton(ztr("Cancel"), true, &dlg);
    connect(cancelBtn, &QPushButton::clicked, &dlg, &QDialog::reject);
    QPushButton *okBtn = makeDialogButton(ztr("OK"), false, &dlg);
    connect(okBtn, &QPushButton::clicked, &dlg, &QDialog::accept);
    row->addWidget(cancelBtn);
    row->addWidget(okBtn);
    lay->addLayout(row);

    if (dlg.exec() != QDialog::Accepted)
        return;

    const QString text = edit->text().trimmed();
    const QUrl url(text);
    const QString scheme = url.scheme().toLower();
    if (!url.isValid() || url.host().isEmpty()
        || (scheme != QLatin1String("http") && scheme != QLatin1String("https"))) {
        PlatinumAlertDialog(
            ztr("Add Filter List"),
            ztr("The address must be a valid http:// or https:// URL."),
            QStringLiteral("warning"),
            this)
            .exec();
        return;
    }

    if (!m_adBlockUrls.contains(text)) {
        m_adBlockUrls.append(text);
        refreshAdBlockList();
        m_adBlockList->setCurrentRow(m_adBlockUrls.size() - 1);
    }
    updateAdBlockStatus();
}

void PreferencesDialog::updateAdBlockStatus()
{
    if (!m_adBlockStatusLbl || !m_adBlockChk)
        return;

    if (!m_adBlockChk->isChecked()) {
        m_adBlockStatusLbl->setText(ztr("Ad Blocker is disabled."));
        return;
    }
    if (NetzacAdBlocker::instance()->isUpdating()) {
        m_adBlockStatusLbl->setText(ztr("Updating filter lists..."));
        return;
    }
    const QString error = NetzacAdBlocker::instance()->lastError();
    if (!error.isEmpty()) {
        m_adBlockStatusLbl->setText(ztr("Update failed: %1").arg(error));
        return;
    }
    const int rules = NetzacAdBlocker::instance()->ruleCount();
    if (rules <= 0) {
        m_adBlockStatusLbl->setText(ztr("No filter lists loaded yet."));
        return;
    }
    const QDateTime updated = NetzacAdBlocker::instance()->lastUpdate();
    const QString when = updated.isValid()
        ? updated.toString(QStringLiteral("yyyy-MM-dd HH:mm"))
        : ztr("Never");
    m_adBlockStatusLbl->setText(ztr("%1 rules loaded. Last update: %2").arg(rules).arg(when));
}

NetzacPreferences PreferencesDialog::preferences() const
{
    if (m_resetRequested)
        return NetzacPreferences::defaults();

    NetzacPreferences out = m_prefs;
    const QString home = m_homeEdit->text().trimmed();
    if (!home.isEmpty())
        out.homeUrl = home;
    out.startupMode = static_cast<NetzacPreferences::StartupMode>(m_startupPopup->currentData().toInt());
    out.searchEngineId = m_searchPopup->currentData().toString();
    out.autoloadImages = m_imagesChk->isChecked();
    out.enableJavaScript = m_jsChk->isChecked();
    out.rememberPasswords = m_rememberPasswordsChk->isChecked();

    out.calm.jsDetection = static_cast<CalmJsDetection>(m_calmJsPopup->currentData().toInt());
    out.calm.layoutMode = m_calmLayoutReader->isChecked()
        ? CalmLayoutMode::ReaderFocus
        : CalmLayoutMode::CleanPage;
    out.calm.removeScripts = m_calmRemoveScriptsChk->isChecked();
    out.calm.removeIframes = m_calmRemoveIframesChk->isChecked();
    out.calm.removeVideos = m_calmRemoveVideosChk->isChecked();
    out.calm.removeAds = m_calmRemoveAdsChk->isChecked();
    out.calm.normalizeTables = m_calmNormalizeTablesChk->isChecked();
    out.calm.maxImageWidth = m_calmMaxImgSpin->value();

    QStringList parsedHosts;
    const QStringList tokens = m_calmExcludedHostsEdit->text().split(
        QRegularExpression(QStringLiteral("[,\\s]+")), Qt::SkipEmptyParts);
    for (const QString &tok : tokens) {
        const QString norm = CalmPolicy::normalizeHost(tok);
        if (!norm.isEmpty() && !parsedHosts.contains(norm))
            parsedHosts.append(norm);
    }
    out.calm.excludedHosts = parsedHosts;

    out.personalToolbar = m_personalItems;
    out.mailUrl = m_mailUrlEdit->text().trimmed();
    out.chatUrl = m_chatUrlEdit->text().trimmed();
    out.gitUrl = m_gitUrlEdit->text().trimmed();

    if (m_cookieBlockAll->isChecked())
        out.cookiePolicy = NetzacPreferences::CookiePolicy::BlockAll;
    else if (m_cookieBlockThird->isChecked())
        out.cookiePolicy = NetzacPreferences::CookiePolicy::BlockThirdParty;
    else
        out.cookiePolicy = NetzacPreferences::CookiePolicy::AllowAll;
    out.sessionCookiesOnly = m_cookieSessionChk->isChecked();

    out.cacheMode = static_cast<NetzacPreferences::CacheMode>(m_cachePopup->currentData().toInt());
    out.cacheMaxMb = m_cacheMaxSpin->value();

    if (m_proxyDirect->isChecked())
        out.proxyMode = NetzacPreferences::ProxyMode::Direct;
    else if (m_proxyManual->isChecked())
        out.proxyMode = NetzacPreferences::ProxyMode::Manual;
    else
        out.proxyMode = NetzacPreferences::ProxyMode::System;
    out.proxyType = static_cast<NetzacPreferences::ProxyType>(m_proxyTypePopup->currentData().toInt());
    out.proxyHost = m_proxyHostEdit->text().trimmed();
    out.proxyPort = m_proxyPortSpin->value();

    out.userAgentPreset = m_uaPresetPopup->currentData().toString();
    const QString ua = m_uaEdit->text().trimmed();
    out.userAgent = ua.isEmpty() ? NetzacPreferences::defaultChromeUserAgent() : ua;

    out.adBlockEnabled = m_adBlockChk->isChecked();
    out.adBlockSubscriptions = m_adBlockUrls;
    return out;
}
