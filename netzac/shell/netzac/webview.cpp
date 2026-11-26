#include "webview.h"

#include <QApplication>
#include <QContextMenuEvent>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPalette>
#include <QUuid>
#include <QWebEngineContextMenuRequest>
#include <QWebEngineProfile>
#include <QWebEngineScript>
#include <QWebEngineScriptCollection>

namespace {

const QString &popupTokenPrefix()
{
    static const QString prefix = QStringLiteral("__NETZAC_POPUP_%1__:")
        .arg(QUuid::createUuid().toString(QUuid::Id128));
    return prefix;
}

struct PlatinumAccentRamp {
    const char *id;
    const char *gripHi;
    const char *light;
    const char *body;
    const char *dark;
    const char *shadow;
};

PlatinumAccentRamp currentZacosAccentRamp()
{
    static constexpr PlatinumAccentRamp kAccents[] = {
        {"lavender", "#eeeeee", "#ccccff", "#9999ff", "#6666cc", "#333399"},
        {"graphite", "#eeeeee", "#e6e6e6", "#cccccc", "#999999", "#666666"},
        {"gold",     "#eeeeee", "#fff2cc", "#ffe699", "#ccb366", "#998033"},
        {"bronze",   "#eeeeee", "#f4e3d7", "#e8c7b0", "#b5947d", "#82614a"},
        {"ivy",      "#ccffcc", "#66ff99", "#33cc66", "#339966", "#006633"},
        {"ocean",    "#eeeeee", "#cfedfc", "#9edbfa", "#6ba8c7", "#387594"},
        {"rose",     "#eeeeee", "#f9d2df", "#f2a6bf", "#bf738c", "#8c4059"},
        {"plum",     "#eeeeee", "#f0d6f5", "#e0adeb", "#ad7ab8", "#7a4785"},
    };

    QString configRoot = qEnvironmentVariable("XDG_CONFIG_HOME");
    if (configRoot.isEmpty())
        configRoot = QDir::homePath() + QStringLiteral("/.config");

    QString accentId = QStringLiteral("lavender");
    QFile desktopConf(configRoot + QStringLiteral("/zacos9/desktop.conf"));
    if (desktopConf.open(QIODevice::ReadOnly | QIODevice::Text)) {
        while (!desktopConf.atEnd()) {
            const QString line = QString::fromUtf8(desktopConf.readLine()).trimmed();
            if (line.startsWith(QLatin1String("accent="))) {
                accentId = line.mid(7).trimmed().toLower();
                break;
            }
        }
    }

    for (const PlatinumAccentRamp &ramp : kAccents) {
        if (accentId == QLatin1String(ramp.id))
            return ramp;
    }
    return kAccents[0];
}

QString platinumScrollbarCss()
{
    const PlatinumAccentRamp ac = currentZacosAccentRamp();
    auto esc = [](const char *hex) {
        return QString::fromLatin1(hex).replace(QLatin1Char('#'), QStringLiteral("%23"));
    };

    // Note: Do NOT set scrollbar-color to explicit colors here; in Chromium, a non-auto
    // scrollbar-color disables all ::-webkit-scrollbar pseudo-elements (hiding arrow buttons
    // and the 3D Platinum thumb/caret). Setting both to 'auto !important' neutralizes page
    // overrides so the full ::-webkit-scrollbar Platinum styling always renders.
    QString css = QStringLiteral(R"CSS(
*, *::before, *::after, html, body {
  scrollbar-width: auto !important;
  scrollbar-color: auto !important;
}
::-webkit-scrollbar {
  width: 16px !important;
  height: 16px !important;
  background-color: #aaaaaa !important;
  -webkit-appearance: none !important;
}
::-webkit-scrollbar-track {
  background-color: #aaaaaa !important;
  border-radius: 0 !important;
}
::-webkit-scrollbar-track:vertical {
  border-left: 1px solid #000000 !important;
  border-right: 1px solid #000000 !important;
  border-top: none !important;
  border-bottom: none !important;
  box-shadow: inset 1px 1px 0 #777777, inset 2px 2px 0 #888888, inset -1px 0 0 #cccccc, inset -2px 0 0 #bbbbbb !important;
}
::-webkit-scrollbar-track:horizontal {
  border-top: 1px solid #000000 !important;
  border-bottom: 1px solid #000000 !important;
  border-left: none !important;
  border-right: none !important;
  box-shadow: inset 1px 1px 0 #777777, inset 2px 2px 0 #888888, inset 0 -1px 0 #cccccc, inset 0 -2px 0 #bbbbbb !important;
}
::-webkit-scrollbar-track-piece {
  background-color: transparent !important;
  border: none !important;
}
::-webkit-scrollbar-thumb {
  background-color: __BODY__ !important;
  border: 1px solid #000000 !important;
  border-radius: 0 !important;
  background-repeat: no-repeat !important;
  background-position: center !important;
}
::-webkit-scrollbar-thumb:vertical {
  min-height: 18px !important;
  box-shadow: inset 1px 1px 0 __LIGHT__, inset -1px -1px 0 __DARK__, 0 1px 0 #777777, 0 2px 0 #888888 !important;
  background-image: url("data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' width='8' height='8' shape-rendering='crispEdges'%3E%3Crect x='0' y='0' width='1' height='1' fill='__GRIP_HI_ENC__'/%3E%3Crect x='1' y='0' width='6' height='1' fill='__LIGHT_ENC__'/%3E%3Crect x='1' y='1' width='7' height='1' fill='__SHADOW_ENC__'/%3E%3Crect x='0' y='2' width='1' height='1' fill='__GRIP_HI_ENC__'/%3E%3Crect x='1' y='2' width='6' height='1' fill='__LIGHT_ENC__'/%3E%3Crect x='1' y='3' width='7' height='1' fill='__SHADOW_ENC__'/%3E%3Crect x='0' y='4' width='1' height='1' fill='__GRIP_HI_ENC__'/%3E%3Crect x='1' y='4' width='6' height='1' fill='__LIGHT_ENC__'/%3E%3Crect x='1' y='5' width='7' height='1' fill='__SHADOW_ENC__'/%3E%3Crect x='0' y='6' width='1' height='1' fill='__GRIP_HI_ENC__'/%3E%3Crect x='1' y='6' width='6' height='1' fill='__LIGHT_ENC__'/%3E%3Crect x='1' y='7' width='7' height='1' fill='__SHADOW_ENC__'/%3E%3C/svg%3E") !important;
}
::-webkit-scrollbar-thumb:horizontal {
  min-width: 18px !important;
  box-shadow: inset 1px 1px 0 __LIGHT__, inset -1px -1px 0 __DARK__, 1px 0 0 #777777, 2px 0 0 #888888 !important;
  background-image: url("data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' width='8' height='8' shape-rendering='crispEdges'%3E%3Crect x='0' y='0' width='1' height='1' fill='__GRIP_HI_ENC__'/%3E%3Crect x='0' y='1' width='1' height='6' fill='__LIGHT_ENC__'/%3E%3Crect x='1' y='1' width='1' height='7' fill='__SHADOW_ENC__'/%3E%3Crect x='2' y='0' width='1' height='1' fill='__GRIP_HI_ENC__'/%3E%3Crect x='2' y='1' width='1' height='6' fill='__LIGHT_ENC__'/%3E%3Crect x='3' y='1' width='1' height='7' fill='__SHADOW_ENC__'/%3E%3Crect x='4' y='0' width='1' height='1' fill='__GRIP_HI_ENC__'/%3E%3Crect x='4' y='1' width='1' height='6' fill='__LIGHT_ENC__'/%3E%3Crect x='5' y='1' width='1' height='7' fill='__SHADOW_ENC__'/%3E%3Crect x='6' y='0' width='1' height='1' fill='__GRIP_HI_ENC__'/%3E%3Crect x='6' y='1' width='1' height='6' fill='__LIGHT_ENC__'/%3E%3Crect x='7' y='1' width='1' height='7' fill='__SHADOW_ENC__'/%3E%3C/svg%3E") !important;
}
::-webkit-scrollbar-thumb:hover {
  background-color: __BODY__ !important;
}
::-webkit-scrollbar-thumb:vertical:active {
  background-color: __DARK__ !important;
  box-shadow: inset 1px 1px 0 __SHADOW__, inset -1px -1px 0 __LIGHT__, 0 1px 0 #777777, 0 2px 0 #888888 !important;
}
::-webkit-scrollbar-thumb:horizontal:active {
  background-color: __DARK__ !important;
  box-shadow: inset 1px 1px 0 __SHADOW__, inset -1px -1px 0 __LIGHT__, 1px 0 0 #777777, 2px 0 0 #888888 !important;
}
::-webkit-scrollbar-button {
  display: none !important;
  width: 0 !important;
  height: 0 !important;
}
::-webkit-scrollbar-button:single-button,
::-webkit-scrollbar-button:vertical:start:decrement,
::-webkit-scrollbar-button:vertical:end:increment,
::-webkit-scrollbar-button:horizontal:start:decrement,
::-webkit-scrollbar-button:horizontal:end:increment {
  display: block !important;
  width: 16px !important;
  height: 16px !important;
  background-color: #dddddd !important;
  border: 1px solid #000000 !important;
  border-radius: 0 !important;
  box-shadow: inset 1px 1px 0 #ffffff, inset -1px -1px 0 #bbbbbb !important;
  background-repeat: no-repeat !important;
  background-position: center !important;
}
::-webkit-scrollbar-button:single-button:hover,
::-webkit-scrollbar-button:vertical:start:decrement:hover,
::-webkit-scrollbar-button:vertical:end:increment:hover,
::-webkit-scrollbar-button:horizontal:start:decrement:hover,
::-webkit-scrollbar-button:horizontal:end:increment:hover {
  background-color: #e6e6e6 !important;
}
::-webkit-scrollbar-button:single-button:active,
::-webkit-scrollbar-button:vertical:start:decrement:active,
::-webkit-scrollbar-button:vertical:end:increment:active,
::-webkit-scrollbar-button:horizontal:start:decrement:active,
::-webkit-scrollbar-button:horizontal:end:increment:active {
  background-color: #999999 !important;
  box-shadow: inset 1px 1px 0 #666666, inset -1px -1px 0 #cccccc !important;
}
::-webkit-scrollbar-button:vertical:decrement {
  background-image: url("data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' width='14' height='14' shape-rendering='crispEdges'%3E%3Crect x='6' y='5' width='2' height='1' fill='%23000000'/%3E%3Crect x='5' y='6' width='4' height='1' fill='%23000000'/%3E%3Crect x='4' y='7' width='6' height='1' fill='%23000000'/%3E%3Crect x='3' y='8' width='8' height='1' fill='%23000000'/%3E%3C/svg%3E") !important;
}
::-webkit-scrollbar-button:vertical:increment {
  background-image: url("data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' width='14' height='14' shape-rendering='crispEdges'%3E%3Crect x='3' y='5' width='8' height='1' fill='%23000000'/%3E%3Crect x='4' y='6' width='6' height='1' fill='%23000000'/%3E%3Crect x='5' y='7' width='4' height='1' fill='%23000000'/%3E%3Crect x='6' y='8' width='2' height='1' fill='%23000000'/%3E%3C/svg%3E") !important;
}
::-webkit-scrollbar-button:horizontal:decrement {
  background-image: url("data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' width='14' height='14' shape-rendering='crispEdges'%3E%3Crect x='5' y='6' width='1' height='2' fill='%23000000'/%3E%3Crect x='6' y='5' width='1' height='4' fill='%23000000'/%3E%3Crect x='7' y='4' width='1' height='6' fill='%23000000'/%3E%3Crect x='8' y='3' width='1' height='8' fill='%23000000'/%3E%3C/svg%3E") !important;
}
::-webkit-scrollbar-button:horizontal:increment {
  background-image: url("data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' width='14' height='14' shape-rendering='crispEdges'%3E%3Crect x='5' y='3' width='1' height='8' fill='%23000000'/%3E%3Crect x='6' y='4' width='1' height='6' fill='%23000000'/%3E%3Crect x='7' y='5' width='1' height='4' fill='%23000000'/%3E%3Crect x='8' y='6' width='1' height='2' fill='%23000000'/%3E%3C/svg%3E") !important;
}
::-webkit-scrollbar-corner {
  background-color: #dddddd !important;
  border: 1px solid #000000 !important;
  box-shadow: inset 1px 1px 0 #ffffff, inset -1px -1px 0 #bbbbbb !important;
}
)CSS");

    css.replace(QStringLiteral("__BODY__"), QString::fromLatin1(ac.body));
    css.replace(QStringLiteral("__LIGHT__"), QString::fromLatin1(ac.light));
    css.replace(QStringLiteral("__DARK__"), QString::fromLatin1(ac.dark));
    css.replace(QStringLiteral("__SHADOW__"), QString::fromLatin1(ac.shadow));
    css.replace(QStringLiteral("__GRIP_HI_ENC__"), esc(ac.gripHi));
    css.replace(QStringLiteral("__LIGHT_ENC__"), esc(ac.light));
    css.replace(QStringLiteral("__SHADOW_ENC__"), esc(ac.shadow));
    return css;
}

}  // namespace

void installNetzacScripts(QWebEngineProfile *profile)
{
    if (!profile)
        return;

    QWebEngineScriptCollection *scripts = profile->scripts();
    if (!scripts)
        return;

    const QString scrollbarScriptName = QStringLiteral("netzac-platinum-scrollbars");
    for (const QWebEngineScript &old : scripts->find(scrollbarScriptName))
        scripts->remove(old);
    {
        QString css = platinumScrollbarCss();
        css.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
        css.replace(QLatin1Char('`'), QStringLiteral("\\`"));
        css.replace(QStringLiteral("${"), QStringLiteral("\\${"));

        // Inject the Platinum scrollbar style at the earliest possible moment. At
        // DocumentCreation the parser has usually not created <html>/<head> yet, so
        // waiting for DOMContentLoaded would show native scrollbars until the page
        // finishes parsing. A MutationObserver on the document plus a pre-paint
        // requestAnimationFrame loop apply the style as soon as <html> exists.
        const QString js = QStringLiteral(
            "(function() {\n"
            "  var id = 'zacos9-platinum-scrollbars';\n"
            "  var css = `%1`;\n"
            "  var guard = null;\n"
            "  var waiter = null;\n"
            "  function apply() {\n"
            "    if (document.getElementById(id)) return true;\n"
            "    var parent = document.head || document.documentElement;\n"
            "    if (!parent) return false;\n"
            "    var st = document.createElement('style');\n"
            "    st.id = id;\n"
            "    st.textContent = css;\n"
            "    parent.appendChild(st);\n"
            "    return true;\n"
            "  }\n"
            "  function watch() {\n"
            "    if (guard || typeof MutationObserver === 'undefined') return;\n"
            "    var head = document.head;\n"
            "    if (!head) return;\n"
            "    guard = new MutationObserver(function() {\n"
            "      if (!document.getElementById(id)) apply();\n"
            "    });\n"
            "    guard.observe(head, { childList: true });\n"
            "  }\n"
            "  function attempt() {\n"
            "    if (!apply()) return false;\n"
            "    if (waiter) { waiter.disconnect(); waiter = null; }\n"
            "    watch();\n"
            "    return true;\n"
            "  }\n"
            "  if (!attempt() && typeof MutationObserver !== 'undefined') {\n"
            "    waiter = new MutationObserver(function() { attempt(); });\n"
            "    waiter.observe(document, { childList: true, subtree: true });\n"
            "  }\n"
            "  document.addEventListener('readystatechange', attempt);\n"
            "  document.addEventListener('DOMContentLoaded', attempt);\n"
            "  window.addEventListener('load', attempt);\n"
            "  var frames = 0;\n"
            "  (function raf() {\n"
            "    if (document.getElementById(id)) return;\n"
            "    attempt();\n"
            "    if (++frames < 300) requestAnimationFrame(raf);\n"
            "  })();\n"
            "})();\n").arg(css);

        QWebEngineScript script;
        script.setName(scrollbarScriptName);
        script.setSourceCode(js);
        script.setInjectionPoint(QWebEngineScript::DocumentCreation);
        script.setRunsOnSubFrames(true);
        script.setWorldId(QWebEngineScript::ApplicationWorld);
        scripts->insert(script);
    }

    const QString popupScriptName = QStringLiteral("netzac-window-open-hook");
    if (scripts->find(popupScriptName).isEmpty()) {
        QString prefix = popupTokenPrefix();
        prefix.replace(QLatin1Char('\''), QStringLiteral("\\'"));
        const QString js = QStringLiteral(
            "(function() {\n"
            "  if (window.__netzacOpenHooked) return;\n"
            "  window.__netzacOpenHooked = true;\n"
            "  var origOpen = window.open;\n"
            "  var prefix = '%1';\n"
            "  window.open = function(url, target, features) {\n"
            "    if (typeof features === 'string' && features.length > 0) {\n"
            "      console.debug(prefix + features);\n"
            "    }\n"
            "    return origOpen.apply(this, arguments);\n"
            "  };\n"
            "})();\n").arg(prefix);

        QWebEngineScript script;
        script.setName(popupScriptName);
        script.setSourceCode(js);
        script.setInjectionPoint(QWebEngineScript::DocumentCreation);
        script.setRunsOnSubFrames(true);
        script.setWorldId(QWebEngineScript::MainWorld);
        scripts->insert(script);
    }
}

NetzacWebPage::NetzacWebPage(QWebEngineProfile *profile, QObject *parent)
    : QWebEnginePage(profile, parent)
{
}

void NetzacWebPage::setBuiltInPage(bool isBuiltIn)
{
    m_isBuiltInPage = isBuiltIn;
}

QString NetzacWebPage::takePendingPopupFeatures()
{
    QString out = m_pendingPopupFeatures;
    m_pendingPopupFeatures.clear();
    return out;
}

void NetzacWebPage::javaScriptConsoleMessage(
    JavaScriptConsoleMessageLevel level,
    const QString &message,
    int lineNumber,
    const QString &sourceID)
{
    const QString &prefix = popupTokenPrefix();
    if (message.startsWith(prefix)) {
        m_pendingPopupFeatures = message.mid(prefix.size()).trimmed();
        return;
    }
    static constexpr const char *kCredPrefix = "__NETZAC_CRED__:";
    if (message.startsWith(QLatin1String(kCredPrefix))) {
        const QByteArray jsonBytes = message.mid(int(strlen(kCredPrefix))).toUtf8();
        const QJsonDocument doc = QJsonDocument::fromJson(jsonBytes);
        if (doc.isObject()) {
            const QJsonObject obj = doc.object();
            const QString host = obj.value(QStringLiteral("host")).toString().trimmed();
            const QString user = obj.value(QStringLiteral("user")).toString().trimmed();
            const QString pass = obj.value(QStringLiteral("pass")).toString();
            if (!host.isEmpty() && !user.isEmpty() && !pass.isEmpty())
                emit credentialSubmitted(host, user, pass);
        }
        return;
    }
    QWebEnginePage::javaScriptConsoleMessage(level, message, lineNumber, sourceID);
}

bool NetzacWebPage::acceptNavigationRequest(
    const QUrl &url,
    NavigationType type,
    bool isMainFrame)
{
    if (isMainFrame) {
        const QString urlStr = url.toString();
        const QString scheme = url.scheme().toLower();
        if (m_isBuiltInPage) {
            const bool stillBuiltinDoc = (type != NavigationTypeLinkClicked)
                && (urlStr == QLatin1String("https://localhost/")
                    || scheme == QLatin1String("data")
                    || scheme == QLatin1String("about"));
            if (!stillBuiltinDoc)
                m_isBuiltInPage = false;
        }
        emit mainFrameNavigationRequested(url, type);
    }
    return QWebEnginePage::acceptNavigationRequest(url, type, isMainFrame);
}

NetzacWebView::NetzacWebView(QWidget *parent)
    : QWebEngineView(parent)
{
}

void NetzacWebView::setWindowCreator(WindowCreator creator)
{
    m_windowCreator = std::move(creator);
}

QWebEngineView *NetzacWebView::createWindow(QWebEnginePage::WebWindowType type)
{
    if (m_windowCreator)
        return m_windowCreator(type);
    return nullptr;
}

void NetzacWebView::contextMenuEvent(QContextMenuEvent *event)
{
    event->accept();
    emit contextMenuRequested(event->globalPos(), lastContextMenuRequest());
}
