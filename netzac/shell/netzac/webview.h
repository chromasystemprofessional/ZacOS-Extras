#pragma once

#include <functional>

#include <QPoint>
#include <QUrl>
#include <QWebEnginePage>
#include <QWebEngineView>

class QContextMenuEvent;
class QWebEngineContextMenuRequest;
class QWebEngineProfile;

class NetzacWebPage : public QWebEnginePage {
    Q_OBJECT

public:
    explicit NetzacWebPage(QWebEngineProfile *profile, QObject *parent = nullptr);

    void setBuiltInPage(bool isBuiltIn);
    bool isBuiltInPage() const { return m_isBuiltInPage; }
    QString takePendingPopupFeatures();

signals:
    void mainFrameNavigationRequested(const QUrl &url, QWebEnginePage::NavigationType type);
    void credentialSubmitted(const QString &host, const QString &username, const QString &password);

protected:
    bool acceptNavigationRequest(const QUrl &url, NavigationType type, bool isMainFrame) override;
    void javaScriptConsoleMessage(
        JavaScriptConsoleMessageLevel level,
        const QString &message,
        int lineNumber,
        const QString &sourceID) override;

private:
    bool m_isBuiltInPage = false;
    QString m_pendingPopupFeatures;
};

void installNetzacScripts(QWebEngineProfile *profile);

class NetzacWebView : public QWebEngineView {
    Q_OBJECT

public:
    using WindowCreator = std::function<QWebEngineView *(QWebEnginePage::WebWindowType)>;

    explicit NetzacWebView(QWidget *parent = nullptr);

    void setWindowCreator(WindowCreator creator);

signals:
    void contextMenuRequested(const QPoint &globalPos, QWebEngineContextMenuRequest *request);

protected:
    QWebEngineView *createWindow(QWebEnginePage::WebWindowType type) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

private:
    WindowCreator m_windowCreator;
};
