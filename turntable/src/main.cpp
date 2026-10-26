/* zacos9-collar-turntable: a USB turntable in ZacOS's Collar, played through the current sound output. */

#include <QCoreApplication>
#include <QSocketNotifier>
#include <csignal>
#include <cstdio>
#include <unistd.h>

#include "collarextension.h"
#include "turntableaudio.h"
#include "turntablemodule.h"

/* SIGTERM (the package removed, logging out) and SIGINT quit by way of a
 * pipe: a signal handler may only write(). */
static int signalPipe[2];

static void quitOnSignal(int) {
	const char c = 0;
	(void)!write(signalPipe[1], &c, 1);
}

int main(int argc, char **argv) {
	QCoreApplication app(argc, argv);
	app.setApplicationName("zacos9-collar-turntable");

	CollarExtensionService service("Turntable");
	TurntableAudio audio;
	TurntableModule module(&audio, &service);
	QString error;
	if (!service.start(&error)) {
		fprintf(stderr, "zacos9-collar-turntable: %s\n", qPrintable(error));
		return 1;
	}
	if (pipe(signalPipe) == 0) {
		auto *notifier = new QSocketNotifier(signalPipe[0], QSocketNotifier::Read, &app);
		QObject::connect(notifier, &QSocketNotifier::activated, [] { QCoreApplication::quit(); });
		std::signal(SIGTERM, quitOnSignal);
		std::signal(SIGINT, quitOnSignal);
	}
	audio.start();
	const int code = app.exec();
	// Removing the extra mustn't leave the record playing with nothing to stop it.
	module.quit();
	return code;
}
