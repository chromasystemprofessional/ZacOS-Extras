#pragma once

#include "preferences.h"

#include <QDialog>
#include <QPair>
#include <QString>
#include <QStringList>
#include <QVector>

class QButtonGroup;
class QCheckBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QRadioButton;
class QSpinBox;
class QStackedWidget;

class PlatinumPopupButton;

QPushButton *makeDialogButton(const QString &text, bool isDefault = false, QWidget *parent = nullptr);
QString platinumOpenFileName(
    QWidget *parent,
    const QString &caption,
    const QString &dir = QString(),
    const QString &filter = QString());
QString platinumSaveFileName(
    QWidget *parent,
    const QString &caption,
    const QString &dirOrFile = QString(),
    const QString &filter = QString());

class PlatinumDialog : public QDialog {
    Q_OBJECT

public:
    explicit PlatinumDialog(QWidget *parent = nullptr);
};

class PlatinumAlertDialog : public PlatinumDialog {
    Q_OBJECT

public:
    PlatinumAlertDialog(
        const QString &title,
        const QString &message,
        const QString &kind = QStringLiteral("info"),
        QWidget *parent = nullptr);
};

class AboutDialog : public PlatinumDialog {
    Q_OBJECT

public:
    explicit AboutDialog(QWidget *parent = nullptr);
};

class BookmarksDialog : public PlatinumDialog {
    Q_OBJECT

public:
    explicit BookmarksDialog(QVector<QPair<QString, QString>> *bookmarks, QWidget *parent = nullptr);

signals:
    void openRequested(const QString &url);
    void bookmarksModified();

private slots:
    void openSelected();
    void deleteSelected();
    void importBookmarks();
    void exportBookmarks();

private:
    void refresh();

    QVector<QPair<QString, QString>> *m_bookmarks = nullptr;
    QListWidget *m_list = nullptr;
};

class PreferencesDialog : public PlatinumDialog {
    Q_OBJECT

public:
    PreferencesDialog(
        const NetzacPreferences &prefs,
        const QString &currentUrl = QString(),
        QWidget *parent = nullptr);

    NetzacPreferences preferences() const;
    bool didResetAll() const { return m_resetRequested; }
    void selectCategory(int index);

private:
    QWidget *buildNavigatorPage(const QString &currentUrl);
    QWidget *buildCalmModePage();
    QWidget *buildPersonalToolbarPage();
    QWidget *buildQuickLinksPage();
    QWidget *buildCookiesPage();
    QWidget *buildCachePage();
    QWidget *buildProxiesPage();
    QWidget *buildUserAgentPage();
    QWidget *buildAdBlockerPage();
    void updateProxyControlsEnabled();
    void refreshAdBlockList();
    void addAdBlockListUrl();
    void updateAdBlockStatus();

    NetzacPreferences m_prefs;
    QListWidget *m_categoryList = nullptr;
    QLabel *m_pageHeader = nullptr;
    QStackedWidget *m_stack = nullptr;

    // Navigator
    QLineEdit *m_homeEdit = nullptr;
    PlatinumPopupButton *m_startupPopup = nullptr;
    PlatinumPopupButton *m_searchPopup = nullptr;
    QCheckBox *m_imagesChk = nullptr;
    QCheckBox *m_jsChk = nullptr;
    QCheckBox *m_rememberPasswordsChk = nullptr;
    QLabel *m_defaultBrowserLbl = nullptr;
    QPushButton *m_setDefaultBrowserBtn = nullptr;
    bool m_resetRequested = false;

    // Calm Mode
    PlatinumPopupButton *m_calmJsPopup = nullptr;
    QRadioButton *m_calmLayoutClean = nullptr;
    QRadioButton *m_calmLayoutReader = nullptr;
    QCheckBox *m_calmRemoveScriptsChk = nullptr;
    QCheckBox *m_calmRemoveIframesChk = nullptr;
    QCheckBox *m_calmRemoveVideosChk = nullptr;
    QCheckBox *m_calmRemoveAdsChk = nullptr;
    QCheckBox *m_calmNormalizeTablesChk = nullptr;
    QSpinBox *m_calmMaxImgSpin = nullptr;
    QLineEdit *m_calmExcludedHostsEdit = nullptr;

    // Personal Toolbar
    QVector<NetzacPreferences::PersonalToolbarItem> m_personalItems;
    QListWidget *m_ptList = nullptr;

    // Quick Links (Mail / Chat / Git)
    QLineEdit *m_mailUrlEdit = nullptr;
    QLineEdit *m_chatUrlEdit = nullptr;
    QLineEdit *m_gitUrlEdit = nullptr;

    // Cookies
    QRadioButton *m_cookieAllowAll = nullptr;
    QRadioButton *m_cookieBlockThird = nullptr;
    QRadioButton *m_cookieBlockAll = nullptr;
    QCheckBox *m_cookieSessionChk = nullptr;
    QLabel *m_cookieStatusLbl = nullptr;

    // Cache
    PlatinumPopupButton *m_cachePopup = nullptr;
    QSpinBox *m_cacheMaxSpin = nullptr;
    QLabel *m_cacheStatusLbl = nullptr;

    // Proxies
    QRadioButton *m_proxyDirect = nullptr;
    QRadioButton *m_proxySystem = nullptr;
    QRadioButton *m_proxyManual = nullptr;
    QWidget *m_proxyManualBox = nullptr;
    PlatinumPopupButton *m_proxyTypePopup = nullptr;
    QLineEdit *m_proxyHostEdit = nullptr;
    QSpinBox *m_proxyPortSpin = nullptr;

    // User Agent
    PlatinumPopupButton *m_uaPresetPopup = nullptr;
    QLineEdit *m_uaEdit = nullptr;
    bool m_syncingUa = false;

    // Ad Blocker
    QStringList m_adBlockUrls;
    QCheckBox *m_adBlockChk = nullptr;
    QListWidget *m_adBlockList = nullptr;
    QLabel *m_adBlockStatusLbl = nullptr;
    QPushButton *m_adBlockAddBtn = nullptr;
    QPushButton *m_adBlockRemoveBtn = nullptr;
    QPushButton *m_adBlockDefaultsBtn = nullptr;
    QPushButton *m_adBlockUpdateBtn = nullptr;
};
