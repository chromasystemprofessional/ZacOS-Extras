#pragma once

/*
 * The sound server as the Turntable extra needs it, through pactl: the USB
 * inputs (a USB turntable is one), the loopbacks playing them, and the
 * current sound output. `pactl subscribe` says when any of these change.
 */

#include <QMultiMap>
#include <QObject>
#include <functional>

class QProcess;
class QTimer;

struct TurntableInput {
	QString name, label;
	/* A TI/Burr-Brown PCM29xx: the USB codec in most turntables. */
	bool codec = false;
};

class TurntableAudio : public QObject {
public:
	using Done = std::function<void(const QString &error)>;
	enum State { Starting, NoServer, Ready };

	explicit TurntableAudio(QObject *parent = nullptr);

	State state = Starting;
	QString error; /* why there's no sound server */
	QList<TurntableInput> inputs; /* the USB inputs */
	QString output; /* the current sound output, named as the Sound control panel names it */
	/* After every look at the server. */
	std::function<void()> changed;

	void start();
	bool playing(const QString &source) const { return m_loopbacks.contains(source); }
	/* Plays `source` through the current sound output, `latency` ms behind. */
	void play(const QString &source, int latency, Done done);
	/* Stops every loopback from `source`. */
	void stop(const QString &source, Done done);
	/* The same without waiting, for quitting. */
	void stopDetached(const QString &source);

private:
	using Output = std::function<void(const QByteArray &out, const QString &error)>;
	QTimer *m_debounce;
	bool m_refreshing = false, m_again = false;
	/* source -> module-loopback index; play() and stop() keep it current between looks. */
	QMultiMap<QString, uint> m_loopbacks;
	void subscribe();
	void refresh();
	void read(const QList<QByteArray> &replies);
	void fail(const QString &error);
	void run(const QStringList &args, Output done);
};
