#include "sound.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

#include <cassert>
#include <cstdio>
#include <unistd.h>

namespace {

bool waitForLog(const QString &path) {
	for (int i = 0; i < 400; i++) {
		if (QFileInfo(path).size() > 0) {
			return true;
		}
		usleep(10000);
	}
	return false;
}

QString takeLog(const QString &path) {
	QFile file(path);
	if (!file.open(QIODevice::ReadOnly)) {
		return {};
	}
	const QString text = QString::fromUtf8(file.readAll());
	file.close();
	file.remove();
	return text;
}

void writeFile(const QString &path, const QByteArray &data) {
	QDir().mkpath(QFileInfo(path).absolutePath());
	QFile file(path);
	assert(file.open(QIODevice::WriteOnly));
	assert(file.write(data) == data.size());
}

} // namespace

int main(int argc, char **argv) {
	QCoreApplication app(argc, argv);
	QTemporaryDir root;
	assert(root.isValid());
	const QString directory = root.path();

	writeFile(directory + "/paplay",
		"#!/bin/sh\nprintf '%s\\n' \"$*\" >> \"$SOUND_LOG\"\n");
	QFile player(directory + "/paplay");
	assert(player.setPermissions(player.permissions() | QFileDevice::ExeOwner));
	for (const char *name : { "woodblock.wav", "chirp.wav", "platinum.wav" }) {
		writeFile(directory + "/sounds/" + QString::fromLatin1(name), "RIFF");
	}

	qputenv("ZACOS9_DATA", directory.toLocal8Bit());
	qputenv("XDG_CONFIG_HOME", (directory + "/config").toLocal8Bit());
	qputenv("XDG_DATA_HOME", (directory + "/data").toLocal8Bit());
	qputenv("PATH", directory.toLocal8Bit());
	const QString log = directory + "/played";
	qputenv("SOUND_LOG", log.toLocal8Bit());

	FrontDeskSound sound;
	assert(sound.available());
	assert(sound.directory() == directory + "/sounds");

	sound.click();
	assert(waitForLog(log));
	QString played = takeLog(log);
	assert(played.contains("woodblock.wav"));
	assert(played.contains("--volume=46811"));

	sound.open();
	assert(waitForLog(log));
	played = takeLog(log);
	assert(played.contains("chirp.wav"));

	sound.alert();
	assert(waitForLog(log));
	played = takeLog(log);
	assert(played.contains("platinum.wav"));

	writeFile(directory + "/config/zacos9/desktop.conf",
		"[General]\ninterface-volume=0\n");
	sound.click();
	usleep(200000);
	assert(takeLog(log).isEmpty());

	std::printf("ok: Front Desk plays the click, open and alert sounds\n");
	return 0;
}
