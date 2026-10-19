#pragma once

/*
 * Serves Collar modules from an extra: owns org.zacos9.CollarExtension.<name>
 * and answers org.zacos9.CollarExtension1 at /org/zacos9/CollarExtension.
 * The protocol is in ~/ZacOS/docs/collar.md, "Extensions".
 */

#include <QImage>
#include <QList>
#include <QObject>
#include <functional>

struct CollarMenuItem {
	enum Flags : uint { Enabled = 1, Checked = 2, Separator = 4 };
	QString id, label;
	uint flags = Enabled;

	static CollarMenuItem separator() { return { QString(), QString(), Separator }; }
	static CollarMenuItem text(const QString &label) { return { QString(), label, 0 }; }
	static CollarMenuItem action(const QString &id, const QString &label, bool enabled = true, bool checked = false) {
		return { id, label, (enabled ? uint(Enabled) : 0u) | (checked ? uint(Checked) : 0u) };
	}
};

struct CollarModule {
	QString id, label, description;
	bool active = false;
	QImage icon; /* 16 x 16 */
	QList<CollarMenuItem> menu;
};

class CollarExtensionService : public QObject {
public:
	static constexpr const char *Interface = "org.zacos9.CollarExtension1";
	static constexpr const char *Path = "/org/zacos9/CollarExtension";
	/* $ZACOS9_COLLAR_EXTENSION_PREFIX (for tests), else org.zacos9.CollarExtension. */
	static QString prefix();

	explicit CollarExtensionService(const QString &name, QObject *parent = nullptr);
	~CollarExtensionService() override;
	/* Takes the bus name; false (with a reason) if another copy has it. */
	bool start(QString *error);

	/* The modules as they are now. */
	std::function<QList<CollarModule>()> modules;
	/* A menu choice. Call `done` with an empty string on success, or an alert's text. */
	std::function<void(const QString &module, const QString &item, std::function<void(const QString &)> done)> activate;

	/* Tells The Collar to call GetModules again. */
	void changed();

private:
	class Object;
	QString m_service;
	Object *m_object;
};
