#include "dialogs.h"
#include "i18n.h"
#include "netzacwindow.h"
#include "platinum.h"
#include "preferences.h"

#include <QApplication>
#include <QCoreApplication>
#include <QFileInfo>
#include <QUrl>
#include <QWebEngineDownloadRequest>
#include <QWebEngineProfile>

int main(int argc, char *argv[])
{
    initI18n();

    // Prevent WebEngine context-sharing and Wayland/pixman maximize/resize freezes.
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
    if (qEnvironmentVariable("WLR_RENDERER") == QLatin1String("pixman")) {
        if (!qEnvironmentVariableIsSet("QT_QUICK_BACKEND"))
            qputenv("QT_QUICK_BACKEND", "software");
        if (!qEnvironmentVariableIsSet("QTWEBENGINE_CHROMIUM_FLAGS"))
            qputenv("QTWEBENGINE_CHROMIUM_FLAGS", "--disable-gpu --disable-gpu-compositing");
    }

    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("Netzac Navigator"));
    app.setDesktopFileName(QStringLiteral("netzac"));
    applyZacos9Theme(app);

    NetzacPreferences::ensureDesktopEntryRegistered();
    NetzacPreferences::load().apply(netzacProfile());

    QObject::connect(
        netzacProfile(),
        &QWebEngineProfile::downloadRequested,
        &app,
        [](QWebEngineDownloadRequest *download) {
            if (!download)
                return;
            const QString target = platinumSaveFileName(
                QApplication::activeWindow(),
                ztr("Save File"),
                download->suggestedFileName());
            if (target.isEmpty()) {
                download->cancel();
                return;
            }
            const QFileInfo fi(target);
            download->setDownloadDirectory(fi.absolutePath());
            download->setDownloadFileName(fi.fileName());
            download->accept();
        });

    QString initialUrl;
    for (int i = 1; i < argc; ++i) {
        const QString arg = QString::fromLocal8Bit(argv[i]).trimmed();
        if (arg.isEmpty() || arg.startsWith(QLatin1Char('-')))
            continue;
        const QFileInfo fi(arg);
        if (fi.exists() && fi.isFile())
            initialUrl = QUrl::fromLocalFile(fi.absoluteFilePath()).toString();
        else
            initialUrl = arg;
        break;
    }

    NetzacWindow window(initialUrl);
    window.show();
    return app.exec();
}
