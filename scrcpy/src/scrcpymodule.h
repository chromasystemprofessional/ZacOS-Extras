#pragma once

#include "collarextension.h"

#include <QProcess>
#include <QObject>
#include <functional>

class ScrcpyModule : public QObject {
public:
	using Done = std::function<void(const QString &)>;
	ScrcpyModule(CollarExtensionService *service, QObject *parent = nullptr);
	CollarModule module() const;
	void activate(const QString &item, Done done);
private:
	CollarExtensionService *m_service;
	QProcess m_process;
};