#include "collarextension.h"
#include "nightmodule.h"

#include <QCoreApplication>
#include <cstdio>

int main(int argc, char **argv) {
	QCoreApplication app(argc, argv);
	app.setApplicationName("zacos9-collar-nightmode");

	CollarExtensionService service("NightMode");
	NightModule module(&service);
	QString error;
	if (!service.start(&error)) {
		fprintf(stderr, "zacos9-collar-nightmode: %s\n", qPrintable(error));
		return 1;
	}
	return app.exec();
}
