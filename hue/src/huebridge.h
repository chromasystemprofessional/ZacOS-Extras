#pragma once

/*
 * A Philips Hue Bridge on the local network, through its CLIP v2 API:
 * finding it (mDNS, then Philips's discovery service), pairing with its
 * link button, its rooms, zones, lights and scenes, live updates from its
 * event stream, and commands.
 *
 * Settings: ~/.config/zacos9/hue.conf ([bridge] address, id, key).
 * HueModules keeps its [strip] choices there too.
 * $ZACOS9_HUE_BRIDGE (a URL such as http://127.0.0.1:8080) replaces the
 * bridge and skips discovery and certificate checks; tests use it.
 */

#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QNetworkAccessManager>
#include <QObject>
#include <QPointF>
#include <QStringList>
#include <functional>

class QNetworkReply;
class QTimer;

struct HueLight {
	QString id, name;
	bool on = false;
	double brightness = 0; /* 0-100 */
	bool color = false, temperature = false;
	QPointF xy;
	int mirek = 0; /* 0 unless the light is showing a colour temperature */
};

struct HueGroup {
	enum Kind { Room, Zone };
	QString id, name, groupedLight;
	Kind kind = Room;
	bool on = false;
	double brightness = 0;
	QStringList lights;
};

struct HueScene {
	QString id, name, group;
	bool active = false;
};

class HueBridge : public QObject {
public:
	using Done = std::function<void(const QString &error)>;
	enum State { Searching, NoBridge, NeedsPairing, Connecting, Ready, Unreachable };

	explicit HueBridge(QObject *parent = nullptr);
	/* ~/.config/zacos9/hue.conf */
	static QString settingsPath();

	State state = Searching;
	QString address, bridgeId, error;
	QList<HueGroup> groups;          /* rooms by name, then zones by name */
	QHash<QString, HueLight> lights;
	QList<HueScene> scenes;          /* by name */
	QString allLights;               /* the grouped_light for every light, if any */
	std::function<void()> changed;

	void start();
	/* Forgets the bridge and looks again. */
	void search();
	void pair(Done done);
	void retry();

	void setOn(const QString &groupedLight, bool on, Done done);
	void setBrightness(const QString &groupedLight, int percent, Done done);
	void recallScene(const QString &scene, Done done);
	void setPreset(const HueGroup &group, const QString &preset, Done done);

	const HueLight *light(const QString &id) const;
	QList<HueScene> scenesFor(const QString &group) const;

private:
	QNetworkAccessManager m_net;
	QHash<QString, QJsonObject> m_resources;
	QNetworkReply *m_events = nullptr;
	QByteArray m_eventBuffer;
	QTimer *m_reconnect, *m_poll;
	QString m_key;
	bool m_testBridge = false;

	void setState(State state, const QString &error = QString());
	void discoverMdns();
	void discoverCloud();
	void found(const QString &address, const QString &id);
	void load();
	void listen();
	void readEvents();
	void applyEvents(const QByteArray &json);
	void rebuild();
	QString base() const;
	QNetworkRequest request(const QString &path) const;
	void put(const QString &type, const QString &id, const QJsonObject &body, Done done);
	void saveSettings();
};
