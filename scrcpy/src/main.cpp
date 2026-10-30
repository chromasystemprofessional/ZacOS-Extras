#include <QApplication>
#include <cstdio>
#include <cstdlib>

#include "collarextension.h"
#include "scrcpymodule.h"

int main(int argc, char **argv) {
	QApplication app(argc, argv);
	app.setApplicationName("zacos9-scrcpy");

	CollarExtensionService service("Scrcpy");
	fprintf(stderr, "bus=%s\n", getenv("DBUS_SESSION_BUS_ADDRESS"));
	fflush(stderr);
	fprintf(stderr, "service name: %s\n", qPrintable(service.prefix() + ".Scrcpy"));
	fflush(stderr);
	QString error;
	if (!service.start(&error)) {
		fprintf(stderr, "zacos9-scrcpy: %s\n", qPrintable(error));
		return 1;
	}
	fprintf(stderr, "started ok\n");
	ScrcpyModule module(&service);
	return app.exec();
}