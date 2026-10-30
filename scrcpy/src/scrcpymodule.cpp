#include "scrcpymodule.h"

#include <QImage>

static QImage moduleIcon() {
	QImage image(16, 16, QImage::Format_ARGB32);
	image.fill(Qt::transparent);
	for (int y = 0; y < 16; ++y)
		for (int x = 0; x < 16; ++x)
			if (x > 2 && x < 13 && y > 2 && y < 13)
				image.setPixel(x, y, qRgb(0x40, 0x40, 0x40));
	return image;
}

ScrcpyModule::ScrcpyModule(CollarExtensionService *service, QObject *parent)
	: QObject(parent), m_service(service) {
	m_service->modules = [this] { return QList<CollarModule>{ module() }; };
	m_service->activate = [this](const QString &, const QString &item, Done done) { activate(item, std::move(done)); };
	connect(&m_process, &QProcess::finished, this, [this] { m_service->changed(); });
}

CollarModule ScrcpyModule::module() const {
	CollarModule m;
	m.id = "scrcpy";
	m.label = "Android Mirror";
	m.active = m_process.state() != QProcess::NotRunning;
	m.icon = moduleIcon();
	m.description = m.active ? "Mirroring Android" : "Mirror an Android device (Scrcpy)";
	m.menu << CollarMenuItem::action("start", "Start Mirroring", !m.active)
		<< CollarMenuItem::action("stop", "Stop Mirroring", m.active);
	return m;
}

void ScrcpyModule::activate(const QString &item, Done done) {
	if (item == "start") {
		if (m_process.state() != QProcess::NotRunning) { done(QString()); return; }
		m_process.start("scrcpy");
		m_service->changed();
		done(QString());
	} else if (item == "stop") {
		if (m_process.state() != QProcess::NotRunning) {
			m_process.terminate();
		}
		done(QString());
	} else {
		done(QString());
	}
}