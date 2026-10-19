/* zacos9-collar-hue: Philips Hue rooms and zones as modules in ZacOS's Collar. */

#include <QCoreApplication>
#include <cstdio>

#include "collarextension.h"
#include "huebridge.h"
#include "huemodules.h"

int main(int argc, char **argv) {
	QCoreApplication app(argc, argv);
	app.setApplicationName("zacos9-collar-hue");

	CollarExtensionService service("Hue");
	HueBridge bridge;
	HueModules modules(&bridge, &service);
	QString error;
	if (!service.start(&error)) {
		fprintf(stderr, "zacos9-collar-hue: %s\n", qPrintable(error));
		return 1;
	}
	bridge.start();
	return app.exec();
}
