/* The Front Desk panel's program list (apps.cpp): ZacOS's own user
 * programs belong on it, but its control panels and system programs do
 * not, whatever their desktop entries say. */
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
	const QString data = tmp.filePath("data");
	const QString bin = tmp.filePath("bin");
	QDir().mkpath(data + "/applications");
	QDir().mkpath(bin);

	/* A ZacOS user program, found on PATH as the installed ones are. */
	QFile helper(bin + "/zacos9-test-program");
	helper.open(QIODevice::WriteOnly);
	helper.write("#!/bin/sh\nexit 0\n");
	helper.close();
	helper.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);

	entry(data, "zacos9-test-program.desktop",
		"Name=Test Program\nExec=zacos9-test-program\nCategories=Utility;\n");
	entry(data, "zacos9-test-panel.desktop",
		"Name=Test Panel\nExec=/bin/false\nCategories=Settings;X-ZacOS9-ControlPanel;\n");
	qputenv("PATH", (bin + ':' + qgetenv("PATH")).toUtf8());
	qputenv("XDG_DATA_HOME", data.toUtf8());
	qputenv("XDG_DATA_DIRS", data.toUtf8());

	bool user = false, panel = false;
	for (const DeskApp &a : AppCatalog::installed()) {
		user |= a.name == QStringLiteral("Test Program");
		panel |= a.name == QStringLiteral("Test Panel");
	}
	check(user, "a ZacOS user program is on the panel");
	check(!panel, "a control panel is not");
	return fails ? 1 : 0;
}
