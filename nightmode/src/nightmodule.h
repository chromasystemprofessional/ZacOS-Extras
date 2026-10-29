#pragma once

#include "collarextension.h"

#include <QList>
#include <QObject>
#include <QWidget>

class NightModule : public QObject {
public:
	explicit NightModule(CollarExtensionService *service, QObject *parent = nullptr);
	~NightModule() override;

	QList<CollarModule> modules() const;
	void activate(const QString &module, const QString &item, std::function<void(const QString &)> done);

	bool isOn() const { return m_on; }

private:
	CollarExtensionService *m_service;
	bool m_on = false;
	QList<QWidget *> m_overlays;

	void load();
	void save();
	void apply();
	void createOverlays();
	void destroyOverlays();
	static QImage icon(bool on);
};
