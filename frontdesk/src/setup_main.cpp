#include "apps.h"
#include "settings.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QStyleFactory>
#include <QVBoxLayout>

#include <pwd.h>
#include <unistd.h>

#include <algorithm>
#include <cstdio>
#include <functional>

namespace {

struct Account {
	QString user, name;
};

QString configDir() {
	return FrontDeskSettings::configDir();
}

bool frontDeskOn(const QString &user) {
	return QFile::exists(QDir(configDir()).filePath(QStringLiteral("frontdesk/") + user));
}

std::vector<Account> accounts() {
	uid_t lo = 1000, hi = 60000;
	QFile defs(QStringLiteral("/etc/login.defs"));
	if (defs.open(QIODevice::ReadOnly)) {
		for (const QByteArray &line : defs.readAll().split('\n')) {
			const QList<QByteArray> f = line.simplified().split(' ');
			if (f.size() == 2 && f[0] == "UID_MIN") {
				lo = f[1].toUInt();
			} else if (f.size() == 2 && f[0] == "UID_MAX") {
				hi = f[1].toUInt();
			}
		}
	}
	std::vector<Account> list;
	setpwent();
	while (struct passwd *pw = getpwent()) {
		const QString shell = QString::fromLocal8Bit(pw->pw_shell);
		if (pw->pw_uid < lo || pw->pw_uid > hi || shell.endsWith("/nologin") ||
				shell.endsWith("/false")) {
			continue;
		}
		const QString user = QString::fromLocal8Bit(pw->pw_name);
		const QString name = QString::fromLocal8Bit(pw->pw_gecos).section(',', 0, 0).trimmed();
		list.push_back({ user, name.isEmpty() ? user : name });
	}
	endpwent();
	std::sort(list.begin(), list.end(), [](const Account &a, const Account &b) {
		return a.name.localeAwareCompare(b.name) < 0;
	});
	return list;
}

QString helperPath() {
	const QByteArray override = qgetenv("ZACOS9_ACCOUNTS_HELPER");
	if (!override.isEmpty()) {
		return QString::fromUtf8(override);
	}
	const QFileInfo installed(QStringLiteral("/usr/libexec/zacos9/zacos9-accounts-helper"));
	if (installed.isExecutable()) {
		return installed.absoluteFilePath();
	}
	return QStandardPaths::findExecutable(QStringLiteral("zacos9-accounts-helper"));
}

class ProgramsDialog : public QDialog {
public:
	ProgramsDialog(const QString &user, QWidget *parent)
		: QDialog(parent), m_user(user) {
		setWindowTitle(tr("Programs"));
		setModal(true);
		auto *layout = new QVBoxLayout(this);
		auto *title = new QLabel(tr("Programs on the Front Desk panel for %1:").arg(user), this);
		layout->addWidget(title);

		m_only = new QCheckBox(tr("Only these programs"), this);
		layout->addWidget(m_only);

		m_list = new QListWidget(this);
		m_list->setIconSize(QSize(24, 24));
		layout->addWidget(m_list, 1);

		m_status = new QLabel(this);
		m_status->setWordWrap(true);
		layout->addWidget(m_status);

		auto *buttons = new QHBoxLayout();
		buttons->addStretch(1);
		m_cancel = new QPushButton(tr("Cancel"), this);
		m_ok = new QPushButton(tr("OK"), this);
		m_ok->setDefault(true);
		buttons->addWidget(m_cancel);
		buttons->addWidget(m_ok);
		layout->addLayout(buttons);

		resize(420, 430);
		connect(m_cancel, &QPushButton::clicked, this, &QDialog::reject);
		connect(m_ok, &QPushButton::clicked, this, [this] { accept(); });
		connect(m_only, &QCheckBox::toggled, this, [this](bool on) {
			m_list->setEnabled(on);
		});

		bool allowlist = false;
		QStringList entries;
		const bool filtered = AppCatalog::filterForUser(m_user, &allowlist, &entries);
		m_only->setChecked(filtered && allowlist);
		m_list->setEnabled(m_only->isChecked());
		for (const DeskApp &app : AppCatalog::installed()) {
			auto *item = new QListWidgetItem(AppCatalog::iconFor(app), app.name, m_list);
			item->setData(Qt::UserRole, app.entry);
			item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
			item->setCheckState(m_only->isChecked() && entries.contains(app.entry)
				? Qt::Checked : Qt::Unchecked);
		}
		if (!filtered) {
			m_status->setText(tr("There is no program limit for this account. Check “Only these programs” to make one."));
		} else if (!allowlist) {
			m_status->setText(tr("This account currently uses a block list. Choosing “Only these programs” replaces it with an allow list."));
		}
	}

private:
	void accept() override {
		QStringList entries;
		if (m_only->isChecked()) {
			for (int i = 0; i < m_list->count(); i++) {
				const QListWidgetItem *item = m_list->item(i);
				if (item->checkState() == Qt::Checked) {
					entries << item->data(Qt::UserRole).toString();
				}
			}
		}
		m_ok->setEnabled(false);
		m_only->setEnabled(false);
		m_list->setEnabled(false);
		m_status->setText(tr("Saving…"));
		AppCatalog::setFilterForUser(m_user, m_only->isChecked(), entries, this,
			[this](const QString &error) {
				if (!error.isEmpty()) {
					m_status->setText(error);
					m_ok->setEnabled(true);
					m_only->setEnabled(true);
					m_list->setEnabled(m_only->isChecked());
					return;
				}
				QDialog::accept();
			});
	}

	QString m_user;
	QCheckBox *m_only = nullptr;
	QListWidget *m_list = nullptr;
	QLabel *m_status = nullptr;
	QPushButton *m_ok = nullptr;
	QPushButton *m_cancel = nullptr;
};

class AccountDialog : public QDialog {
public:
	AccountDialog(const QString &user, QWidget *parent)
		: QDialog(parent), m_user(user) {
		setWindowTitle(tr("Front Desk Settings"));
		setModal(true);
		m_settings = FrontDeskSettings::load(user);
		const QString puppy = QFile::exists(QDir(configDir()).filePath(QStringLiteral("puppy/") + user))
			? QStringLiteral("puppy") : QStringLiteral("finder");
		const QString startup = m_settings.frontDesk ? QStringLiteral("frontdesk") : puppy;

		auto *layout = new QVBoxLayout(this);
		m_title = new QLabel(m_settings.displayName, this);
		QFont titleFont = m_title->font();
		titleFont.setBold(true);
		m_title->setFont(titleFont);
		layout->addWidget(m_title);

		m_pages = new QStackedWidget(this);
		layout->addWidget(m_pages, 1);

		/* 1. Account and startup mode. */
		auto *page1 = new QWidget(this);
		auto *form1 = new QFormLayout(page1);
		auto *name = new QLabel(m_settings.displayName + QStringLiteral(" (") + user + QStringLiteral(")"), page1);
		form1->addRow(tr("User Name:"), name);
		auto *password = new QLabel(tr("The account's password and picture are managed in Users."), page1);
		password->setWordWrap(true);
		form1->addRow(QString(), password);
		m_startup = new QComboBox(page1);
		m_startup->addItem(tr("Front Desk panels"), QStringLiteral("frontdesk"));
		m_startup->addItem(tr("Puppy Mode"), QStringLiteral("puppy"));
		m_startup->addItem(tr("Regular Finder"), QStringLiteral("finder"));
		m_startup->setCurrentIndex(qMax(0, m_startup->findData(startup)));
		form1->addRow(tr("After startup:"), m_startup);
		m_pages->addWidget(page1);

		/* 2. Programs. */
		auto *page2 = new QWidget(this);
		auto *layout2 = new QVBoxLayout(page2);
		layout2->addWidget(new QLabel(tr("Programs on the Front Desk panel:"), page2));
		m_programs = new QListWidget(page2);
		layout2->addWidget(m_programs, 1);
		auto *choose = new QPushButton(tr("Choose Programs…"), page2);
		layout2->addWidget(choose, 0, Qt::AlignLeft);
		m_pages->addWidget(page2);

		/* 3. Menus and speech. */
		auto *page3 = new QWidget(this);
		auto *form3 = new QFormLayout(page3);
		m_menus = new QComboBox(page3);
		m_menus->addItem(tr("Minimal"), QStringLiteral("minimal"));
		m_menus->addItem(tr("Restricted"), QStringLiteral("restricted"));
		m_menus->addItem(tr("All"), QStringLiteral("all"));
		m_menus->setCurrentIndex(qMax(0, m_menus->findData(m_settings.menus)));
		form3->addRow(tr("Menus:"), m_menus);
		m_speech = new QCheckBox(tr("Speak button names"), page3);
		m_speech->setChecked(m_settings.speech);
		form3->addRow(QString(), m_speech);
		m_pages->addWidget(page3);

		/* 4. Security. */
		auto *page4 = new QWidget(this);
		auto *form4 = new QFormLayout(page4);
		m_save = new QComboBox(page4);
		m_save->addItem(tr("Only in the user's folder"), QStringLiteral("user"));
		m_save->addItem(tr("Anywhere"), QStringLiteral("anywhere"));
		m_save->setCurrentIndex(qMax(0, m_save->findData(m_settings.save)));
		form4->addRow(tr("Allow this person to open and save:"), m_save);
		m_removable = new QCheckBox(tr("Allow removable disks"), page4);
		m_removable->setChecked(m_settings.saveRemovable);
		form4->addRow(QString(), m_removable);
		m_capture = new QCheckBox(tr("Allow screen captures"), page4);
		m_capture->setChecked(m_settings.capture);
		form4->addRow(QString(), m_capture);
		m_finder = new QCheckBox(tr("Allow unrestricted access to the Finder"), page4);
		m_finder->setChecked(m_settings.finder);
		form4->addRow(QString(), m_finder);
		m_pages->addWidget(page4);

		auto *nav = new QHBoxLayout();
		m_pageLabel = new QLabel(this);
		nav->addWidget(m_pageLabel);
		nav->addStretch(1);
		m_cancel = new QPushButton(tr("Cancel"), this);
		m_previous = new QPushButton(tr("Previous"), this);
		m_next = new QPushButton(tr("Next"), this);
		m_done = new QPushButton(tr("Done"), this);
		m_done->setDefault(true);
		nav->addWidget(m_cancel);
		nav->addWidget(m_previous);
		nav->addWidget(m_next);
		nav->addWidget(m_done);
		layout->addLayout(nav);

		resize(440, 340);
		connect(m_cancel, &QPushButton::clicked, this, &QDialog::reject);
		connect(m_previous, &QPushButton::clicked, this, [this] { showPage(m_page - 1); });
		connect(m_next, &QPushButton::clicked, this, [this] { showPage(m_page + 1); });
		connect(m_done, &QPushButton::clicked, this, [this] { save(); });
		connect(choose, &QPushButton::clicked, this, [this] {
			ProgramsDialog dialog(m_user, this);
			if (dialog.exec() == QDialog::Accepted) {
				loadPrograms();
			}
		});
		connect(m_save, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
			[this] { m_removable->setEnabled(m_save->currentData().toString() == QStringLiteral("user")); });
		m_removable->setEnabled(m_save->currentData().toString() == QStringLiteral("user"));

		showPage(0);
		loadPrograms();
	}

private:
	void showPage(int page) {
		m_page = qBound(0, page, m_pages->count() - 1);
		m_pages->setCurrentIndex(m_page);
		m_pageLabel->setText(tr("Page %1 of %2").arg(m_page + 1).arg(m_pages->count()));
		m_previous->setEnabled(m_page > 0);
		m_next->setEnabled(m_page + 1 < m_pages->count());
		m_done->setEnabled(m_page + 1 == m_pages->count());
	}

	void loadPrograms() {
		m_programs->clear();
		const bool filtered = [&] {
			bool allowlist = false;
			QStringList entries;
			return AppCatalog::filterForUser(m_user, &allowlist, &entries);
		}();
		const auto apps = AppCatalog::forUser(m_user);
		if (!filtered) {
			m_programs->addItem(tr("All installed programs"));
			return;
		}
		if (apps.empty()) {
			m_programs->addItem(tr("No programs"));
			return;
		}
		for (const DeskApp &app : apps) {
			new QListWidgetItem(AppCatalog::iconFor(app), app.name, m_programs);
		}
	}

	void save() {
		QStringList args;
		args << QStringLiteral("desktop") << m_user;
		const QString startup = m_startup->currentData().toString();
		if (startup == QStringLiteral("frontdesk")) {
			args << QStringLiteral("frontdesk");
			args << QStringLiteral("menus=") + m_menus->currentData().toString();
			args << QStringLiteral("save=") + m_save->currentData().toString();
			args << QStringLiteral("save-removable=") + (m_removable->isChecked() ? QStringLiteral("on") : QStringLiteral("off"));
			args << QStringLiteral("finder=") + (m_finder->isChecked() ? QStringLiteral("on") : QStringLiteral("off"));
			args << QStringLiteral("speech=") + (m_speech->isChecked() ? QStringLiteral("on") : QStringLiteral("off"));
			args << QStringLiteral("capture=") + (m_capture->isChecked() ? QStringLiteral("on") : QStringLiteral("off"));
		} else {
			args << startup;
		}
		setEnabled(false);
		runHelper(args, [this](bool ok, const QString &error) {
			setEnabled(true);
			if (!ok) {
				QMessageBox::warning(this, tr("Front Desk Setup"), error);
				return;
			}
			QDialog::accept();
		});
	}

	void runHelper(const QStringList &args, std::function<void(bool, const QString &)> done) {
		const QString helper = helperPath();
		if (helper.isEmpty()) {
			done(false, tr("The accounts helper isn't installed."));
			return;
		}
		auto *process = new QProcess(this);
		if (geteuid() == 0) {
			process->setProgram(helper);
			process->setArguments(args);
		} else {
			process->setProgram(QStringLiteral("pkexec"));
			process->setArguments(QStringList{ helper } + args);
		}
		connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
			[process, done](int code, QProcess::ExitStatus status) {
				const QString error = QString::fromUtf8(process->readAllStandardError()).trimmed();
				process->deleteLater();
				done(status == QProcess::NormalExit && code == 0,
					error.isEmpty() ? QStringLiteral("The change couldn't be saved.") : error);
			});
		connect(process, &QProcess::errorOccurred, this,
			[process, done](QProcess::ProcessError error) {
				process->deleteLater();
				done(false, error == QProcess::FailedToStart
					? QStringLiteral("The accounts helper couldn't start.")
					: QStringLiteral("The change couldn't be saved."));
			});
		process->start();
	}

	QString m_user;
	FrontDeskSettings m_settings;
	QLabel *m_title = nullptr;
	QStackedWidget *m_pages = nullptr;
	QLabel *m_pageLabel = nullptr;
	QComboBox *m_startup = nullptr;
	QComboBox *m_menus = nullptr;
	QComboBox *m_save = nullptr;
	QCheckBox *m_speech = nullptr;
	QCheckBox *m_removable = nullptr;
	QCheckBox *m_capture = nullptr;
	QCheckBox *m_finder = nullptr;
	QListWidget *m_programs = nullptr;
	QPushButton *m_cancel = nullptr;
	QPushButton *m_previous = nullptr;
	QPushButton *m_next = nullptr;
	QPushButton *m_done = nullptr;
	int m_page = 0;
};

class SetupWindow : public QWidget {
public:
	SetupWindow() {
		setWindowTitle(tr("Front Desk Setup"));
		resize(520, 380);

		auto *layout = new QVBoxLayout(this);
		auto *header = new QHBoxLayout();
		auto *logo = new QFrame(this);
		logo->setFrameStyle(QFrame::Panel | QFrame::Raised);
		logo->setMinimumWidth(180);
		auto *logoLayout = new QVBoxLayout(logo);
		auto *title = new QLabel(tr("Front Desk"), logo);
		QFont titleFont = title->font();
		titleFont.setPointSize(18);
		titleFont.setBold(true);
		title->setFont(titleFont);
		logoLayout->addWidget(title);
		logoLayout->addWidget(new QLabel(tr("Setup"), logo));
		header->addWidget(logo);
		auto *startBox = new QVBoxLayout();
		auto *startRow = new QHBoxLayout();
		auto *icon = new QLabel(this);
		icon->setPixmap(QIcon::fromTheme(QStringLiteral("computer")).pixmap(32, 32));
		startRow->addWidget(icon);
		m_frontDesk = new QCheckBox(tr("Front Desk starts for this person."), this);
		startRow->addWidget(m_frontDesk);
		startRow->addStretch(1);
		startBox->addLayout(startRow);
		startBox->addWidget(new QLabel(tr("Choose a person, then Open… for their settings."), this));
		header->addLayout(startBox, 1);
		layout->addLayout(header);
		layout->addWidget(new QLabel(tr("Users:"), this));

		auto *body = new QHBoxLayout();
		m_users = new QListWidget(this);
		m_users->setIconSize(QSize(24, 24));
		body->addWidget(m_users, 1);
		auto *buttons = new QVBoxLayout();
		m_newUser = new QPushButton(tr("New User…"), this);
		m_open = new QPushButton(tr("Open…"), this);
		m_programs = new QPushButton(tr("Programs…"), this);
		m_remove = new QPushButton(tr("Remove Front Desk…"), this);
		buttons->addWidget(m_newUser);
		buttons->addWidget(m_open);
		buttons->addWidget(m_programs);
		buttons->addWidget(m_remove);
		buttons->addStretch(1);
		body->addLayout(buttons);
		layout->addLayout(body, 1);

		auto *bottom = new QHBoxLayout();
		m_status = new QLabel(this);
		m_status->setWordWrap(true);
		bottom->addWidget(m_status, 1);
		m_close = new QPushButton(tr("Close"), this);
		bottom->addWidget(m_close);
		layout->addLayout(bottom);

		connect(m_close, &QPushButton::clicked, this, &QWidget::close);
		connect(m_users, &QListWidget::currentRowChanged, this, [this](int) { select(); });
		connect(m_frontDesk, &QCheckBox::clicked, this, [this](bool on) { toggle(on); });
		connect(m_open, &QPushButton::clicked, this, [this] { openAccount(); });
		connect(m_programs, &QPushButton::clicked, this, [this] { openPrograms(); });
		connect(m_remove, &QPushButton::clicked, this, [this] { remove(); });
		connect(m_newUser, &QPushButton::clicked, this, [this] { newUser(); });

		reload();
	}

private:
	QString currentUser() const {
		const QListWidgetItem *item = m_users->currentItem();
		return item ? item->data(Qt::UserRole).toString() : QString();
	}

	void reload() {
		const QString selected = currentUser();
		m_users->clear();
		for (const Account &account : accounts()) {
			auto *item = new QListWidgetItem(QIcon::fromTheme(QStringLiteral("user-info")),
				account.name + (frontDeskOn(account.user) ? tr("  —  Front Desk") : QString()), m_users);
			item->setData(Qt::UserRole, account.user);
			if (account.user == selected) {
				m_users->setCurrentItem(item);
			}
		}
		if (!m_users->currentItem() && m_users->count() > 0) {
			m_users->setCurrentRow(0);
		}
		select();
	}

	void select() {
		const QString user = currentUser();
		const bool enabled = !user.isEmpty();
		m_frontDesk->setEnabled(enabled);
		m_open->setEnabled(enabled);
		m_programs->setEnabled(enabled);
		m_remove->setEnabled(enabled && frontDeskOn(user));
		m_frontDesk->blockSignals(true);
		m_frontDesk->setChecked(enabled && frontDeskOn(user));
		m_frontDesk->blockSignals(false);
	}

	void toggle(bool on) {
		const QString user = currentUser();
		if (user.isEmpty()) {
			return;
		}
		setEnabled(false);
		m_status->setText(tr("Saving…"));
		runHelper({ QStringLiteral("desktop"), user, on ? QStringLiteral("frontdesk") : QStringLiteral("finder") },
			[this](bool ok, const QString &error) {
				setEnabled(true);
				m_status->clear();
				if (!ok) {
					QMessageBox::warning(this, tr("Front Desk Setup"), error);
				}
				reload();
			});
	}

	void openAccount() {
		const QString user = currentUser();
		if (user.isEmpty()) {
			return;
		}
		AccountDialog dialog(user, this);
		if (dialog.exec() == QDialog::Accepted) {
			reload();
		}
	}

	void openPrograms() {
		const QString user = currentUser();
		if (user.isEmpty()) {
			return;
		}
		ProgramsDialog dialog(user, this);
		if (dialog.exec() == QDialog::Accepted) {
			reload();
		}
	}

	void remove() {
		const QString user = currentUser();
		if (user.isEmpty() || QMessageBox::question(this, tr("Remove Front Desk"),
				tr("Take %1 off Front Desk? The account stays.").arg(user),
				QMessageBox::Cancel | QMessageBox::Yes) != QMessageBox::Yes) {
			return;
		}
		setEnabled(false);
		m_status->setText(tr("Saving…"));
		runHelper({ QStringLiteral("desktop"), user, QStringLiteral("finder") },
			[this](bool ok, const QString &error) {
				setEnabled(true);
				m_status->clear();
				if (!ok) {
					QMessageBox::warning(this, tr("Front Desk Setup"), error);
				}
				reload();
			});
	}

	void newUser() {
		auto *process = new QProcess(this);
		process->setProgram(QStringLiteral("zacos9-controlpanel"));
		process->setArguments({ QStringLiteral("users") });
		connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
			[this, process](int, QProcess::ExitStatus) {
				process->deleteLater();
				reload();
			});
		connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError) {
			process->deleteLater();
			QMessageBox::warning(this, tr("Front Desk Setup"),
				tr("The Users control panel couldn't be opened."));
		});
		process->start();
	}

	void runHelper(const QStringList &args, std::function<void(bool, const QString &)> done) {
		const QString helper = helperPath();
		if (helper.isEmpty()) {
			done(false, tr("The accounts helper isn't installed."));
			return;
		}
		auto *process = new QProcess(this);
		if (geteuid() == 0) {
			process->setProgram(helper);
			process->setArguments(args);
		} else {
			process->setProgram(QStringLiteral("pkexec"));
			process->setArguments(QStringList{ helper } + args);
		}
		connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
			[process, done](int code, QProcess::ExitStatus status) {
				const QString error = QString::fromUtf8(process->readAllStandardError()).trimmed();
				process->deleteLater();
				done(status == QProcess::NormalExit && code == 0,
					error.isEmpty() ? QStringLiteral("The change couldn't be saved.") : error);
			});
		connect(process, &QProcess::errorOccurred, this,
			[process, done](QProcess::ProcessError error) {
				process->deleteLater();
				done(false, error == QProcess::FailedToStart
					? QStringLiteral("The accounts helper couldn't start.")
					: QStringLiteral("The change couldn't be saved."));
			});
		process->start();
	}

	QListWidget *m_users = nullptr;
	QCheckBox *m_frontDesk = nullptr;
	QPushButton *m_newUser = nullptr;
	QPushButton *m_open = nullptr;
	QPushButton *m_programs = nullptr;
	QPushButton *m_remove = nullptr;
	QPushButton *m_close = nullptr;
	QLabel *m_status = nullptr;
};

} // namespace

int main(int argc, char **argv) {
	QApplication app(argc, argv);
	QApplication::setApplicationName(QStringLiteral("Front Desk Setup"));
	QApplication::setApplicationDisplayName(QStringLiteral("Front Desk Setup"));
	QApplication::setDesktopFileName(QStringLiteral("zacos9-frontdesk-setup"));
	QApplication::setWindowIcon(QIcon::fromTheme(QStringLiteral("computer")));
	if (QStyleFactory::keys().contains(QStringLiteral("zacos9"), Qt::CaseInsensitive)) {
		QApplication::setStyle(QStyleFactory::create(QStringLiteral("zacos9")));
	}

	if (argc > 1 && QString::fromLocal8Bit(argv[1]) == QStringLiteral("--check")) {
		std::printf("Front Desk Setup: accounts=%zu\n", accounts().size());
		return 0;
	}
	SetupWindow window;
	window.show();
	return app.exec();
}
