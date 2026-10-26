#pragma once

/*
 * The Turntable module: On plays a USB turntable through the current sound
 * output (a PipeWire loopback), Off stops it. Left on, it plays again when
 * the turntable appears: after logging in, a sound server restart, or
 * plugging it back in. turntable.conf, [turntable]: `input` (the source
 * played), `listening`, and `latency` in ms (100).
 */

#include "collarextension.h"
#include "turntableaudio.h"

class TurntableModule : public QObject {
public:
	using Done = std::function<void(const QString &)>;
	enum Look { Playing, Stopped, Unavailable };

	TurntableModule(TurntableAudio *audio, CollarExtensionService *service, QObject *parent = nullptr);

	static QString settingsPath();
	/* A record seen from above, its arm on it while playing; all grey when there's no turntable. */
	static QImage icon(Look look);
	CollarModule module() const;
	void activate(const QString &item, Done done);
	/* Quitting (the extra removed, or logging out): the record stops with it. */
	void quit();

private:
	TurntableAudio *m_audio;
	CollarExtensionService *m_service;
	QString m_input;
	bool m_listening = false;
	int m_latency = 100;
	QString m_present; /* the turntable found last time, to notice it appearing */
	QString m_failure; /* why it couldn't play again */
	/* The input chosen; before one is, the only USB input or the only turntable codec. */
	const TurntableInput *turntable() const;
	void update();
	void play(const QString &input, Done done);
	void save();
};
