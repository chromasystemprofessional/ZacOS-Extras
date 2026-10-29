#pragma once

#include "collarextension.h"

#include <QList>
#include <QObject>
#include <QWidget>

class QTimer;

class NightOverlay : public QWidget {
public:
	explicit NightOverlay(QWidget *parent = nullptr);
protected:
	void paintEvent(QPaintEvent *) override;
};

class NightModule : public QObject {
public:
	explicit NightModule(CollarExtensionService *service, QObject *parent = nullptr);
	~NightModule() override;

	QList<CollarModule> modules() const;
	void activate(const QString &module, const QString &item, std::function<void(const QString &)> done);

private:
	CollarExtensionService *m_service;
	bool m_on = false;
	bool m_auto = false;
	double m_lat = 51.5;   /* London, a sensible default */
	double m_lon = -0.13;
	QList<QWidget *> m_overlays;
	QTimer *m_timer = nullptr;

	void load();
	void save();
	void apply();
	void createOverlays();
	void destroyOverlays();
	void updateAuto();
	void showLocationDialog();
	static double sunHourAngle(double lat, double decl);
	static void sunTimes(double lat, double lon, int &sunriseMin, int &sunsetMin);
	static QImage icon(bool on);
};
