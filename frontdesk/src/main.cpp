#include "apps.h"
#include "frontdesk.h"
#include "settings.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QIcon>
#include <QStyleFactory>
#include <QTimer>

#include <cstdio>

int main(int argc, char **argv) {
	QApplication app(argc, argv);
	QApplication::setApplicationName(QStringLiteral("Front Desk"));
	QApplication::setApplicationDisplayName(QStringLiteral("Front Desk"));
	QApplication::setDesktopFileName(QStringLiteral("zacos9-frontdesk"));
	QApplication::setWindowIcon(QIcon::fromTheme(QStringLiteral("applications-other")));
	if (QStyleFactory::keys().contains(QStringLiteral("zacos9"), Qt::CaseInsensitive)) {
		QApplication::setStyle(QStyleFactory::create(QStringLiteral("zacos9")));
	}

	QCommandLineParser parser;
	parser.setApplicationDescription(QStringLiteral("The Front Desk shared-computer launcher."));
	parser.addHelpOption();
	QCommandLineOption window(QStringLiteral("window"),
		QStringLiteral("Open as a 575×400 window instead of the session shell."));
	QCommandLineOption check(QStringLiteral("check"),
		QStringLiteral("Print what the panel would show, then exit."));
	parser.addOption(window);
	parser.addOption(check);
	parser.process(app);

	if (parser.isSet(check)) {
		const FrontDeskSettings settings = FrontDeskSettings::load();
		const auto apps = AppCatalog::forCurrentUser();
		std::printf("Front Desk: user=%s programs=%zu frontDesk=%d menus=%s zacMenu=%d\n",
			settings.user.toUtf8().constData(), apps.size(), settings.frontDesk ? 1 : 0,
			settings.menus.toUtf8().constData(), settings.zacMenu ? 1 : 0);
		return 0;
	}

	FrontDeskWindow frontDesk(parser.isSet(window));
	if (parser.isSet(window)) {
		frontDesk.show();
	} else {
		frontDesk.showMaximized();
		/* The first maximize request can arrive before the xdg_toplevel is
		 * mapped, and ZacOS treats it as its zoom box. Ask once the window
		 * exists, from an unzoomed state, so the request is a real change. */
		QTimer::singleShot(200, &frontDesk, [&frontDesk] {
			frontDesk.showNormal();
			QTimer::singleShot(100, &frontDesk, [&frontDesk] { frontDesk.showMaximized(); });
		});
	}
	return app.exec();
}
