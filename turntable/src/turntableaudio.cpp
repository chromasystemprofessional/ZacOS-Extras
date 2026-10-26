#include "turntableaudio.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QRegularExpression>
#include <QTimer>
#include <memory>

static const QString NoPactl = "pactl isn't installed (it's in the pulseaudio-utils package).";

/* pactl's "Connection failure: ..." means the sound server, not pactl. */
static QString serverError(const QString &error) {
	return error.startsWith("Connection failure") ? QString("The sound server isn't running.") : error;
}

TurntableAudio::TurntableAudio(QObject *parent) : QObject(parent), m_debounce(new QTimer(this)) {
	// Plugging a turntable in adds a card, its sources and their monitors: look once they're all there.
	m_debounce->setSingleShot(true);
	m_debounce->setInterval(150);
	connect(m_debounce, &QTimer::timeout, this, [this] { refresh(); });
}

void TurntableAudio::start() {
	subscribe();
}

void TurntableAudio::subscribe() {
	auto *p = new QProcess(this);
	connect(p, &QProcess::readyReadStandardOutput, this, [this, p] {
		// Every pactl run, ours included, is a client coming and going: only these matter.
		static const QRegularExpression event("^Event '(new|remove|change)' on (source|module|server) ");
		while (p->canReadLine()) {
			const QRegularExpressionMatch m = event.match(QString::fromUtf8(p->readLine()));
			if (m.hasMatch() && (m.captured(1) != "change" || m.captured(2) == "server")) {
				m_debounce->start();
			}
		}
	});
	auto ended = [this, p](const QString &why) {
		p->deleteLater();
		// The server stopped (its loopbacks went with it) or never answered: try again shortly.
		fail(why);
		QTimer::singleShot(2000, this, [this] { subscribe(); });
	};
	connect(p, &QProcess::finished, this, [p, ended] {
		const QString why = QString::fromUtf8(p->readAllStandardError()).trimmed();
		ended(serverError(why.isEmpty() ? QString("Connection failure") : why));
	});
	connect(p, &QProcess::errorOccurred, this, [ended](QProcess::ProcessError e) {
		if (e == QProcess::FailedToStart) {
			ended(NoPactl);
		}
	});
	p->start("pactl", { "subscribe" });
	refresh();
}

void TurntableAudio::refresh() {
	if (m_refreshing) {
		m_again = true;
		return;
	}
	m_refreshing = true;
	static const QList<QStringList> commands = { { "-f", "json", "info" }, { "-f", "json", "list", "sources" },
		{ "-f", "json", "list", "sinks" }, { "list", "short", "modules" } };
	auto replies = std::make_shared<QList<QByteArray>>(commands.size());
	auto pending = std::make_shared<int>(commands.size());
	auto failure = std::make_shared<QString>();
	for (int i = 0; i < commands.size(); ++i) {
		run(commands[i], [this, i, replies, pending, failure](const QByteArray &out, const QString &error) {
			(*replies)[i] = out;
			if (failure->isEmpty()) {
				*failure = error;
			}
			if (--*pending) {
				return;
			}
			m_refreshing = false;
			if (m_again) {
				// Something changed while we looked (a loopback of ours, say): this may be out of date.
				m_again = false;
				refresh();
				return;
			}
			failure->isEmpty() ? read(*replies) : fail(serverError(*failure));
		});
	}
}

void TurntableAudio::read(const QList<QByteArray> &replies) {
	const QString defaultSink = QJsonDocument::fromJson(replies[0]).object().value("default_sink_name").toString();

	inputs.clear();
	// A name with a space or a quote couldn't be given to load-module.
	static const QRegularExpression unusable("[\\s\"']");
	for (const QJsonValue &value : QJsonDocument::fromJson(replies[1]).array()) {
		const QJsonObject source = value.toObject();
		const QJsonObject props = source.value("properties").toObject();
		const QString name = source.value("name").toString();
		if (name.isEmpty() || name.contains(unusable) || props.value("device.bus").toString() != "usb" ||
				name.endsWith(".monitor") || props.value("device.class").toString() == "monitor") {
			continue;
		}
		TurntableInput input;
		input.name = name;
		input.label = source.value("description").toString();
		if (input.label.isEmpty()) {
			input.label = name;
		}
		input.codec = props.value("device.vendor.id").toString() == "0x08bb" &&
			props.value("device.product.id").toString().startsWith("0x29");
		inputs << input;
	}

	output.clear();
	for (const QJsonValue &value : QJsonDocument::fromJson(replies[2]).array()) {
		const QJsonObject sink = value.toObject();
		if (sink.value("name").toString() != defaultSink) {
			continue;
		}
		output = sink.value("description").toString(defaultSink);
		// As the Sound control panel does: an HDMI output by the monitor's name, "SONY TV - ...".
		const QJsonObject props = sink.value("properties").toObject();
		const QString monitor = props.value("alsa.name").toString();
		if (props.value("device.profile.name").toString().startsWith("hdmi") && !monitor.isEmpty() &&
				!monitor.startsWith("HDMI")) {
			output = monitor + " - " + output;
		}
	}

	// "536870916<TAB>module-loopback<TAB>source=NAME sink=... latency_msec=100<TAB>"
	m_loopbacks.clear();
	static const QRegularExpression sourceArg("(?:^|\\s)source=\"?([^\\s\"]+)");
	for (const QString &line : QString::fromUtf8(replies[3]).split('\n')) {
		const QStringList fields = line.split('\t');
		if (fields.size() < 3 || fields[1] != "module-loopback") {
			continue;
		}
		const QRegularExpressionMatch m = sourceArg.match(fields[2]);
		if (m.hasMatch()) {
			m_loopbacks.insert(m.captured(1), fields[0].toUInt());
		}
	}

	state = Ready;
	error.clear();
	if (changed) {
		changed();
	}
}

void TurntableAudio::fail(const QString &why) {
	state = NoServer;
	error = why;
	inputs.clear();
	output.clear();
	m_loopbacks.clear();
	if (changed) {
		changed();
	}
}

void TurntableAudio::play(const QString &source, int latency, Done done) {
	run({ "load-module", "module-loopback", "source=" + source, QString("latency_msec=%1").arg(latency),
			// Unplugged, the loopback ends. Otherwise it would move to the next input: the microphone.
			"source_dont_move=true",
			// No sink: it plays through the current sound output, and follows it when that changes.
			"sink_input_properties=media.name=Turntable", "source_output_properties=media.name=Turntable" },
		[this, source, done](const QByteArray &out, const QString &error) {
			// Known at once, not at the next look: an Off or a quit straight after must find it.
			bool ok = false;
			const uint module = out.trimmed().toUInt(&ok);
			// A look that finished first may have found it already.
			if (error.isEmpty() && ok && !m_loopbacks.contains(source, module)) {
				m_loopbacks.insert(source, module);
			}
			refresh();
			done(error);
		});
}

void TurntableAudio::stop(const QString &source, Done done) {
	const QList<uint> modules = m_loopbacks.values(source);
	if (modules.isEmpty()) {
		done(QString());
		return;
	}
	auto pending = std::make_shared<int>(modules.size());
	auto failure = std::make_shared<QString>();
	for (uint module : modules) {
		run({ "unload-module", QString::number(module) },
			[this, source, module, done, pending, failure](const QByteArray &, const QString &error) {
				// Already gone is stopped too: PipeWire ends it when the turntable is unplugged.
				if (error.isEmpty() || error == "Failure: No such entity") {
					m_loopbacks.remove(source, module);
				} else if (failure->isEmpty()) {
					*failure = error;
				}
				if (--*pending == 0) {
					refresh();
					done(*failure);
				}
			});
	}
}

void TurntableAudio::stopDetached(const QString &source) {
	for (uint module : m_loopbacks.values(source)) {
		QProcess::startDetached("pactl", { "unload-module", QString::number(module) });
	}
}

void TurntableAudio::run(const QStringList &args, Output done) {
	auto *p = new QProcess(this);
	connect(p, &QProcess::finished, this, [p, done](int code, QProcess::ExitStatus status) {
		p->deleteLater();
		const QString error = QString::fromUtf8(p->readAllStandardError()).trimmed();
		if (status != QProcess::NormalExit) {
			done({}, "The sound server didn't answer.");
		} else if (code != 0) {
			done({}, error.isEmpty() ? QString("pactl failed with status %1.").arg(code) : error);
		} else {
			done(p->readAllStandardOutput(), {});
		}
	});
	connect(p, &QProcess::errorOccurred, this, [p, done](QProcess::ProcessError e) {
		if (e == QProcess::FailedToStart) {
			p->deleteLater();
			done({}, NoPactl);
		}
	});
	p->start("pactl", args);
	QTimer::singleShot(5000, p, [p] { p->kill(); });
}
