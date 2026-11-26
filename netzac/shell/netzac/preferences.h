#pragma once

#include "calmmode.h"

#include <QString>
#include <QStringList>
#include <QVector>

class QWebEngineProfile;

struct NetzacPreferences {
    enum class CookiePolicy {
        AllowAll = 0,
        BlockThirdParty = 1,
        BlockAll = 2,
    };

    enum class CacheMode {
        Disk = 0,
        Memory = 1,
        Disabled = 2,
    };

    enum class ProxyMode {
        Direct = 0,
        System = 1,
        Manual = 2,
    };

    enum class ProxyType {
        Http = 0,
        Socks5 = 1,
    };

    enum class StartupMode {
        HomePage = 0,
        RestoreLastPage = 1,
    };

    enum class LinkOpenMode {
        Tabs = 0,
        Windows = 1,
    };

    enum class TabBarStyle {
        IE45Left = 0,
        ChromeTop = 1,
    };

    struct PresetEntry {
        const char *id;
        QString label;
        const char *value;
    };

    struct PersonalToolbarSubItem {
        QString title;
        QString url;
    };

    struct PersonalToolbarItem {
        QString title;
        QString url;
        QString icon;
        bool isFolder = false;
        QVector<PersonalToolbarSubItem> children;
    };

    QString homeUrl;
    StartupMode startupMode = StartupMode::HomePage;
    LinkOpenMode linkOpenMode = LinkOpenMode::Tabs;
    TabBarStyle tabBarStyle = TabBarStyle::IE45Left;
    bool defaultBrowserPrompted = false;
    QString lastUrl;
    QStringList lastTabs;
    int lastActiveTab = 0;
    QString searchEngineId;
    bool autoloadImages = true;
    bool enableJavaScript = true;
    bool rememberPasswords = true;
    bool showNavButtonText = true;
    CalmConfig calm;

    QVector<PersonalToolbarItem> personalToolbar;
    QString mailUrl;
    QString chatUrl;
    QString gitUrl;

    CookiePolicy cookiePolicy = CookiePolicy::AllowAll;
    bool sessionCookiesOnly = false;

    CacheMode cacheMode = CacheMode::Disk;
    int cacheMaxMb = 50;

    ProxyMode proxyMode = ProxyMode::System;
    ProxyType proxyType = ProxyType::Http;
    QString proxyHost;
    int proxyPort = 8080;

    QString userAgentPreset;
    QString userAgent;

    bool adBlockEnabled = false;
    QStringList adBlockSubscriptions;

    static NetzacPreferences defaults();
    static NetzacPreferences load();
    static void resetAll();
    void save() const;
    static void saveLastUrl(const QString &url);
    static void saveLastTabs(const QStringList &urls, int activeIndex);
    static void ensureDesktopEntryRegistered();
    static bool isDefaultBrowser();
    static bool setAsDefaultBrowser();
    void apply(QWebEngineProfile *profile) const;
    QString searchUrlFor(const QString &query) const;

    static QVector<PresetEntry> searchEnginePresets();
    static QVector<PresetEntry> userAgentPresets();
    static QStringList defaultAdBlockSubscriptions();
    static QVector<PersonalToolbarItem> defaultPersonalToolbar();
    static QString defaultChromeUserAgent();
    static QString userAgentForPreset(const QString &presetId);
};

class PasswordStorage {
public:
    struct Credential {
        QString username;
        QString password;
    };

    static bool isAvailable();
    static bool saveCredential(const QString &host, const QString &username, const QString &password);
    static bool lookupCredential(const QString &host, Credential *out);
    static void clearAll();
};

QWebEngineProfile *netzacProfile();
