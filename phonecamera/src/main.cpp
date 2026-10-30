#include <QApplication>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

#include <QFile>

static bool haveProgram(const QString &name) {
	QProcess p;
	p.start("sh", {"-c", "command -v " + name});
	p.waitForFinished(2000);
	return p.exitCode() == 0;
}

class PhoneCameraWindow : public QWidget {
public:
	PhoneCameraWindow() {
		setWindowTitle("Phone Camera");
		resize(480, 360);
		auto *layout = new QVBoxLayout(this);
		status = new QLabel("Ready.", this);
		layout->addWidget(status);

		cameraCombo = new QComboBox(this);
		cameraCombo->addItem("Back", "back");
		cameraCombo->addItem("Front", "front");
		layout->addWidget(new QLabel("Camera:", this));
		layout->addWidget(cameraCombo);

		deviceEdit = new QLineEdit("/dev/video0", this);
		layout->addWidget(new QLabel("V4L2 device:", this));
		layout->addWidget(deviceEdit);

		auto *listButton = new QPushButton("List cameras", this);
		layout->addWidget(listButton);
		connect(listButton, &QPushButton::clicked, this, &PhoneCameraWindow::listCameras);

		startButton = new QPushButton("Start", this);
		stopButton = new QPushButton("Stop", this);
		stopButton->setEnabled(false);
		layout->addWidget(startButton);
		layout->addWidget(stopButton);
		connect(startButton, &QPushButton::clicked, this, &PhoneCameraWindow::start);
		connect(stopButton, &QPushButton::clicked, this, &PhoneCameraWindow::stop);

		log = new QPlainTextEdit(this);
		log->setReadOnly(true);
		layout->addWidget(log);

		if (!haveProgram("scrcpy")) {
			status->setText("scrcpy not found. Install it before use.");
		}
		if (!haveProgram("adb")) {
			status->setText(status->text() + " adb not found.");
		}
	}

	~PhoneCameraWindow() override {
		stop();
	}

private:
	void listCameras() {
		if (!haveProgram("scrcpy")) {
			log->appendPlainText("scrcpy not found.");
			return;
		}
		QProcess p;
		p.start("scrcpy", {"--list-cameras"});
		p.waitForFinished(10000);
		QString out = QString::fromUtf8(p.readAllStandardOutput()) + QString::fromUtf8(p.readAllStandardError());
		log->appendPlainText(out);
		// Parse lines like "  0: back (..." or camera ids; keep simple: add raw tokens
		for (const QString &line : out.split('\n')) {
			if (line.contains("Camera")) {
				cameraCombo->addItem(line.trimmed(), line.trimmed());
			}
		}
	}

	void start() {
		if (proc) return;
		if (!haveProgram("scrcpy")) {
			log->appendPlainText("scrcpy not found.");
			return;
		}
		QString facing = cameraCombo->currentData().toString();
		QStringList args;
		args << "--video-source=camera";
		if (facing == "front" || facing == "back") {
			args << ("--camera-facing=" + facing);
		} else if (!facing.isEmpty()) {
			args << ("--camera-id=" + facing);
		}
		args << ("--v4l2-sink=" + deviceEdit->text());
		proc = new QProcess(this);
		connect(proc, &QProcess::readyReadStandardOutput, this, [this]() {
			log->appendPlainText(QString::fromUtf8(proc->readAllStandardOutput()));
		});
		connect(proc, &QProcess::readyReadStandardError, this, [this]() {
			log->appendPlainText(QString::fromUtf8(proc->readAllStandardError()));
		});
		connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, [this]() {
			status->setText("Stopped.");
			startButton->setEnabled(true);
			stopButton->setEnabled(false);
			proc->deleteLater();
			proc = nullptr;
		});
		proc->start("scrcpy", args);
		status->setText("Starting...");
		startButton->setEnabled(false);
		stopButton->setEnabled(true);
	}

	void stop() {
		if (proc) {
			proc->terminate();
			proc->waitForFinished(3000);
		}
	}

	QLabel *status = nullptr;
	QComboBox *cameraCombo = nullptr;
	QLineEdit *deviceEdit = nullptr;
	QPushButton *startButton = nullptr;
	QPushButton *stopButton = nullptr;
	QPlainTextEdit *log = nullptr;
	QProcess *proc = nullptr;
};

int main(int argc, char **argv) {
	QApplication app(argc, argv);
	PhoneCameraWindow w;
	w.show();
	return app.exec();
}
