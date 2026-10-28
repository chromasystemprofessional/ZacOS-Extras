#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>
#include <QProcess>

int main(int argc, char *argv[]) {
	QGuiApplication app(argc, argv);

	/* Start Syncthing's daemon if it isn't already running. */
	QProcess::startDetached(QStringLiteral("syncthing"),
		QStringList() << QStringLiteral("serve") << QStringLiteral("--no-browser"));

	/* Open Syncthing's web interface once, the first time this runs. */
	const QString stateDir = QStandardPaths::writableLocation(QStandardPaths::StateLocation);
	QDir().mkpath(stateDir);
	const QString marker = stateDir + QStringLiteral("/phone-opened");
	if (!QFile::exists(marker)) {
		QTimer::singleShot(2500, [] {
			QDesktopServices::openUrl(QUrl(QStringLiteral("http://127.0.0.1:8384")));
		});
		QTimer::singleShot(3000, [marker] {
			QFile f(marker);
			if (f.open(QIODevice::WriteOnly)) {
				f.write("opened\n");
			}
			QCoreApplication::quit();
		});
	} else {
		QTimer::singleShot(100, [] { QCoreApplication::quit(); });
	}

	return app.exec();
}
