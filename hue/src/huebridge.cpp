#include "huebridge.h"

#include "huecolor.h"

#include <QFile>
#include <QHostInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QProcess>
#include <QRegularExpression>
#include <QSettings>
#include <QSslCertificate>
#include <QSslConfiguration>
#include <QStandardPaths>
#include <QTimer>
#include <algorithm>
#include <memory>

QString HueBridge::settingsPath() {
	return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + "/zacos9/hue.conf";
}

/* "Couldn't reach ..." texts for the menus and alerts. */
static QString networkError(QNetworkReply *reply) {
	switch (reply->error()) {
	case QNetworkReply::ConnectionRefusedError:
	case QNetworkReply::HostNotFoundError:
	case QNetworkReply::TimeoutError:
	case QNetworkReply::OperationCanceledError:
		return "The Hue Bridge isn't responding.";
	case QNetworkReply::SslHandshakeFailedError:
		return "The Hue Bridge's certificate doesn't match the bridge.";
	default:
		return "The Hue Bridge couldn't be reached: " + reply->errorString();
	}
}

/* CLIP v2 replies list problems under "errors": [{ "description": ... }]. */
static QString clipError(const QJsonObject &body) {
	const QJsonArray errors = body.value("errors").toArray();
	return errors.isEmpty() ? QString() : errors.first().toObject().value("description").toString();
}

static void merge(QJsonObject &into, const QJsonObject &from) {
	for (auto it = from.begin(); it != from.end(); ++it) {
		if (it->isObject() && into.value(it.key()).isObject()) {
			QJsonObject inner = into.value(it.key()).toObject();
			merge(inner, it->toObject());
			into.insert(it.key(), inner);
		} else {
			into.insert(it.key(), *it);
		}
	}
}

HueBridge::HueBridge(QObject *parent)
	: QObject(parent), m_reconnect(new QTimer(this)), m_poll(new QTimer(this)) {
	m_reconnect->setSingleShot(true);
	m_reconnect->setInterval(5000);
	connect(m_reconnect, &QTimer::timeout, this, [this] { load(); });
	// The event stream carries every change; this only catches a stream that silently stalled.
	m_poll->setInterval(60000);
	connect(m_poll, &QTimer::timeout, this, [this] {
		if (state == Ready) {
			load();
		}
	});
	// Bridges sign their own certificates, naming themselves (their bridge ID) as the subject.
	connect(&m_net, &QNetworkAccessManager::sslErrors, this, [this](QNetworkReply *reply, const QList<QSslError> &) {
		const QStringList names = reply->sslConfiguration().peerCertificate().subjectInfo(QSslCertificate::CommonName);
		if (!bridgeId.isEmpty() && names.contains(bridgeId, Qt::CaseInsensitive)) {
			reply->ignoreSslErrors();
		}
	});
}

void HueBridge::setState(State s, const QString &e) {
	state = s;
	error = e;
	if (s != Ready) {
		m_poll->stop();
	} else if (!m_poll->isActive()) {
		m_poll->start();
	}
	if (changed) {
		changed();
	}
}

QString HueBridge::base() const {
	return m_testBridge ? address : "https://" + address;
}

QNetworkRequest HueBridge::request(const QString &path) const {
	QNetworkRequest r(QUrl(base() + path));
	r.setRawHeader("hue-application-key", m_key.toUtf8());
	r.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
	r.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);
	r.setTransferTimeout(10000);
	return r;
}

void HueBridge::saveSettings() {
	QSettings s(settingsPath(), QSettings::IniFormat);
	s.setValue("bridge/key", m_key);
	if (m_testBridge) {
		s.sync();
		QFile::setPermissions(settingsPath(), QFileDevice::ReadOwner | QFileDevice::WriteOwner);
		return; // never remember a test bridge's address
	}
	s.setValue("bridge/address", address);
	s.setValue("bridge/id", bridgeId);
	s.sync();
	// The key controls every light in the house.
	QFile::setPermissions(settingsPath(), QFileDevice::ReadOwner | QFileDevice::WriteOwner);
}

void HueBridge::start() {
	QSettings s(settingsPath(), QSettings::IniFormat);
	m_key = s.value("bridge/key").toString();
	const QString test = qEnvironmentVariable("ZACOS9_HUE_BRIDGE");
	if (!test.isEmpty()) {
		m_testBridge = true;
		address = test;
		bridgeId = "test";
	} else {
		address = s.value("bridge/address").toString();
		bridgeId = s.value("bridge/id").toString();
	}
	if (address.isEmpty()) {
		discoverMdns();
	} else if (m_key.isEmpty()) {
		setState(NeedsPairing);
	} else {
		load();
	}
}

void HueBridge::search() {
	if (m_events) {
		m_events->abort();
	}
	m_resources.clear();
	rebuild();
	if (m_testBridge) {
		start();
		return;
	}
	address.clear();
	bridgeId.clear();
	m_key.clear();
	discoverMdns();
}

void HueBridge::retry() {
	if (state == NeedsPairing || state == Searching) {
		return;
	}
	setState(Connecting);
	load();
}

/* ---- Finding the bridge ------------------------------------------------------------- */

void HueBridge::discoverMdns() {
	setState(Searching);
	// One service type per avahi-browse; -k keeps the type unsubstituted in -p output.
	auto *avahi = new QProcess(this);
	connect(avahi, &QProcess::finished, this, [this, avahi] {
		avahi->deleteLater();
		QString ip4, ip6, id;
		for (const QString &line : QString::fromUtf8(avahi->readAllStandardOutput()).split('\n')) {
			const QStringList f = line.split(';');
			if (f.size() < 10 || f[0] != "=") {
				continue;
			}
			// The TXT records share the last field: "modelid=BSB002" "bridgeid=001788fffe123456".
			const QRegularExpressionMatch match = QRegularExpression("bridgeid=([0-9A-Fa-f]+)").match(f.mid(9).join(';'));
			if (match.hasMatch()) {
				id = match.captured(1);
			}
			(f[2] == "IPv4" ? ip4 : ip6) = f[7];
		}
		if (!ip4.isEmpty() || !ip6.isEmpty()) {
			found(!ip4.isEmpty() ? ip4 : "[" + ip6 + "]", id);
		} else {
			discoverCloud();
		}
	});
	connect(avahi, &QProcess::errorOccurred, this, [this, avahi](QProcess::ProcessError e) {
		if (e == QProcess::FailedToStart) {
			avahi->deleteLater();
			discoverCloud();
		}
	});
	avahi->start("avahi-browse", { "-rtpk", "_hue._tcp" });
	QTimer::singleShot(8000, avahi, [avahi] { avahi->kill(); });
}

void HueBridge::discoverCloud() {
	QNetworkRequest r(QUrl("https://discovery.meethue.com/"));
	r.setTransferTimeout(10000);
	QNetworkReply *reply = m_net.get(r);
	connect(reply, &QNetworkReply::finished, this, [this, reply] {
		reply->deleteLater();
		const QJsonArray bridges = QJsonDocument::fromJson(reply->readAll()).array();
		if (reply->error() != QNetworkReply::NoError || bridges.isEmpty()) {
			setState(NoBridge, "No Hue Bridge was found on this network.");
			return;
		}
		const QJsonObject b = bridges.first().toObject();
		found(b.value("internalipaddress").toString(), b.value("id").toString());
	});
}

void HueBridge::found(const QString &addr, const QString &id) {
	address = addr;
	bridgeId = id.toLower();
	saveSettings();
	m_key.isEmpty() ? setState(NeedsPairing) : load();
}

void HueBridge::pair(Done done) {
	const QJsonObject body{ { "devicetype", "zacos9_collar#" + QHostInfo::localHostName().left(19) },
		{ "generateclientkey", true } };
	QNetworkRequest r(QUrl(base() + "/api"));
	r.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
	r.setTransferTimeout(10000);
	QNetworkReply *reply = m_net.post(r, QJsonDocument(body).toJson(QJsonDocument::Compact));
	connect(reply, &QNetworkReply::finished, this, [this, reply, done] {
		reply->deleteLater();
		// The bridge answers pairing problems with HTTP 200 and an error body; no status means no answer.
		if (!reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).isValid()) {
			done(networkError(reply));
			return;
		}
		const QJsonObject result = QJsonDocument::fromJson(reply->readAll()).array().first().toObject();
		const QString key = result.value("success").toObject().value("username").toString();
		if (key.isEmpty()) {
			const QJsonObject e = result.value("error").toObject();
			done(e.value("type").toInt() == 101
				? "Press the round link button on top of the Hue Bridge, then choose Connect again within 30 seconds."
				: "The Hue Bridge refused: " + e.value("description").toString());
			return;
		}
		m_key = key;
		saveSettings();
		done(QString());
		setState(Connecting);
		load();
	});
}

/* ---- Resources and events ----------------------------------------------------------- */

void HueBridge::load() {
	QNetworkReply *reply = m_net.get(request("/clip/v2/resource"));
	connect(reply, &QNetworkReply::finished, this, [this, reply] {
		reply->deleteLater();
		const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
		if (status == 401 || status == 403) {
			m_key.clear();
			saveSettings();
			setState(NeedsPairing);
			return;
		}
		if (reply->error() != QNetworkReply::NoError) {
			setState(Unreachable, networkError(reply));
			m_reconnect->start();
			return;
		}
		m_resources.clear();
		for (const QJsonValue &v : QJsonDocument::fromJson(reply->readAll()).object().value("data").toArray()) {
			const QJsonObject o = v.toObject();
			m_resources.insert(o.value("id").toString(), o);
		}
		rebuild();
		setState(Ready);
		if (!m_events) {
			listen();
		}
	});
}

void HueBridge::listen() {
	QNetworkRequest r = request("/eventstream/clip/v2");
	r.setRawHeader("Accept", "text/event-stream");
	r.setTransferTimeout(0);
	m_eventBuffer.clear();
	m_events = m_net.get(r);
	connect(m_events, &QNetworkReply::readyRead, this, [this] { readEvents(); });
	connect(m_events, &QNetworkReply::finished, this, [this, reply = m_events] {
		reply->deleteLater();
		if (m_events == reply) {
			m_events = nullptr;
			// The bridge ends streams now and then; pick up anything missed, then listen again.
			if (state == Ready) {
				m_reconnect->start();
			}
		}
	});
}

void HueBridge::readEvents() {
	m_eventBuffer += m_events->readAll();
	m_eventBuffer.replace("\r\n", "\n");
	int end;
	while ((end = m_eventBuffer.indexOf("\n\n")) >= 0) {
		const QByteArray event = m_eventBuffer.left(end);
		m_eventBuffer.remove(0, end + 2);
		QByteArray data;
		for (const QByteArray &line : event.split('\n')) {
			if (line.startsWith("data:")) {
				data += line.mid(5).trimmed();
			}
		}
		if (!data.isEmpty()) {
			applyEvents(data);
		}
	}
}

void HueBridge::applyEvents(const QByteArray &json) {
	for (const QJsonValue &v : QJsonDocument::fromJson(json).array()) {
		const QJsonObject container = v.toObject();
		const QString type = container.value("type").toString();
		for (const QJsonValue &d : container.value("data").toArray()) {
			const QJsonObject o = d.toObject();
			const QString id = o.value("id").toString();
			if (type == "delete") {
				m_resources.remove(id);
			} else if (type == "add" || !m_resources.contains(id)) {
				m_resources.insert(id, o);
			} else {
				merge(m_resources[id], o);
			}
		}
	}
	rebuild();
	if (changed) {
		changed();
	}
}

void HueBridge::rebuild() {
	lights.clear();
	groups.clear();
	scenes.clear();
	allLights.clear();
	for (const QJsonObject &o : m_resources) {
		const QString type = o.value("type").toString();
		if (type == "light") {
			HueLight l;
			l.id = o.value("id").toString();
			l.name = o.value("metadata").toObject().value("name").toString();
			l.on = o.value("on").toObject().value("on").toBool();
			l.brightness = o.value("dimming").toObject().value("brightness").toDouble(100);
			const QJsonObject xy = o.value("color").toObject().value("xy").toObject();
			l.color = o.contains("color");
			l.xy = QPointF(xy.value("x").toDouble(0.3227), xy.value("y").toDouble(0.3290));
			const QJsonObject ct = o.value("color_temperature").toObject();
			l.temperature = o.contains("color_temperature");
			l.mirek = ct.value("mirek_valid").toBool() ? ct.value("mirek").toInt() : 0;
			lights.insert(l.id, l);
		} else if (type == "scene") {
			const QString status = o.value("status").toObject().value("active").toString();
			scenes << HueScene{ o.value("id").toString(), o.value("metadata").toObject().value("name").toString(),
				o.value("group").toObject().value("rid").toString(), !status.isEmpty() && status != "inactive" };
		}
	}
	auto groupedLight = [this](const QJsonObject &o) {
		for (const QJsonValue &s : o.value("services").toArray()) {
			if (s.toObject().value("rtype").toString() == "grouped_light") {
				return s.toObject().value("rid").toString();
			}
		}
		return QString();
	};
	for (const QJsonObject &o : m_resources) {
		const QString type = o.value("type").toString();
		if (type == "bridge_home") {
			allLights = groupedLight(o);
			continue;
		}
		if (type != "room" && type != "zone") {
			continue;
		}
		HueGroup g;
		g.id = o.value("id").toString();
		g.kind = type == "room" ? HueGroup::Room : HueGroup::Zone;
		g.name = o.value("metadata").toObject().value("name").toString();
		g.groupedLight = groupedLight(o);
		// Rooms hold devices (each with its lights); zones hold lights.
		for (const QJsonValue &c : o.value("children").toArray()) {
			const QString rid = c.toObject().value("rid").toString();
			if (c.toObject().value("rtype").toString() == "light") {
				g.lights << rid;
				continue;
			}
			for (const QJsonValue &s : m_resources.value(rid).value("services").toArray()) {
				if (s.toObject().value("rtype").toString() == "light") {
					g.lights << s.toObject().value("rid").toString();
				}
			}
		}
		const QJsonObject gl = m_resources.value(g.groupedLight);
		g.on = gl.value("on").toObject().value("on").toBool();
		g.brightness = gl.value("dimming").toObject().value("brightness").toDouble();
		if (!g.groupedLight.isEmpty() && !g.lights.isEmpty()) {
			groups << g;
		}
	}
	std::sort(groups.begin(), groups.end(), [](const HueGroup &a, const HueGroup &b) {
		return a.kind != b.kind ? a.kind < b.kind : a.name.localeAwareCompare(b.name) < 0;
	});
	std::sort(scenes.begin(), scenes.end(), [](const HueScene &a, const HueScene &b) {
		return a.name.localeAwareCompare(b.name) < 0;
	});
}

const HueLight *HueBridge::light(const QString &id) const {
	const auto it = lights.constFind(id);
	return it == lights.cend() ? nullptr : &*it;
}

QList<HueScene> HueBridge::scenesFor(const QString &group) const {
	QList<HueScene> list;
	for (const HueScene &s : scenes) {
		if (s.group == group) {
			list << s;
		}
	}
	return list;
}

/* ---- Commands ----------------------------------------------------------------------- */

void HueBridge::put(const QString &type, const QString &id, const QJsonObject &body, Done done) {
	QNetworkReply *reply = m_net.put(request("/clip/v2/resource/" + type + "/" + id),
		QJsonDocument(body).toJson(QJsonDocument::Compact));
	connect(reply, &QNetworkReply::finished, this, [reply, done] {
		reply->deleteLater();
		const QString problem = clipError(QJsonDocument::fromJson(reply->readAll()).object());
		if (!problem.isEmpty()) {
			done("The Hue Bridge refused: " + problem);
		} else if (reply->error() != QNetworkReply::NoError) {
			done(networkError(reply));
		} else {
			done(QString());
		}
	});
}

void HueBridge::setOn(const QString &groupedLight, bool on, Done done) {
	put("grouped_light", groupedLight, { { "on", QJsonObject{ { "on", on } } } }, done);
}

void HueBridge::setBrightness(const QString &groupedLight, int percent, Done done) {
	put("grouped_light", groupedLight,
		{ { "on", QJsonObject{ { "on", true } } }, { "dimming", QJsonObject{ { "brightness", percent } } } }, done);
}

void HueBridge::recallScene(const QString &scene, Done done) {
	put("scene", scene, { { "recall", QJsonObject{ { "action", "active" } } } }, done);
}

void HueBridge::setPreset(const HueGroup &group, const QString &id, Done done) {
	const huecolor::Preset *p = huecolor::preset(id);
	if (!p) {
		done("Unknown color.");
		return;
	}
	QList<QPair<QString, QJsonObject>> commands;
	for (const QString &lightId : group.lights) {
		const HueLight *l = light(lightId);
		if (!l) {
			continue;
		}
		QJsonObject body{ { "on", QJsonObject{ { "on", true } } } };
		if (p->mirek > 0 && l->temperature) {
			body.insert("color_temperature", QJsonObject{ { "mirek", p->mirek } });
		} else if (l->color) {
			const QPointF xy = huecolor::xyFromRgb(p->mirek > 0 ? huecolor::rgbFromMirek(p->mirek) : QColor(p->rgb));
			body.insert("color", QJsonObject{ { "xy", QJsonObject{ { "x", xy.x() }, { "y", xy.y() } } } });
		} else {
			continue;
		}
		commands << qMakePair(lightId, body);
	}
	if (commands.isEmpty()) {
		done(QString("None of the lights in %1 can show that color.").arg(group.name));
		return;
	}
	// One light at a time, 100 ms apart: the bridge drops commands sent faster than about ten a second.
	struct Batch {
		int left;
		QString error;
		Done done;
	};
	auto batch = std::make_shared<Batch>(Batch{ int(commands.size()), QString(), done });
	for (int i = 0; i < commands.size(); ++i) {
		QTimer::singleShot(100 * i, this, [this, batch, command = commands[i]] {
			put("light", command.first, command.second, [batch](const QString &error) {
				if (batch->error.isEmpty()) {
					batch->error = error;
				}
				if (--batch->left == 0) {
					batch->done(batch->error);
				}
			});
		});
	}
}
