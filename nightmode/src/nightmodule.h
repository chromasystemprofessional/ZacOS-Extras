#pragma once

#include "collarextension.h"

#include <QObject>

class NightModule : public QObject {
public:
	explicit NightModule(CollarExtensionService *service, QObject *parent = nullptr);

	QList<CollarModule> modules() const;
	void activate(const QString &module, const QString &item, std::function<void(const QString &)> done);

	bool isOn() const { return m_on; }

private:
	CollarExtensionService *m_service;
	bool m_on = false;

	void load();
	void save();
	void apply();
	static QImage icon(bool on);
};
