#include "turntablemodule.h"

#include <QSettings>
#include <QStandardPaths>

/* 16 x 16, seen from above: # outline, v vinyl, g a groove's shine, L the label. */
static const char *const RecordArt[16] = {
	"................",
	"................",
	"................",
	"....####........",
	"..##vvvv##......",
	".#vvggggvv#.....",
	".#vgvvvvgv#.....",
	"#vgvvLLvvgv#....",
	"#vgvLLLLvgv#....",
	"#vgvLLLLvgv#....",
	"#vgvvLLvvgv#....",
	".#vgvvvvgv#.....",
	".#vvggggvv#.....",
	"..##vvvv##......",
	"....####........",
	"................",
};

/* The tone arm over the record: = its pivot, * the cartridge on the vinyl. */
static const char *const ArmResting[16] = {
	"............###.",
	"............#=#.",
	"............###.",
	".............#..",
	".............#..",
	".............#..",
	".............#..",
	".............#..",
	".............#..",
	".............#..",
	".............#..",
	"............##..",
	"............##..",
	"................",
	"................",
	"................",
};

static const char *const ArmPlaying[16] = {
	"............###.",
	"............#=#.",
	"............###.",
	".............#..",
	".............#..",
	"............#...",
	"............#...",
	"...........#....",
	"...........#....",
	"..........*.....",
	".........**.....",
	"................",
	"................",
	"................",
	"................",
	"................",
};

TurntableModule::TurntableModule(TurntableAudio *audio, CollarExtensionService *service, QObject *parent)
	: QObject(parent), m_audio(audio), m_service(service) {
	m_audio->changed = [this] { update(); };
	m_service->modules = [this] { return QList<CollarModule>{ module() }; };
	m_service->activate = [this](const QString &, const QString &item, Done done) { activate(item, std::move(done)); };
	QSettings s(settingsPath(), QSettings::IniFormat);
	m_input = s.value("turntable/input").toString();
	m_listening = s.value("turntable/listening").toBool();
	const int latency = s.value("turntable/latency").toInt();
	if (latency > 0) {
		m_latency = latency;
	}
}

QString TurntableModule::settingsPath() {
	return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + "/zacos9/turntable.conf";
}

void TurntableModule::save() {
	QSettings s(settingsPath(), QSettings::IniFormat);
	s.setValue("turntable/input", m_input);
	s.setValue("turntable/listening", m_listening);
	s.sync();
}

const TurntableInput *TurntableModule::turntable() const {
	const QList<TurntableInput> &inputs = m_audio->inputs;
	if (!m_input.isEmpty()) {
		// Never another input in its place: a USB microphone played through the speakers howls.
		for (const TurntableInput &input : inputs) {
			if (input.name == m_input) {
				return &input;
			}
		}
		return nullptr;
	}
	if (inputs.size() == 1) {
		return &inputs.first();
	}
	const TurntableInput *codec = nullptr;
	for (const TurntableInput &input : inputs) {
		if (input.codec) {
			if (codec) {
				return nullptr;
			}
			codec = &input;
		}
	}
	return codec;
}

void TurntableModule::update() {
	const TurntableInput *t = turntable();
	const QString present = t ? t->name : QString();
	// Logging in, a sound server restart and plugging it in all make it appear.
	if (t && present != m_present && m_listening && !m_audio->playing(t->name)) {
		m_audio->play(t->name, m_latency, [this](const QString &error) {
			m_failure = error.isEmpty() ? QString() : "Couldn't play it: " + error;
			m_service->changed();
		});
	}
	m_present = present;
	m_service->changed();
}

QImage TurntableModule::icon(Look look) {
	const bool grey = look == Unavailable;
	const QRgb ink = grey ? qRgb(0x80, 0x80, 0x80) : qRgb(0, 0, 0);
	const QRgb vinyl = grey ? qRgb(0xB0, 0xB0, 0xB0) : qRgb(0x30, 0x30, 0x30);
	const QRgb groove = grey ? qRgb(0xC8, 0xC8, 0xC8) : qRgb(0x68, 0x68, 0x68);
	const QRgb label = look == Playing ? qRgb(0xCC, 0x33, 0x33) : grey ? qRgb(0xDD, 0xDD, 0xDD) : qRgb(0xEE, 0xEE, 0xEE);
	const QRgb metal = grey ? qRgb(0xC0, 0xC0, 0xC0) : qRgb(0xA0, 0xA0, 0xA0);
	const QRgb cartridge = qRgb(0xE0, 0xE0, 0xE0);
	const char *const *arm = look == Playing ? ArmPlaying : ArmResting;
	QImage image(16, 16, QImage::Format_ARGB32);
	image.fill(Qt::transparent);
	for (int y = 0; y < 16; ++y) {
		for (int x = 0; x < 16; ++x) {
			switch (arm[y][x] != '.' ? arm[y][x] : RecordArt[y][x]) {
			case '#': image.setPixel(x, y, ink); break;
			case 'v': image.setPixel(x, y, vinyl); break;
			case 'g': image.setPixel(x, y, groove); break;
			case 'L': image.setPixel(x, y, label); break;
			case '=': image.setPixel(x, y, metal); break;
			case '*': image.setPixel(x, y, cartridge); break;
			}
		}
	}
	return image;
}

CollarModule TurntableModule::module() const {
	CollarModule m;
	m.id = "turntable";
	m.label = "Turntable";
	auto &menu = m.menu;
	if (m_audio->state != TurntableAudio::Ready) {
		m.icon = icon(Unavailable);
		m.description = m_audio->state == TurntableAudio::Starting ? QString("Looking for the turntable...")
			: m_audio->error;
		menu << CollarMenuItem::text(m.description);
		return m;
	}

	const TurntableInput *t = turntable();
	const bool playing = t && m_audio->playing(t->name);
	const QList<TurntableInput> &inputs = m_audio->inputs;
	m.active = playing;
	m.icon = icon(!t ? Unavailable : playing ? Playing : Stopped);
	m.description = playing ? QString("Turntable: on")
		: t ? QString("Turntable: off")
		: m_input.isEmpty() && !inputs.isEmpty() ? QString("Turntable: choose its input")
		: QString("Turntable: not plugged in");

	menu << CollarMenuItem::action("on", "On", t != nullptr, playing)
		 << CollarMenuItem::action("off", "Off", t != nullptr, t && !playing);
	if (!m_failure.isEmpty()) {
		menu << CollarMenuItem::text(m_failure);
	}
	menu << CollarMenuItem::separator();
	if (inputs.isEmpty()) {
		menu << CollarMenuItem::text("Plug the turntable in with its USB cable.");
	} else if (inputs.size() == 1 && t) {
		menu << CollarMenuItem::text("Input: " + t->label);
	} else {
		menu << CollarMenuItem::text("Input");
		for (const TurntableInput &input : inputs) {
			menu << CollarMenuItem::action("input:" + input.name, "  " + input.label, true, t && input.name == t->name);
		}
	}
	if (t && !m_audio->output.isEmpty()) {
		menu << CollarMenuItem::text("Output: " + m_audio->output);
	}
	return m;
}

void TurntableModule::play(const QString &input, Done done) {
	m_input = input;
	m_present = input;
	m_failure.clear();
	// Remembered only once it plays: a failed On is still off.
	auto on = [this, done] {
		m_listening = true;
		save();
		done(QString());
	};
	if (m_audio->playing(input)) {
		on();
		return;
	}
	m_audio->play(input, m_latency, [on, done](const QString &error) {
		error.isEmpty() ? on() : done("The turntable couldn't be played. " + error);
	});
}

void TurntableModule::activate(const QString &item, Done done) {
	const TurntableInput *t = turntable();
	if (item == "on") {
		if (!t) {
			done("The turntable isn't plugged in.");
			return;
		}
		play(t->name, done);
	} else if (item == "off") {
		m_listening = false;
		m_failure.clear();
		save();
		if (!t) {
			done(QString());
			return;
		}
		m_audio->stop(t->name, done);
	} else if (item.startsWith("input:")) {
		const QString input = item.mid(6);
		const QString old = t ? t->name : QString();
		if (input == old) {
			done(QString());
			return;
		}
		// Taken as present already, so the update after the stop below doesn't play it a second time.
		m_input = input;
		m_present = input;
		if (!t || !m_audio->playing(old)) {
			save();
			m_service->changed();
			done(QString());
			return;
		}
		// Playing: the new input takes over.
		m_audio->stop(old, [this, input, done](const QString &error) {
			if (!error.isEmpty()) {
				done(error);
				return;
			}
			play(input, done);
		});
	} else {
		done(QString());
	}
}

void TurntableModule::quit() {
	if (const TurntableInput *t = turntable()) {
		m_audio->stopDetached(t->name);
	}
}
