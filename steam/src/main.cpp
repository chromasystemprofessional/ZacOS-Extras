#include <QApplication>
#include <cstdio>

#include "collarextension.h"
#include "steammodule.h"

int main(int argc, char **argv) {
	QApplication app(argc, argv);
	app.setApplicationName("zacos9-steam");

	CollarExtensionService service("Steam");
	QString error;
	if (!service.start(&error)) {
		fprintf(stderr, "zacos9-steam: %s\n", qPrintable(error));
		return 1;
	}
	SteamModule module(&service);
	return app.exec();
}