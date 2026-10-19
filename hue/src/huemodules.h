#pragma once

/*
 * The Hue bridge as Collar modules: a "Hue" module (setup, or Turn Off All
 * Lights and the choice of rooms to show), then one per chosen room or zone,
 * by name. None show until chosen; hue.conf's [strip] rooms lists their IDs.
 */

#include "collarextension.h"
#include "huebridge.h"

class QTimer;

class HueModules : public QObject {
public:
	explicit HueModules(HueBridge *bridge, CollarExtensionService *service, QObject *parent = nullptr);

	QList<CollarModule> modules() const;
	void activate(const QString &module, const QString &item, std::function<void(const QString &)> done);

	/* A bulb lit with `color`, or unlit (grey) if it's invalid. */
	static QImage bulb(QColor color);
	/* What a room's lights look like together: their average colour, or invalid if all are off. */
	QColor glow(const HueGroup &group) const;
	/* Whether a room or zone has its own module. */
	bool shown(const HueGroup &group) const;

private:
	HueBridge *m_bridge;
	CollarExtensionService *m_service;
	QTimer *m_debounce;
	QStringList m_chosen;
	/* Rooms and zones together, by name. */
	QList<HueGroup> byName() const;
	void toggleShown(const QString &group);
	CollarModule setupModule() const;
	CollarModule hueModule() const;
	CollarModule groupModule(const HueGroup &group) const;
};
