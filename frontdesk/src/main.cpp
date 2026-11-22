#include "apps.h"
#include "frontdesk.h"
#include "settings.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QGuiApplication>
#include <QHash>
#include <QIcon>
#include <QScreen>
#include <QStyleFactory>
#include <QTimer>

#include <cstdio>

namespace {

/* One Front Desk per screen: a panel on each display, each with its own
 * tab, page and selection. One panel with one screen, as before. */
class Panels : public QObject {
public:
	explicit Panels(bool each) : m_each(each) {}

	void start() {
		const QList<QScreen *> screens = QGuiApplication::screens();
		if (!m_each || screens.size() < 2) {
			add(screens.value(0), m_each);
			return;
		}
		for (QScreen *screen : screens) {
			add(screen, true);
		}
		connect(qApp, &QGuiApplication::screenAdded, this, [this](QScreen *screen) {
			if (m_each) {
				add(screen, true);
			}
		});
		connect(qApp, &QGuiApplication::screenRemoved, this, [this](QScreen *screen) {
			if (FrontDeskWindow *window = m_windows.take(screen)) {
				window->deleteLater();
			}
		});
	}

private:
	void add(QScreen *screen, bool each) {
		auto *window = new FrontDeskWindow(false);
		if (screen) {
			window->setScreen(screen);
		}
		window->showMaximized();
		m_windows.insert(screen, window);
		if (each) {
			/* The compositor fills each panel to its screen's usable area
			 * and keeps the panel there. */
			return;
		}
		/* The first maximize request can arrive before the xdg_toplevel is
		 * mapped, and ZacOS treats it as its zoom box. Ask once the window
		 * exists, from an unzoomed state, so the request is a real change. */
		QTimer::singleShot(200, window, [window] {
			window->showNormal();
			QTimer::singleShot(100, window, [window] { window->showMaximized(); });
		});
	}

	bool m_each;
	QHash<QScreen *, FrontDeskWindow *> m_windows;
};

} // namespace

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
		std::printf("Front Desk: user=%s programs=%zu frontDesk=%d menus=%s zacMenu=%d "
			"screens=%s\n", settings.user.toUtf8().constData(), apps.size(),
			settings.frontDesk ? 1 : 0, settings.menus.toUtf8().constData(),
			settings.zacMenu ? 1 : 0, settings.eachScreen ? "each" : "one");
		return 0;
	}

	if (parser.isSet(window)) {
		FrontDeskWindow frontDesk(true);
		frontDesk.show();
		return app.exec();
	}
	const FrontDeskSettings settings = FrontDeskSettings::load();
	Panels panels(settings.eachScreen);
	panels.start();
	return app.exec();
}
