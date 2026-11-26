#include "preferences.h"

#include "adblocker.h"
#include "bookmarks.h"
#include "history.h"
#include "i18n.h"
#include "webview.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QNetworkProxy>
#include <QNetworkProxyFactory>
#include <QPointer>
#include <QProcess>
#include <QSettings>
#include <QStandardPaths>
#include <QTextStream>
#include <QUrl>
#include <QWebEngineCookieStore>
#include <QWebEngineProfile>

namespace {

constexpr const char *kChromeLinuxUa =
    "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) NetZacNavigator/1.0 Chrome/200.0.0.0 Safari/900.36";
constexpr const char *kFirefoxLinuxUa =
    "Mozilla/5.0 (X11; Linux x86_64; rv:200.0) Gecko/20100101 NetZacNavigator/1.0 Firefox/200.0";
constexpr const char *kSafariMacUa =
    "Mozilla/5.0 (Macintosh; Intel Mac OS X 12_15_7) AppleWebKit/900.1.15 (KHTML, like Gecko) Version/17.6 NetZacNavigator/1.0 Safari/900.1.15";
constexpr const char *kNetzacMacUa =
    "Mozilla/4.08 (Macintosh; U; PPC) NetZacNavigator/1.0";

}  // namespace

QString NetzacPreferences::defaultChromeUserAgent()
{
    return QString::fromLatin1(kChromeLinuxUa);
}

QVector<NetzacPreferences::PresetEntry> NetzacPreferences::searchEnginePresets()
{
    return {
        {"frogfind", ztr("FrogFind! (Classic Web)"), "https://frogfind.com/?q=%1"},
        {"duckduckgo", ztr("DuckDuckGo"), "https://duckduckgo.com/?q=%1"},
        {"google", ztr("Google"), "https://www.google.com/search?q=%1"},
        {"wiby", ztr("Wiby (Old Web)"), "https://wiby.me/?q=%1"},
        {"wikipedia", ztr("Wikipedia"), "https://en.wikipedia.org/w/index.php?search=%1"},
        {"bing", ztr("Bing"), "https://www.bing.com/search?q=%1"},
    };
}

QVector<NetzacPreferences::PresetEntry> NetzacPreferences::userAgentPresets()
{
    return {
        {"chrome_linux", ztr("Chrome (Linux) — Default"), kChromeLinuxUa},
        {"firefox_linux", ztr("Firefox (Linux)"), kFirefoxLinuxUa},
        {"safari_macos", ztr("Safari (macOS)"), kSafariMacUa},
        {"netzac_mac", ztr("Netzac Navigator"), kNetzacMacUa},
        {"custom", ztr("Custom"), ""},
    };
}

QStringList NetzacPreferences::defaultAdBlockSubscriptions()
{
    return {
        QStringLiteral("https://easylist.to/easylist/easylist.txt"),
        QStringLiteral("https://easylist.to/easylist/easyprivacy.txt"),
        QStringLiteral("https://malware-filter.gitlab.io/malware-filter/urlhaus-filter-domains.txt"),
    };
}

QString NetzacPreferences::userAgentForPreset(const QString &presetId)
{
    for (const auto &entry : userAgentPresets()) {
        if (presetId == QLatin1String(entry.id))
            return QString::fromLatin1(entry.value);
    }
    return QString();
}

QVector<NetzacPreferences::PersonalToolbarItem> NetzacPreferences::defaultPersonalToolbar()
{
    return {
        {ztr("Home"), QStringLiteral("about:netzac"), QStringLiteral("home"), false, {}},
        {ztr("FrogFind"), QStringLiteral("https://frogfind.com"), QStringLiteral("search"), false, {}},
        {ztr("Wiby"), QStringLiteral("https://wiby.me"), QStringLiteral("guide"), false, {}},
        {ztr("68k News"), QStringLiteral("http://68k.news"), QStringLiteral("page"), false, {}},
        {ztr("The Old Net"), QStringLiteral("https://theoldnet.com"), QStringLiteral("page"), false, {}},
        {ztr("Internet"), QString(), QStringLiteral("folder"), true, {
            {ztr("FrogFind!"), QStringLiteral("https://frogfind.com")},
            {ztr("Wiby"), QStringLiteral("https://wiby.me")},
            {ztr("68k.news"), QStringLiteral("http://68k.news")},
            {ztr("NPR Text"), QStringLiteral("https://text.npr.org")},
            {ztr("CNN Lite"), QStringLiteral("https://lite.cnn.com")},
            {ztr("Wikipedia"), QStringLiteral("https://en.wikipedia.org")},
            {ztr("The Old Net"), QStringLiteral("https://theoldnet.com")},
            {ztr("Internet Archive"), QStringLiteral("https://web.archive.org")},
        }},
    };
}

NetzacPreferences NetzacPreferences::defaults()
{
    NetzacPreferences prefs;
    prefs.homeUrl = QStringLiteral("about:netzac");
    prefs.startupMode = StartupMode::HomePage;
    prefs.linkOpenMode = LinkOpenMode::Tabs;
    prefs.tabBarStyle = TabBarStyle::IE45Left;
    prefs.defaultBrowserPrompted = false;
    prefs.lastUrl.clear();
    prefs.searchEngineId = QStringLiteral("duckduckgo");
    prefs.autoloadImages = true;
    prefs.enableJavaScript = true;
    prefs.rememberPasswords = true;
    prefs.showNavButtonText = true;
    prefs.calm = CalmConfig();
    prefs.personalToolbar = defaultPersonalToolbar();
    prefs.mailUrl.clear();
    prefs.chatUrl.clear();
    prefs.gitUrl.clear();
    prefs.cookiePolicy = CookiePolicy::AllowAll;
    prefs.sessionCookiesOnly = false;
    prefs.cacheMode = CacheMode::Disk;
    prefs.cacheMaxMb = 50;
    prefs.proxyMode = ProxyMode::System;
    prefs.proxyType = ProxyType::Http;
    prefs.proxyHost.clear();
    prefs.proxyPort = 8080;
    prefs.userAgentPreset = QStringLiteral("chrome_linux");
    prefs.userAgent = defaultChromeUserAgent();
    prefs.adBlockEnabled = false;
    prefs.adBlockSubscriptions = defaultAdBlockSubscriptions();
    return prefs;
}

NetzacPreferences NetzacPreferences::load()
{
    NetzacPreferences prefs = defaults();
    QSettings s(QStringLiteral("zacos9"), QStringLiteral("netzac"));

    prefs.homeUrl = s.value(QStringLiteral("navigator/homeUrl"), prefs.homeUrl).toString().trimmed();
    if (prefs.homeUrl.isEmpty())
        prefs.homeUrl = QStringLiteral("about:netzac");
    const int startupVal = qBound(0, s.value(QStringLiteral("navigator/startupMode"), int(prefs.startupMode)).toInt(), 1);
    prefs.startupMode = static_cast<StartupMode>(startupVal);
    const int linkOpenVal = qBound(0, s.value(QStringLiteral("navigator/linkOpenMode"), int(prefs.linkOpenMode)).toInt(), 1);
    prefs.linkOpenMode = static_cast<LinkOpenMode>(linkOpenVal);
    const int tabStyleVal = qBound(0, s.value(QStringLiteral("navigator/tabBarStyle"), int(prefs.tabBarStyle)).toInt(), 1);
    prefs.tabBarStyle = static_cast<TabBarStyle>(tabStyleVal);
    prefs.defaultBrowserPrompted = s.value(QStringLiteral("navigator/defaultBrowserPrompted"), prefs.defaultBrowserPrompted).toBool();
    prefs.lastUrl = s.value(QStringLiteral("navigator/lastUrl"), prefs.lastUrl).toString().trimmed();
    const QStringList rawTabs = s.value(QStringLiteral("navigator/lastTabs")).toStringList();
    for (const QString &t : rawTabs) {
        const QString clean = t.trimmed();
        if (!clean.isEmpty()
            && clean != QLatin1String("about:blank")
            && clean != QLatin1String("https://localhost/")
            && !clean.startsWith(QLatin1String("data:"))) {
            prefs.lastTabs.append(clean);
        }
    }
    prefs.lastActiveTab = qMax(0, s.value(QStringLiteral("navigator/lastActiveTab"), 0).toInt());
    prefs.searchEngineId = s.value(QStringLiteral("navigator/searchEngine"), prefs.searchEngineId).toString();
    prefs.autoloadImages = s.value(QStringLiteral("navigator/autoloadImages"), prefs.autoloadImages).toBool();
    prefs.enableJavaScript = s.value(QStringLiteral("navigator/enableJavaScript"), prefs.enableJavaScript).toBool();
    prefs.rememberPasswords = s.value(QStringLiteral("navigator/rememberPasswords"), prefs.rememberPasswords).toBool();
    prefs.showNavButtonText = s.value(QStringLiteral("navigator/showNavButtonText"), prefs.showNavButtonText).toBool();

    const int calmJsVal = qBound(0, s.value(QStringLiteral("calm/jsDetection"), int(prefs.calm.jsDetection)).toInt(), 2);
    prefs.calm.jsDetection = static_cast<CalmJsDetection>(calmJsVal);
    const int calmLayoutVal = qBound(0, s.value(QStringLiteral("calm/layoutMode"), int(prefs.calm.layoutMode)).toInt(), 1);
    prefs.calm.layoutMode = static_cast<CalmLayoutMode>(calmLayoutVal);
    prefs.calm.removeScripts = s.value(QStringLiteral("calm/removeScripts"), prefs.calm.removeScripts).toBool();
    prefs.calm.removeIframes = s.value(QStringLiteral("calm/removeIframes"), prefs.calm.removeIframes).toBool();
    prefs.calm.removeVideos = s.value(QStringLiteral("calm/removeVideos"), prefs.calm.removeVideos).toBool();
    prefs.calm.removeAds = s.value(QStringLiteral("calm/removeAds"), prefs.calm.removeAds).toBool();
    prefs.calm.normalizeTables = s.value(QStringLiteral("calm/normalizeTables"), prefs.calm.normalizeTables).toBool();
    prefs.calm.maxImageWidth = qBound(240, s.value(QStringLiteral("calm/maxImageWidth"), prefs.calm.maxImageWidth).toInt(), 1920);
    if (s.contains(QStringLiteral("calm/excludedHosts"))) {
        QStringList parsed;
        const QStringList rawHosts = s.value(QStringLiteral("calm/excludedHosts")).toStringList();
        for (const QString &h : rawHosts) {
            const QString norm = CalmPolicy::normalizeHost(h);
            if (!norm.isEmpty() && !parsed.contains(norm))
                parsed.append(norm);
        }
        prefs.calm.excludedHosts = parsed;
    }

    if (s.contains(QStringLiteral("personalToolbar/size"))) {
        prefs.personalToolbar.clear();
        const int count = s.beginReadArray(QStringLiteral("personalToolbar"));
        for (int i = 0; i < count; ++i) {
            s.setArrayIndex(i);
            const QString title = s.value(QStringLiteral("title")).toString().trimmed();
            const QString url = s.value(QStringLiteral("url")).toString().trimmed();
            const QString icon = s.value(QStringLiteral("icon"), QStringLiteral("page")).toString().trimmed();
            const bool isFolder = s.value(QStringLiteral("isFolder"), icon == QLatin1String("folder")).toBool();

            QVector<PersonalToolbarSubItem> children;
            if (isFolder) {
                const int subCount = s.beginReadArray(QStringLiteral("children"));
                for (int j = 0; j < subCount; ++j) {
                    s.setArrayIndex(j);
                    const QString st = s.value(QStringLiteral("title")).toString().trimmed();
                    const QString su = s.value(QStringLiteral("url")).toString().trimmed();
                    if (!st.isEmpty() && !su.isEmpty())
                        children.append({st, su});
                }
                s.endArray();
            }

            if (!title.isEmpty() && (isFolder || !url.isEmpty())) {
                prefs.personalToolbar.append({
                    title,
                    isFolder ? QString() : url,
                    isFolder ? QStringLiteral("folder") : (icon.isEmpty() ? QStringLiteral("page") : icon),
                    isFolder,
                    children,
                });
            }
        }
        s.endArray();
    }

    prefs.mailUrl = s.value(QStringLiteral("quickLinks/mailUrl"), prefs.mailUrl).toString().trimmed();
    prefs.chatUrl = s.value(QStringLiteral("quickLinks/chatUrl"), prefs.chatUrl).toString().trimmed();
    prefs.gitUrl = s.value(QStringLiteral("quickLinks/gitUrl"), prefs.gitUrl).toString().trimmed();

    const int cookieVal = qBound(0, s.value(QStringLiteral("cookies/policy"), int(prefs.cookiePolicy)).toInt(), 2);
    prefs.cookiePolicy = static_cast<CookiePolicy>(cookieVal);
    prefs.sessionCookiesOnly = s.value(QStringLiteral("cookies/sessionOnly"), prefs.sessionCookiesOnly).toBool();

    const int cacheVal = qBound(0, s.value(QStringLiteral("cache/mode"), int(prefs.cacheMode)).toInt(), 2);
    prefs.cacheMode = static_cast<CacheMode>(cacheVal);
    prefs.cacheMaxMb = qBound(1, s.value(QStringLiteral("cache/maxMb"), prefs.cacheMaxMb).toInt(), 2048);

    const int proxyModeVal = qBound(0, s.value(QStringLiteral("proxy/mode"), int(prefs.proxyMode)).toInt(), 2);
    prefs.proxyMode = static_cast<ProxyMode>(proxyModeVal);
    const int proxyTypeVal = qBound(0, s.value(QStringLiteral("proxy/type"), int(prefs.proxyType)).toInt(), 1);
    prefs.proxyType = static_cast<ProxyType>(proxyTypeVal);
    prefs.proxyHost = s.value(QStringLiteral("proxy/host"), prefs.proxyHost).toString().trimmed();
    prefs.proxyPort = qBound(1, s.value(QStringLiteral("proxy/port"), prefs.proxyPort).toInt(), 65535);

    prefs.userAgentPreset = s.value(QStringLiteral("userAgent/preset"), prefs.userAgentPreset).toString();
    prefs.userAgent = s.value(QStringLiteral("userAgent/value"), prefs.userAgent).toString().trimmed();
    if (prefs.userAgent.isEmpty()) {
        const QString presetUa = userAgentForPreset(prefs.userAgentPreset);
        prefs.userAgent = presetUa.isEmpty() ? defaultChromeUserAgent() : presetUa;
    }

    prefs.adBlockEnabled = s.value(QStringLiteral("adblock/enabled"), prefs.adBlockEnabled).toBool();
    if (s.contains(QStringLiteral("adblock/lists"))) {
        prefs.adBlockSubscriptions.clear();
        const QStringList rawLists = s.value(QStringLiteral("adblock/lists")).toStringList();
        for (const QString &raw : rawLists) {
            const QString url = raw.trimmed();
            if (!url.isEmpty() && !prefs.adBlockSubscriptions.contains(url))
                prefs.adBlockSubscriptions.append(url);
        }
    }
    return prefs;
}

void NetzacPreferences::save() const
{
    QSettings s(QStringLiteral("zacos9"), QStringLiteral("netzac"));
    s.setValue(QStringLiteral("navigator/homeUrl"), homeUrl);
    s.setValue(QStringLiteral("navigator/startupMode"), int(startupMode));
    s.setValue(QStringLiteral("navigator/linkOpenMode"), int(linkOpenMode));
    s.setValue(QStringLiteral("navigator/tabBarStyle"), int(tabBarStyle));
    s.setValue(QStringLiteral("navigator/defaultBrowserPrompted"), defaultBrowserPrompted);
    if (!lastUrl.isEmpty())
        s.setValue(QStringLiteral("navigator/lastUrl"), lastUrl);
    s.setValue(QStringLiteral("navigator/searchEngine"), searchEngineId);
    s.setValue(QStringLiteral("navigator/autoloadImages"), autoloadImages);
    s.setValue(QStringLiteral("navigator/enableJavaScript"), enableJavaScript);
    s.setValue(QStringLiteral("navigator/rememberPasswords"), rememberPasswords);
    s.setValue(QStringLiteral("navigator/showNavButtonText"), showNavButtonText);

    s.setValue(QStringLiteral("calm/jsDetection"), int(calm.jsDetection));
    s.setValue(QStringLiteral("calm/layoutMode"), int(calm.layoutMode));
    s.setValue(QStringLiteral("calm/removeScripts"), calm.removeScripts);
    s.setValue(QStringLiteral("calm/removeIframes"), calm.removeIframes);
    s.setValue(QStringLiteral("calm/removeVideos"), calm.removeVideos);
    s.setValue(QStringLiteral("calm/removeAds"), calm.removeAds);
    s.setValue(QStringLiteral("calm/normalizeTables"), calm.normalizeTables);
    s.setValue(QStringLiteral("calm/maxImageWidth"), calm.maxImageWidth);
    s.setValue(QStringLiteral("calm/excludedHosts"), calm.excludedHosts);

    s.beginWriteArray(QStringLiteral("personalToolbar"), int(personalToolbar.size()));
    for (int i = 0; i < personalToolbar.size(); ++i) {
        s.setArrayIndex(i);
        const PersonalToolbarItem &entry = personalToolbar.at(i);
        s.setValue(QStringLiteral("title"), entry.title);
        s.setValue(QStringLiteral("url"), entry.url);
        s.setValue(QStringLiteral("icon"), entry.icon);
        s.setValue(QStringLiteral("isFolder"), entry.isFolder);
        if (entry.isFolder) {
            s.beginWriteArray(QStringLiteral("children"), int(entry.children.size()));
            for (int j = 0; j < entry.children.size(); ++j) {
                s.setArrayIndex(j);
                s.setValue(QStringLiteral("title"), entry.children.at(j).title);
                s.setValue(QStringLiteral("url"), entry.children.at(j).url);
            }
            s.endArray();
        }
    }
    s.endArray();

    s.setValue(QStringLiteral("quickLinks/mailUrl"), mailUrl);
    s.setValue(QStringLiteral("quickLinks/chatUrl"), chatUrl);
    s.setValue(QStringLiteral("quickLinks/gitUrl"), gitUrl);

    s.setValue(QStringLiteral("cookies/policy"), int(cookiePolicy));
    s.setValue(QStringLiteral("cookies/sessionOnly"), sessionCookiesOnly);

    s.setValue(QStringLiteral("cache/mode"), int(cacheMode));
    s.setValue(QStringLiteral("cache/maxMb"), cacheMaxMb);

    s.setValue(QStringLiteral("proxy/mode"), int(proxyMode));
    s.setValue(QStringLiteral("proxy/type"), int(proxyType));
    s.setValue(QStringLiteral("proxy/host"), proxyHost);
    s.setValue(QStringLiteral("proxy/port"), proxyPort);

    s.setValue(QStringLiteral("userAgent/preset"), userAgentPreset);
    s.setValue(QStringLiteral("userAgent/value"), userAgent);

    s.setValue(QStringLiteral("adblock/enabled"), adBlockEnabled);
    s.setValue(QStringLiteral("adblock/lists"), adBlockSubscriptions);
}

void NetzacPreferences::resetAll()
{
    QSettings s(QStringLiteral("zacos9"), QStringLiteral("netzac"));
    const QString settingsFile = s.fileName();
    s.clear();
    s.sync();
    if (!settingsFile.isEmpty())
        QFile::remove(settingsFile);

    QFile::remove(BookmarkStorage::defaultFilePath());
    BookmarkStorage::save(BookmarkStorage::defaultBookmarks());
    HistoryStorage::save({});
    PasswordStorage::clearAll();

    if (QWebEngineProfile *profile = netzacProfile()) {
        if (QWebEngineCookieStore *cookies = profile->cookieStore())
            cookies->deleteAllCookies();
        profile->clearHttpCache();
    }
    defaults().save();
}

void NetzacPreferences::saveLastUrl(const QString &url)
{
    const QString clean = url.trimmed();
    if (clean.isEmpty()
        || clean == QLatin1String("about:blank")
        || clean == QLatin1String("https://localhost/")
        || clean.startsWith(QLatin1String("data:"))) {
        return;
    }
    QSettings s(QStringLiteral("zacos9"), QStringLiteral("netzac"));
    s.setValue(QStringLiteral("navigator/lastUrl"), clean);
}

void NetzacPreferences::saveLastTabs(const QStringList &urls, int activeIndex)
{
    QStringList cleaned;
    int mappedActive = 0;
    for (int i = 0; i < urls.size(); ++i) {
        const QString clean = urls.at(i).trimmed();
        if (clean.isEmpty()
            || clean == QLatin1String("about:blank")
            || clean == QLatin1String("https://localhost/")
            || clean.startsWith(QLatin1String("data:"))) {
            continue;
        }
        if (i == activeIndex)
            mappedActive = cleaned.size();
        cleaned.append(clean);
    }
    QSettings s(QStringLiteral("zacos9"), QStringLiteral("netzac"));
    s.setValue(QStringLiteral("navigator/lastTabs"), cleaned);
    s.setValue(QStringLiteral("navigator/lastActiveTab"), mappedActive);
    if (!cleaned.isEmpty() && mappedActive >= 0 && mappedActive < cleaned.size())
        s.setValue(QStringLiteral("navigator/lastUrl"), cleaned.at(mappedActive));
}

void NetzacPreferences::ensureDesktopEntryRegistered()
{
    QString userApps = QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation);
    if (userApps.isEmpty())
        userApps = QDir::homePath() + QStringLiteral("/.local/share/applications");

    const QString userDesktop = userApps + QStringLiteral("/netzac.desktop");
    const QString legacyDesktop = userApps + QStringLiteral("/zacos9-netzac.desktop");
    if (QFileInfo::exists(legacyDesktop))
        QFile::remove(legacyDesktop);

    const bool inPath = !QStandardPaths::findExecutable(QStringLiteral("zacos9-netzac")).isEmpty();
    const bool hasSystemDesktop =
        QFileInfo::exists(QStringLiteral("/usr/share/applications/netzac.desktop"))
        || QFileInfo::exists(QStringLiteral("/usr/local/share/applications/netzac.desktop"));

    if (inPath && hasSystemDesktop && !QFileInfo::exists(userDesktop))
        return;

    const QString exePath = QCoreApplication::applicationFilePath();
    const QString execLine = exePath.isEmpty()
        ? QStringLiteral("zacos9-netzac %u")
        : (QStringLiteral("\"") + exePath + QStringLiteral("\" %u"));

    QDir().mkpath(userApps);
    QFile f(userDesktop);
    if (f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        QTextStream out(&f);
        out << "[Desktop Entry]\n"
            << "Type=Application\n"
            << "Name=Netzac\n"
            << "Comment=Browse the World Wide Web in classic Platinum style\n"
            << "Exec=" << execLine << "\n"
            << "Icon=netzac\n"
            << "Terminal=false\n"
            << "Categories=Network;WebBrowser;\n"
            << "MimeType=text/html;text/xml;application/xhtml+xml;x-scheme-handler/http;x-scheme-handler/https;x-scheme-handler/about;\n";
        f.close();
    }

    if (QDir(userApps).exists())
        QProcess::execute(QStringLiteral("update-desktop-database"), {userApps});
}

bool NetzacPreferences::isDefaultBrowser()
{
    QProcess proc;
    proc.start(QStringLiteral("xdg-settings"), {QStringLiteral("get"), QStringLiteral("default-web-browser")});
    if (proc.waitForFinished(2000) && proc.exitStatus() == QProcess::NormalExit && proc.exitCode() == 0) {
        const QString out = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
        if (out == QLatin1String("netzac.desktop"))
            return true;
    }

    QProcess mimeProc;
    mimeProc.start(QStringLiteral("xdg-mime"), {QStringLiteral("query"), QStringLiteral("default"), QStringLiteral("x-scheme-handler/http")});
    if (mimeProc.waitForFinished(2000) && mimeProc.exitStatus() == QProcess::NormalExit && mimeProc.exitCode() == 0) {
        const QString out = QString::fromUtf8(mimeProc.readAllStandardOutput()).trimmed();
        if (out == QLatin1String("netzac.desktop"))
            return true;
    }
    return false;
}

bool NetzacPreferences::setAsDefaultBrowser()
{
    ensureDesktopEntryRegistered();

    bool ok = false;
    QProcess settingsProc;
    settingsProc.start(
        QStringLiteral("xdg-settings"),
        {QStringLiteral("set"), QStringLiteral("default-web-browser"), QStringLiteral("netzac.desktop")});
    if (settingsProc.waitForFinished(3000)
        && settingsProc.exitStatus() == QProcess::NormalExit
        && settingsProc.exitCode() == 0) {
        ok = true;
    }

    QProcess mimeProc;
    mimeProc.start(
        QStringLiteral("xdg-mime"),
        {
            QStringLiteral("default"),
            QStringLiteral("netzac.desktop"),
            QStringLiteral("x-scheme-handler/http"),
            QStringLiteral("x-scheme-handler/https"),
            QStringLiteral("text/html"),
            QStringLiteral("application/xhtml+xml"),
        });
    if (mimeProc.waitForFinished(3000)
        && mimeProc.exitStatus() == QProcess::NormalExit
        && mimeProc.exitCode() == 0) {
        ok = true;
    }

    const QString userApps = QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation);
    if (!userApps.isEmpty() && QDir(userApps).exists())
        QProcess::execute(QStringLiteral("update-desktop-database"), {userApps});

    return ok || isDefaultBrowser();
}

bool PasswordStorage::isAvailable()
{
    return !QStandardPaths::findExecutable(QStringLiteral("secret-tool")).isEmpty();
}

bool PasswordStorage::saveCredential(const QString &host, const QString &username, const QString &password)
{
    const QString cleanHost = host.trimmed().toLower();
    const QString cleanUser = username.trimmed();
    if (cleanHost.isEmpty() || cleanUser.isEmpty() || password.isEmpty() || !isAvailable())
        return false;

    QProcess proc;
    proc.start(
        QStringLiteral("secret-tool"),
        {
            QStringLiteral("store"),
            QStringLiteral("--label=Netzac (%1)").arg(cleanHost),
            QStringLiteral("application"), QStringLiteral("netzac"),
            QStringLiteral("origin"), cleanHost,
            QStringLiteral("username"), cleanUser,
        });
    if (!proc.waitForStarted(1500))
        return false;
    proc.write(password.toUtf8());
    proc.closeWriteChannel();
    return proc.waitForFinished(3000) && proc.exitStatus() == QProcess::NormalExit && proc.exitCode() == 0;
}

bool PasswordStorage::lookupCredential(const QString &host, Credential *out)
{
    const QString cleanHost = host.trimmed().toLower();
    if (cleanHost.isEmpty() || !out || !isAvailable())
        return false;

    QProcess proc;
    proc.setProcessChannelMode(QProcess::MergedChannels);
    proc.start(
        QStringLiteral("secret-tool"),
        {
            QStringLiteral("search"),
            QStringLiteral("--all"),
            QStringLiteral("application"), QStringLiteral("netzac"),
            QStringLiteral("origin"), cleanHost,
        });
    if (!proc.waitForFinished(2500) || proc.exitStatus() != QProcess::NormalExit || proc.exitCode() != 0)
        return false;

    QString foundUser;
    QString foundPass;
    const QString output = QString::fromUtf8(proc.readAllStandardOutput());
    const QStringList lines = output.split(QLatin1Char('\n'));
    for (const QString &rawLine : lines) {
        const QString line = rawLine.trimmed();
        if (line.startsWith(QLatin1String("attribute.username = "))) {
            foundUser = line.mid(21).trimmed();
        } else if (line.startsWith(QLatin1String("secret = "))) {
            foundPass = line.mid(9);
        }
        if (!foundUser.isEmpty() && !foundPass.isEmpty()) {
            out->username = foundUser;
            out->password = foundPass;
            return true;
        }
    }
    return false;
}

void PasswordStorage::clearAll()
{
    if (!isAvailable())
        return;
    QProcess::execute(
        QStringLiteral("secret-tool"),
        {QStringLiteral("clear"), QStringLiteral("application"), QStringLiteral("netzac")});
}

QWebEngineProfile *netzacProfile()
{
    static QPointer<QWebEngineProfile> s_profile;
    if (!s_profile) {
        s_profile = new QWebEngineProfile(
            QStringLiteral("netzac"), QCoreApplication::instance());
        QString dataDir = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
        if (dataDir.isEmpty())
            dataDir = QDir::homePath() + QStringLiteral("/.local/share");
        const QString storagePath = dataDir + QStringLiteral("/zacos9/netzac/profile");
        QDir().mkpath(storagePath);
        s_profile->setPersistentStoragePath(storagePath);

        QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation);
        if (cacheDir.isEmpty())
            cacheDir = QDir::homePath() + QStringLiteral("/.cache");
        const QString cachePath = cacheDir + QStringLiteral("/zacos9/netzac/cache");
        QDir().mkpath(cachePath);
        s_profile->setCachePath(cachePath);
        s_profile->setPersistentPermissionsPolicy(
            QWebEngineProfile::PersistentPermissionsPolicy::StoreOnDisk);
        installNetzacScripts(s_profile);
    }
    return s_profile;
}

void NetzacPreferences::apply(QWebEngineProfile *profile) const
{
    // Proxy configuration (applies process-wide for Qt/WebEngine)
    switch (proxyMode) {
    case ProxyMode::Direct:
        QNetworkProxyFactory::setUseSystemConfiguration(false);
        QNetworkProxy::setApplicationProxy(QNetworkProxy(QNetworkProxy::NoProxy));
        break;
    case ProxyMode::System:
        QNetworkProxy::setApplicationProxy(QNetworkProxy(QNetworkProxy::DefaultProxy));
        QNetworkProxyFactory::setUseSystemConfiguration(true);
        break;
    case ProxyMode::Manual: {
        QNetworkProxyFactory::setUseSystemConfiguration(false);
        const QString host = proxyHost.trimmed();
        if (host.isEmpty()) {
            QNetworkProxy::setApplicationProxy(QNetworkProxy(QNetworkProxy::NoProxy));
        } else {
            const auto qtype = (proxyType == ProxyType::Socks5)
                ? QNetworkProxy::Socks5Proxy
                : QNetworkProxy::HttpProxy;
            QNetworkProxy::setApplicationProxy(QNetworkProxy(qtype, host, quint16(qBound(1, proxyPort, 65535))));
        }
        break;
    }
    }

    if (!profile)
        return;

    // Persistent ZacOS 9 storage & cache paths (ensures cookies & logins survive restarts)
    if (!profile->isOffTheRecord()) {
        QString dataDir = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
        if (dataDir.isEmpty())
            dataDir = QDir::homePath() + QStringLiteral("/.local/share");
        const QString storagePath = dataDir + QStringLiteral("/zacos9/netzac/profile");
        QDir().mkpath(storagePath);
        if (profile->persistentStoragePath() != storagePath)
            profile->setPersistentStoragePath(storagePath);

        QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation);
        if (cacheDir.isEmpty())
            cacheDir = QDir::homePath() + QStringLiteral("/.cache");
        const QString cachePath = cacheDir + QStringLiteral("/zacos9/netzac/cache");
        QDir().mkpath(cachePath);
        if (profile->cachePath() != cachePath)
            profile->setCachePath(cachePath);

        profile->setPersistentPermissionsPolicy(
            QWebEngineProfile::PersistentPermissionsPolicy::StoreOnDisk);
    }

    // User Agent
    const QString ua = userAgent.trimmed().isEmpty() ? defaultChromeUserAgent() : userAgent.trimmed();
    profile->setHttpUserAgent(ua);

    // Cache
    switch (cacheMode) {
    case CacheMode::Disk:
        profile->setHttpCacheType(QWebEngineProfile::DiskHttpCache);
        break;
    case CacheMode::Memory:
        profile->setHttpCacheType(QWebEngineProfile::MemoryHttpCache);
        break;
    case CacheMode::Disabled:
        profile->setHttpCacheType(QWebEngineProfile::NoCache);
        break;
    }
    profile->setHttpCacheMaximumSize(qBound(1, cacheMaxMb, 2048) * 1024 * 1024);

    // Cookies (ForcePersistentCookies keeps session login cookies across restarts)
    if (cookiePolicy == CookiePolicy::BlockAll || sessionCookiesOnly) {
        profile->setPersistentCookiesPolicy(QWebEngineProfile::NoPersistentCookies);
    } else {
        profile->setPersistentCookiesPolicy(QWebEngineProfile::ForcePersistentCookies);
    }

    const CookiePolicy policy = cookiePolicy;
    if (QWebEngineCookieStore *store = profile->cookieStore()) {
        store->setCookieFilter([policy](const QWebEngineCookieStore::FilterRequest &req) {
            switch (policy) {
            case CookiePolicy::AllowAll:
                return true;
            case CookiePolicy::BlockThirdParty:
                return !req.thirdParty;
            case CookiePolicy::BlockAll:
                return false;
            }
            return true;
        });
    }

    NetzacAdBlocker *blocker = NetzacAdBlocker::instance();
    blocker->install(profile);
    blocker->configure(adBlockEnabled, adBlockSubscriptions);
}

QString NetzacPreferences::searchUrlFor(const QString &query) const
{
    QString tmpl = QStringLiteral("https://duckduckgo.com/?q=%1");
    for (const auto &entry : searchEnginePresets()) {
        if (searchEngineId == QLatin1String(entry.id)) {
            tmpl = QString::fromLatin1(entry.value);
            break;
        }
    }
    return tmpl.arg(QString::fromUtf8(QUrl::toPercentEncoding(query)));
}
