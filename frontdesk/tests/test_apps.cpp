/* The Front Desk panel's program list (apps.cpp): the user-installed
 * applications in the Finder's Applications folder. ZacOS's own programs,
 * control panels, utilities and baseline applications are not there. */
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTextStream>

#include "apps.h"

static int fails;

static void check(bool ok, const QString &what) {
	QTextStream(stdout) << (ok ? "ok    " : "FAIL  ") << what << "\n";
	fails += !ok;
}

static void entry(const QString &dir, const QString &name, const QString &body) {
	QFile file(dir + "/applications/" + name);
	file.open(QIODevice::WriteOnly);
	file.write("[Desktop Entry]\nType=Application\n");
	file.write(body.toUtf8());
	file.close();
}

int main(int argc, char **argv) {
	QCoreApplication app(argc, argv);
	QTemporaryDir tmp;
	if (!tmp.isValid()) {
		return 1;
	}
	const QString dataHome = tmp.filePath("data-home");
	const QString dataSystem = tmp.filePath("data-system");
	const QString bin = tmp.filePath("bin");
	QDir().mkpath(dataHome + "/applications");
	QDir().mkpath(dataHome + "/zacos9");
	QDir().mkpath(dataSystem + "/applications");
	QDir().mkpath(bin);

	/* A user-installed application, found on PATH as the installed ones
	 * are. */
	QFile helper(bin + "/zacos9-test-program");
	helper.open(QIODevice::WriteOnly);
	helper.write("#!/bin/sh\nexit 0\n");
	helper.close();
	helper.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);

	entry(dataHome, "test-program.desktop",
		"Name=Test Program\nExec=zacos9-test-program\nCategories=Graphics;\n");
	entry(dataHome, "test-utility.desktop",
		"Name=Test Utility\nExec=/bin/false\nCategories=Utility;\n");
	entry(dataHome, "test-panel.desktop",
		"Name=Test Panel\nExec=/bin/false\nCategories=Settings;X-ZacOS9-ControlPanel;\n");
	entry(dataHome, "zacos9-test-app.desktop",
		"Name=Test ZacOS App\nExec=zacos9-test-program\nCategories=Graphics;\n");
	entry(dataSystem, "baseline.desktop",
		"Name=Baseline App\nExec=/bin/false\nCategories=Graphics;\n");
	QFile baseList(dataHome + "/zacos9/base-applications");
	baseList.open(QIODevice::WriteOnly);
	baseList.write("baseline.desktop\n");
	baseList.close();

	qputenv("PATH", (bin + ':' + qgetenv("PATH")).toUtf8());
	qputenv("XDG_DATA_HOME", dataHome.toUtf8());
	qputenv("XDG_DATA_DIRS", dataSystem.toUtf8());

	QStringList names;
	for (const DeskApp &a : AppCatalog::installed()) {
		names << a.name;
	}
	check(names == QStringList{ "Test Program" },
		"only the Applications-folder application is on the panel");
	return fails ? 1 : 0;
}
