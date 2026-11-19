#include "frontdesk.h"

#include "art.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QCloseEvent>
#include <QDBusConnection>
#include <QDesktopServices>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontMetrics>
#include <QInputDialog>
#include <QKeySequence>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QProcess>
#include <QStandardPaths>
#include <QUrl>

#include <algorithm>

class Canvas : public QWidget {
public:
	explicit Canvas(FrontDeskWindow *window) : QWidget(window), m_window(window) {
		setMouseTracking(true);
		setFocusPolicy(Qt::StrongFocus);
	}

protected:
	void paintEvent(QPaintEvent *) override {
		QPainter p(this);
		m_window->paintCanvas(p);
	}
	void mousePressEvent(QMouseEvent *e) override { m_window->canvasMousePress(e); }
	void mouseMoveEvent(QMouseEvent *e) override { m_window->canvasMouseMove(e); }
	void mouseReleaseEvent(QMouseEvent *e) override { m_window->canvasMouseRelease(e); }

private:
	FrontDeskWindow *m_window;
};

namespace {

const QColor kBlack(0, 0, 0);

QString elide(const QFont &font, const QString &text, int width) {
	return QFontMetrics(font).elidedText(text, Qt::ElideRight, width);
}

QIcon themeIcon(const QString &name, const QString &fallback) {
	QIcon icon = QIcon::fromTheme(name);
	if (icon.isNull()) {
		icon = QIcon::fromTheme(fallback);
	}
	return icon;
}

} // namespace

FrontDeskWindow::FrontDeskWindow(bool preview) : m_preview(preview) {
	setWindowTitle(QStringLiteral("Front Desk"));
	setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
	setMinimumSize(400, 320);
	setCentralWidget(new Canvas(this));
	buildMenus();
	connect(&m_speechTimer, &QTimer::timeout, this, [this] {
		if (m_settings.speech && m_hover >= 0 && m_hover < int(m_cells.size())) {
			speak(m_cells[m_hover].label);
		}
	});
	m_speechTimer.setSingleShot(true);
	if (m_preview) {
		resize(615, 460);
	}
	reload();
}

void FrontDeskWindow::buildMenus() {
	QMenu *file = menuBar()->addMenu(tr("File"));
	m_actNewFolder = file->addAction(tr("New Folder…"));
	m_actNewFolder->setShortcut(QKeySequence::New);
	connect(m_actNewFolder, &QAction::triggered, this, [this] { newFolder(); });
	m_actOpenOther = file->addAction(tr("Open Other…"));
	m_actOpenOther->setShortcut(QKeySequence::Open);
	connect(m_actOpenOther, &QAction::triggered, this, [this] { openOther(); });
	m_actClose = file->addAction(tr("Close"));
	m_actClose->setShortcut(QKeySequence::Close);
	connect(m_actClose, &QAction::triggered, this, [this] { goUp(); });
	file->addSeparator();
	m_actRename = file->addAction(tr("Rename…"));
	m_actRename->setShortcut(QKeySequence(QStringLiteral("Ctrl+R")));
	connect(m_actRename, &QAction::triggered, this, [this] { renameSelected(); });
	m_actDelete = file->addAction(tr("Delete…"));
	m_actDelete->setShortcut(QKeySequence::Delete);
	connect(m_actDelete, &QAction::triggered, this, [this] { deleteSelected(); });
	m_actGetInfo = file->addAction(tr("Get Info…"));
	m_actGetInfo->setShortcut(QKeySequence(QStringLiteral("Ctrl+I")));
	connect(m_actGetInfo, &QAction::triggered, this, [this] { getInfo(); });
	m_actFind = file->addAction(tr("Find…"));
	m_actFind->setShortcut(QKeySequence::Find);
	connect(m_actFind, &QAction::triggered, this, [this] { findItem(); });
	file->addSeparator();
	m_actCopyTo = file->addAction(tr("Copy To Removable…"));
	m_actCopyTo->setEnabled(false);
	m_actCopyFrom = file->addAction(tr("Copy From Removable…"));
	m_actCopyFrom->setEnabled(false);
	file->addSeparator();
	m_actQuit = file->addAction(tr("Quit"));
	m_actQuit->setShortcut(QKeySequence::Quit);
	connect(m_actQuit, &QAction::triggered, this, [this] { logOut(); });

	QMenu *edit = menuBar()->addMenu(tr("Edit"));
	const struct {
		QString text;
		QKeySequence::StandardKey key;
	} edits[] = {
		{ tr("Undo"), QKeySequence::Undo },
		{ tr("Cut"), QKeySequence::Cut },
		{ tr("Copy"), QKeySequence::Copy },
		{ tr("Paste"), QKeySequence::Paste },
		{ tr("Clear"), QKeySequence::Delete },
		{ tr("Select All"), QKeySequence::SelectAll },
	};
	for (const auto &entry : edits) {
		QAction *action = edit->addAction(entry.text);
		action->setShortcut(entry.key);
		action->setEnabled(false);
	}

	m_viewMenu = menuBar()->addMenu(tr("View"));
	QMenu *view = m_viewMenu;
	auto *sortGroup = new QActionGroup(this);
	sortGroup->setExclusive(true);
	m_actSortName = view->addAction(tr("By Name"));
	m_actSortSize = view->addAction(tr("By Size"));
	m_actSortKind = view->addAction(tr("By Kind"));
	m_actSortDate = view->addAction(tr("By Date"));
	for (QAction *action : { m_actSortName, m_actSortSize, m_actSortKind, m_actSortDate }) {
		action->setCheckable(true);
		sortGroup->addAction(action);
	}
	m_actSortName->setChecked(true);
	connect(m_actSortName, &QAction::triggered, this, [this] { m_sort = Sort::Name; m_page = 0; sortCells(); update(); });
	connect(m_actSortSize, &QAction::triggered, this, [this] { m_sort = Sort::Size; m_page = 0; sortCells(); update(); });
	connect(m_actSortKind, &QAction::triggered, this, [this] { m_sort = Sort::Kind; m_page = 0; sortCells(); update(); });
	connect(m_actSortDate, &QAction::triggered, this, [this] { m_sort = Sort::Date; m_page = 0; sortCells(); update(); });

	QMenu *special = menuBar()->addMenu(tr("Special"));
	m_actSound = special->addAction(tr("Turn Sound Off"));
	connect(m_actSound, &QAction::triggered, this, [this] {
		m_sound = !m_sound;
		m_actSound->setText(m_sound ? tr("Turn Sound Off") : tr("Turn Sound On"));
	});
	m_actEject = special->addAction(tr("Eject Disk…"));
	m_actEject->setShortcut(QKeySequence(QStringLiteral("Ctrl+E")));
	m_actEject->setEnabled(false);
	special->addSeparator();
	m_actSetup = special->addAction(tr("Go To Front Desk Setup…"));
	connect(m_actSetup, &QAction::triggered, this, [this] { goToSetup(); });
	m_actFinder = special->addAction(tr("Go To Finder"));
	connect(m_actFinder, &QAction::triggered, this, [this] { goToFinder(); });
	special->addSeparator();
	m_actRestart = special->addAction(tr("Restart"));
	connect(m_actRestart, &QAction::triggered, this, [this] { power(QStringLiteral("reboot")); });
	m_actShutdown = special->addAction(tr("Shut Down"));
	connect(m_actShutdown, &QAction::triggered, this, [this] { power(QStringLiteral("poweroff")); });

	QMenu *panels = menuBar()->addMenu(tr("Panels"));
	m_actPanelApps = panels->addAction(tr("Front Desk Items"));
	m_actPanelUser = panels->addAction(tr("Documents"));
	for (QAction *action : { m_actPanelApps, m_actPanelUser }) {
		action->setCheckable(true);
	}
	connect(m_actPanelApps, &QAction::triggered, this, [this] { setPanel(Panel::Apps); });
	connect(m_actPanelUser, &QAction::triggered, this, [this] { setPanel(Panel::Documents); });
}

void FrontDeskWindow::applyMenuLevel() {
	const bool all = m_settings.menus == QStringLiteral("all");
	const bool restricted = all || m_settings.menus == QStringLiteral("restricted");
	for (QAction *action : { m_actNewFolder, m_actOpenOther, m_actRename, m_actDelete,
			m_actCopyTo, m_actCopyFrom }) {
		action->setVisible(all);
	}
	for (QAction *action : { m_actGetInfo, m_actFind }) {
		action->setVisible(restricted);
	}
	if (m_viewMenu) {
		m_viewMenu->menuAction()->setVisible(restricted);
	}
	m_actFinder->setVisible(m_settings.finder);
	m_actClose->setEnabled(m_panel == Panel::Documents &&
		QFileInfo(currentDir()).absoluteFilePath() != QFileInfo(m_settings.documents).absoluteFilePath());
	m_actRename->setEnabled(m_selected >= 0 && m_panel == Panel::Documents);
	m_actDelete->setEnabled(m_selected >= 0 && m_panel == Panel::Documents);
	m_actGetInfo->setEnabled(m_selected >= 0);
	m_actPanelApps->setChecked(m_panel == Panel::Apps);
	m_actPanelUser->setChecked(m_panel == Panel::Documents);
}

void FrontDeskWindow::reload() {
	m_settings = FrontDeskSettings::load();
	m_apps = AppCatalog::forCurrentUser();
	m_docsDir = m_settings.documents;
	loadDocuments();
	m_page = 0;
	m_hover = -1;
	m_selected = -1;
	m_pressed = -1;
	m_pressHit = {};
	buildCells();
	m_actPanelUser->setText(m_settings.displayName.isEmpty()
		? tr("Documents") : m_settings.displayName);
	applyMenuLevel();
	update();
}

void FrontDeskWindow::loadDocuments() {
	QDir dir(m_docsDir);
	if (!dir.exists()) {
		QDir().mkpath(m_docsDir);
		dir = QDir(m_docsDir);
	}
	m_docs.clear();
	const QFileInfoList list = dir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot,
		QDir::Name);
	for (const QFileInfo &file : list) {
		m_docs.append(file);
	}
}

void FrontDeskWindow::buildCells() {
	m_cells.clear();
	if (m_panel == Panel::Apps) {
		for (int i = 0; i < int(m_apps.size()); i++) {
			m_cells.push_back({ m_apps[i].name, AppCatalog::iconFor(m_apps[i]), 0, i });
		}
	} else {
		for (int i = 0; i < m_docs.size(); i++) {
			const QFileInfo &file = m_docs[i];
			m_cells.push_back({ file.fileName(), m_icons.icon(file), file.isDir() ? 1 : 2, i });
		}
	}
	sortCells();
	if (m_page >= pageCount()) {
		m_page = qMax(0, pageCount() - 1);
	}
	applyMenuLevel();
}

void FrontDeskWindow::sortCells() {
	std::sort(m_cells.begin(), m_cells.end(), [this](const Cell &a, const Cell &b) {
		if (m_panel == Panel::Apps) {
			if (m_sort == Sort::Kind) {
				return m_apps[a.index].entry < m_apps[b.index].entry;
			}
			if (m_sort == Sort::Size || m_sort == Sort::Date) {
				const QFileInfo fa(m_apps[a.index].desktopFile);
				const QFileInfo fb(m_apps[b.index].desktopFile);
				if (m_sort == Sort::Size && fa.size() != fb.size()) {
					return fa.size() > fb.size();
				}
				if (m_sort == Sort::Date && fa.lastModified() != fb.lastModified()) {
					return fa.lastModified() > fb.lastModified();
				}
			}
			return a.label.compare(b.label, Qt::CaseInsensitive) < 0;
		}
		const QFileInfo &fa = m_docs[a.index];
		const QFileInfo &fb = m_docs[b.index];
		switch (m_sort) {
		case Sort::Size:
			if (fa.size() != fb.size()) {
				return fa.size() > fb.size();
			}
			break;
		case Sort::Kind:
			if (fa.isDir() != fb.isDir()) {
				return fa.isDir();
			}
			if (fa.suffix().compare(fb.suffix(), Qt::CaseInsensitive) != 0) {
				return fa.suffix().compare(fb.suffix(), Qt::CaseInsensitive) < 0;
			}
			break;
		case Sort::Date:
			if (fa.lastModified() != fb.lastModified()) {
				return fa.lastModified() > fb.lastModified();
			}
			break;
		case Sort::Name:
			break;
		}
		return a.label.compare(b.label, Qt::CaseInsensitive) < 0;
	});
}

void FrontDeskWindow::setPanel(Panel panel) {
	m_panel = panel;
	m_page = 0;
	m_hover = -1;
	m_selected = -1;
	m_pressed = -1;
	m_pressHit = {};
	buildCells();
	update();
}

QString FrontDeskWindow::currentDir() const {
	return m_docsDir.isEmpty() ? m_settings.documents : m_docsDir;
}

QRect FrontDeskWindow::outerRect() const {
	if (width() >= 595 && height() >= 430) {
		return QRect((width() - 575) / 2, (height() - 400) / 2, 575, 400);
	}
	return QRect(8, 8, qMax(200, width() - 16), qMax(200, height() - 16));
}

QRect FrontDeskWindow::bodyRect() const {
	return outerRect().adjusted(0, 24, -1, -1);
}

QRect FrontDeskWindow::appsTabRect() const {
	const QRect outer = outerRect();
	return QRect(outer.x() + 10, outer.y(), 220, 24);
}

QRect FrontDeskWindow::userTabRect() const {
	const QRect outer = outerRect();
	return QRect(outer.right() - 230, outer.y(), 220, 24);
}

QRect FrontDeskWindow::gridRect() const {
	return bodyRect().adjusted(20, 14, -20, -30);
}

QRect FrontDeskWindow::cellRect(int pageIndex) const {
	const QRect grid = gridRect();
	const int cellW = qMax(1, grid.width() / 4);
	const int cellH = qMax(1, grid.height() / 4);
	const int col = pageIndex % 4;
	const int row = pageIndex / 4;
	return QRect(grid.x() + col * cellW, grid.y() + row * cellH, cellW, cellH);
}

QRect FrontDeskWindow::pagePrevRect() const {
	const QRect body = bodyRect();
	return QRect(body.left() + 10, body.bottom() - 22, 22, 16);
}

QRect FrontDeskWindow::pageNextRect() const {
	const QRect body = bodyRect();
	return QRect(body.right() - 32, body.bottom() - 22, 22, 16);
}

int FrontDeskWindow::itemCount() const {
	return int(m_cells.size());
}

int FrontDeskWindow::pageCount() const {
	return qMax(1, (itemCount() + 15) / 16);
}

FrontDeskWindow::Hit FrontDeskWindow::hitTest(const QPoint &pos) const {
	if (appsTabRect().contains(pos)) {
		return { Hit::Type::AppsTab, -1 };
	}
	if (userTabRect().contains(pos)) {
		return { Hit::Type::UserTab, -1 };
	}
	if (pageCount() > 1) {
		if (pagePrevRect().contains(pos)) {
			return { Hit::Type::Prev, -1 };
		}
		if (pageNextRect().contains(pos)) {
			return { Hit::Type::Next, -1 };
		}
	}
	const int start = m_page * 16;
	const int count = qMin(16, itemCount() - start);
	for (int i = 0; i < count; i++) {
		if (cellRect(i).contains(pos)) {
			return { Hit::Type::Item, start + i };
		}
	}
	return {};
}

void FrontDeskWindow::activate(const Hit &hit) {
	switch (hit.type) {
	case Hit::Type::AppsTab:
		setPanel(Panel::Apps);
		break;
	case Hit::Type::UserTab:
		setPanel(Panel::Documents);
		break;
	case Hit::Type::Prev:
		if (m_page > 0) {
			m_page--;
			m_selected = -1;
			update();
		}
		break;
	case Hit::Type::Next:
		if (m_page + 1 < pageCount()) {
			m_page++;
			m_selected = -1;
			update();
		}
		break;
	case Hit::Type::Item:
		if (hit.index < 0 || hit.index >= itemCount()) {
			break;
		}
		m_selected = hit.index;
		if (m_panel == Panel::Apps) {
			launchApp(m_cells[hit.index].index);
		} else {
			openDocument(m_docs[m_cells[hit.index].index]);
		}
		break;
	case Hit::Type::None:
		break;
	}
}

void FrontDeskWindow::paintCanvas(QPainter &p) {
	p.setRenderHint(QPainter::Antialiasing, false);
	const QColor desk = Art::desk();
	Art::fillStipple(p, rect(), Art::stipple(desk, desk.darker(112)));

	const QRect outer = outerRect();
	const QRect body = bodyRect();
	p.fillRect(outer.translated(5, 5), QColor(0, 0, 0, 80));

	const QColor panel = m_panel == Panel::Apps ? Art::appPanel() : Art::docPanel();
	const QPixmap tile = Art::stipple(panel, panel.darker(108));
	p.fillRect(body, panel);
	Art::fillStipple(p, body, tile);
	p.setPen(QPen(kBlack, 1));
	p.drawRect(body.adjusted(0, 0, -1, -1));

	auto drawTab = [&](const QRect &rect, const QString &text, bool apps, bool active) {
		QPainterPath path;
		const QRect r = rect.adjusted(0, 0, -1, 0);
		path.moveTo(r.left(), r.bottom() + 1);
		path.lineTo(r.left(), r.top() + 6);
		path.quadTo(r.left(), r.top(), r.left() + 6, r.top());
		path.lineTo(r.right() - 6, r.top());
		path.quadTo(r.right(), r.top(), r.right(), r.top() + 6);
		path.lineTo(r.right(), r.bottom() + 1);
		path.closeSubpath();
		p.fillPath(path, Art::tabFace(apps, active));
		p.setPen(QPen(kBlack, 1));
		p.drawPath(path);
		const QIcon icon = themeIcon(apps ? QStringLiteral("applications-other")
			: QStringLiteral("folder"), QStringLiteral("folder"));
		const QPixmap pix = icon.pixmap(16, 16);
		if (!pix.isNull()) {
			p.drawPixmap(r.left() + 7, r.top() + 4, pix);
		}
		p.setFont(Art::tabFont());
		p.setPen(Art::tabText(active));
		p.drawText(r.adjusted(28, 0, -6, 0), Qt::AlignCenter | Qt::TextSingleLine,
			elide(Art::tabFont(), text, r.width() - 34));
	};

	const bool appsActive = m_panel == Panel::Apps;
	if (appsActive) {
		drawTab(userTabRect(), m_settings.displayName, false, false);
		drawTab(appsTabRect(), tr("Front Desk Items"), true, true);
	} else {
		drawTab(appsTabRect(), tr("Front Desk Items"), true, false);
		drawTab(userTabRect(), m_settings.displayName, false, true);
	}

	const int start = m_page * 16;
	const int count = qMin(16, itemCount() - start);
	if (count == 0) {
		p.setFont(Art::smallFont());
		p.setPen(QColor(0x30, 0x30, 0x30));
		const QString message = m_panel == Panel::Apps
			? tr("No programs. An administrator can choose them in Front Desk Setup.")
			: tr("No documents.");
		p.drawText(body.adjusted(30, 50, -30, -50), Qt::AlignCenter | Qt::TextWordWrap, message);
	}
	for (int i = 0; i < count; i++) {
		const QRect cell = cellRect(i);
		const QRect well(cell.center().x() - 24, cell.y() + 2, 48, 48);
		const QRect label(cell.x() + 8, cell.y() + 54, cell.width() - 16, 16);
		const int item = start + i;
		Art::bevel(p, well, item == m_pressed, item == m_hover);
		const QPixmap pix = m_cells[item].icon.pixmap(32, 32);
		if (!pix.isNull()) {
			p.drawPixmap(well.center().x() - 16, well.center().y() - 16, pix);
		} else {
			p.setPen(QPen(kBlack, 1));
			p.drawRect(well.adjusted(12, 12, -12, -12));
		}
		Art::label(p, label, m_cells[item].label, item == m_selected);
	}

	if (pageCount() > 1) {
		const QRect prev = pagePrevRect();
		const QRect next = pageNextRect();
		p.setBrush(QColor(0x30, 0x30, 0x30));
		p.setPen(Qt::NoPen);
		QPolygon left;
		left << QPoint(prev.left() + 4, prev.center().y())
			<< QPoint(prev.right() - 4, prev.top() + 2)
			<< QPoint(prev.right() - 4, prev.bottom() - 2);
		p.drawPolygon(left);
		QPolygon right;
		right << QPoint(next.right() - 4, next.center().y())
			<< QPoint(next.left() + 4, next.top() + 2)
			<< QPoint(next.left() + 4, next.bottom() - 2);
		p.drawPolygon(right);
		p.setPen(QColor(0x30, 0x30, 0x30));
		p.setFont(Art::smallFont());
		p.drawText(QRect(body.left(), body.bottom() - 22, body.width(), 16),
			Qt::AlignCenter, tr("Page %1 of %2").arg(m_page + 1).arg(pageCount()));
	}
}

void FrontDeskWindow::canvasMousePress(QMouseEvent *e) {
	if (e->button() != Qt::LeftButton) {
		return;
	}
	m_pressHit = hitTest(e->pos());
	if (m_pressHit.type == Hit::Type::Item) {
		m_selected = m_pressHit.index;
		m_pressed = m_pressHit.index;
	}
	update();
}

void FrontDeskWindow::canvasMouseMove(QMouseEvent *e) {
	const Hit hit = hitTest(e->pos());
	const int hover = hit.type == Hit::Type::Item ? hit.index : -1;
	if (hover != m_hover) {
		m_hover = hover;
		if (m_settings.speech && hover >= 0) {
			m_speechTimer.start(350);
		}
		update();
	}
}

void FrontDeskWindow::canvasMouseRelease(QMouseEvent *e) {
	if (e->button() != Qt::LeftButton) {
		return;
	}
	const Hit hit = hitTest(e->pos());
	if (hit.type == m_pressHit.type && hit.index == m_pressHit.index) {
		activate(hit);
	}
	m_pressHit = {};
	m_pressed = -1;
	update();
}

void FrontDeskWindow::launchApp(int index) {
	if (index < 0 || index >= int(m_apps.size())) {
		return;
	}
	const DeskApp &app = m_apps[index];
	if (app.command.isEmpty()) {
		alert(tr("Front Desk"), tr("“%1” doesn't say how to start.").arg(app.name));
		return;
	}
	auto *process = new QProcess(this);
	if (!app.workingDir.isEmpty()) {
		process->setWorkingDirectory(app.workingDir);
	}
	process->setProgram(app.command.first());
	process->setArguments(app.command.mid(1));
	connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
		[this, process](int, QProcess::ExitStatus) {
			process->deleteLater();
			if (m_preview) {
				showNormal();
			} else {
				showMaximized();
			}
			reload();
		});
	connect(process, &QProcess::errorOccurred, this,
		[this, process](QProcess::ProcessError error) {
			if (error != QProcess::FailedToStart) {
				return;
			}
			process->deleteLater();
			if (m_preview) {
				showNormal();
			} else {
				showMaximized();
			}
			alert(tr("Front Desk"), tr("“%1” couldn't be started.").arg(process->program()));
		});
	/* In the session shell the panel stays up as a program of its own: the
	 * compositor hides it when the program comes forward, and keeps it in
	 * the Application menu so the person can come back and start another.
	 * Opened as an ordinary window (a preview), it gets out of the way as
	 * before. */
	if (m_preview) {
		hide();
	}
	process->start();
}

void FrontDeskWindow::openDocument(const QFileInfo &file) {
	if (file.isDir()) {
		openFolder(file.absoluteFilePath());
		return;
	}
	if (!QDesktopServices::openUrl(QUrl::fromLocalFile(file.absoluteFilePath()))) {
		alert(tr("Front Desk"), tr("“%1” couldn't be opened.").arg(file.fileName()));
	}
}

void FrontDeskWindow::openFolder(const QString &path) {
	m_docsDir = path;
	m_page = 0;
	m_hover = -1;
	m_selected = -1;
	m_pressed = -1;
	loadDocuments();
	buildCells();
	update();
}

void FrontDeskWindow::goUp() {
	if (m_panel != Panel::Documents) {
		return;
	}
	const QString docs = QFileInfo(m_settings.documents).absoluteFilePath();
	const QString current = QFileInfo(currentDir()).absoluteFilePath();
	if (current == docs) {
		return;
	}
	const QString parent = QFileInfo(current).absolutePath();
	m_docsDir = parent.startsWith(docs) ? parent : docs;
	m_page = 0;
	m_hover = -1;
	m_selected = -1;
	loadDocuments();
	buildCells();
	update();
}

void FrontDeskWindow::newFolder() {
	if (m_panel != Panel::Documents) {
		setPanel(Panel::Documents);
	}
	bool ok = false;
	const QString name = QInputDialog::getText(this, tr("New Folder"), tr("Name:"),
		QLineEdit::Normal, QString(), &ok);
	if (!ok || name.trimmed().isEmpty()) {
		return;
	}
	if (name.contains('/') || name.contains(':')) {
		alert(tr("New Folder"), tr("A folder name can't contain “/” or “:”."));
		return;
	}
	if (!QDir(currentDir()).mkdir(name.trimmed())) {
		alert(tr("New Folder"), tr("“%1” couldn't be made.").arg(name.trimmed()));
		return;
	}
	reload();
	goUp();
	openFolder(QDir(currentDir()).filePath(name.trimmed()));
}

QString FrontDeskWindow::selectedPath() const {
	if (m_selected < 0 || m_selected >= itemCount()) {
		return QString();
	}
	const Cell &cell = m_cells[m_selected];
	if (m_panel == Panel::Apps) {
		return m_apps[cell.index].desktopFile;
	}
	return m_docs[cell.index].absoluteFilePath();
}

void FrontDeskWindow::renameSelected() {
	if (m_selected < 0 || m_selected >= itemCount()) {
		return;
	}
	if (m_panel == Panel::Apps) {
		alert(tr("Rename"), tr("Programs are chosen in Front Desk Setup."));
		return;
	}
	const QFileInfo file = m_docs[m_cells[m_selected].index];
	bool ok = false;
	const QString name = QInputDialog::getText(this, tr("Rename"), tr("New name:"),
		QLineEdit::Normal, file.fileName(), &ok);
	if (!ok || name.trimmed().isEmpty() || name == file.fileName()) {
		return;
	}
	if (name.contains('/') || name.contains(':')) {
		alert(tr("Rename"), tr("A name can't contain “/” or “:”."));
		return;
	}
	if (!QFile::rename(file.absoluteFilePath(), QDir(file.absolutePath()).filePath(name.trimmed()))) {
		alert(tr("Rename"), tr("“%1” couldn't be renamed.").arg(file.fileName()));
		return;
	}
	reload();
}

void FrontDeskWindow::deleteSelected() {
	if (m_selected < 0 || m_selected >= itemCount() || m_panel != Panel::Documents) {
		return;
	}
	const QFileInfo file = m_docs[m_cells[m_selected].index];
	if (QMessageBox::question(this, tr("Delete"),
			tr("Delete “%1”?%2").arg(file.fileName(),
				file.isDir() ? tr(" Everything in the folder goes too.") : QString()),
			QMessageBox::Cancel | QMessageBox::Yes) != QMessageBox::Yes) {
		return;
	}
	bool ok = file.isDir() ? QDir(file.absoluteFilePath()).removeRecursively()
		: QFile::remove(file.absoluteFilePath());
	if (!ok) {
		alert(tr("Delete"), tr("“%1” couldn't be deleted.").arg(file.fileName()));
		return;
	}
	m_selected = -1;
	reload();
}

void FrontDeskWindow::getInfo() {
	const QString path = selectedPath();
	if (path.isEmpty()) {
		QMessageBox::information(this, tr("Get Info"),
			tr("%1\n%2\n%3 items").arg(currentDir()).arg(m_settings.displayName)
				.arg(itemCount()));
		return;
	}
	const QFileInfo file(path);
	QMessageBox::information(this, tr("Get Info"),
		tr("%1\n%2\n%3 bytes\nModified %4")
			.arg(file.fileName(), file.absoluteFilePath())
			.arg(file.size())
			.arg(file.lastModified().toString(Qt::TextDate)));
}

void FrontDeskWindow::findItem() {
	bool ok = false;
	const QString needle = QInputDialog::getText(this, tr("Find"), tr("Find:"),
		QLineEdit::Normal, QString(), &ok);
	if (!ok || needle.trimmed().isEmpty()) {
		return;
	}
	QStringList found;
	QDirIterator it(currentDir(), QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot,
		QDirIterator::Subdirectories);
	while (it.hasNext() && found.size() < 20) {
		it.next();
		if (it.fileName().contains(needle, Qt::CaseInsensitive)) {
			found << it.filePath();
		}
	}
	QMessageBox::information(this, tr("Find"),
		found.isEmpty() ? tr("No items match “%1”.").arg(needle) : found.join('\n'));
}

void FrontDeskWindow::openOther() {
	const QString path = QFileDialog::getOpenFileName(this, tr("Open Other"), currentDir());
	if (path.isEmpty()) {
		return;
	}
	if (!QDesktopServices::openUrl(QUrl::fromLocalFile(path))) {
		alert(tr("Open Other"), tr("“%1” couldn't be opened.").arg(path));
	}
}

void FrontDeskWindow::goToSetup() {
	auto *process = new QProcess(this);
	process->setProgram(QStringLiteral("zacos9-frontdesk-setup"));
	connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
		[this, process](int, QProcess::ExitStatus) {
			process->deleteLater();
			reload();
		});
	connect(process, &QProcess::errorOccurred, this,
		[this, process](QProcess::ProcessError) {
			process->deleteLater();
			alert(tr("Front Desk Setup"),
				tr("Front Desk Setup isn't installed. Install the Front Desk extra first."));
		});
	process->start();
}

void FrontDeskWindow::goToFinder() {
	if (!m_settings.finder) {
		return;
	}
	QProcess::startDetached(QStringLiteral("zacos9-finder"));
	showMinimized();
}

void FrontDeskWindow::logOut() {
	if (m_preview) {
		qApp->quit();
		return;
	}
	const QString session = qEnvironmentVariable("XDG_SESSION_ID");
	if (session.isEmpty() || !QProcess::startDetached(QStringLiteral("loginctl"),
			{ QStringLiteral("terminate-session"), session })) {
		qApp->quit();
	}
}

void FrontDeskWindow::power(const QString &action) {
	if (!QProcess::startDetached(QStringLiteral("systemctl"), { action })) {
		alert(tr("Front Desk"), tr("The computer couldn't %1.")
			.arg(action == QStringLiteral("poweroff") ? tr("shut down") : tr("restart")));
	}
}

void FrontDeskWindow::speak(const QString &text) {
	if (!m_settings.speech || text.isEmpty()) {
		return;
	}
	QProcess::startDetached(QStringLiteral("spd-say"), { text });
}

void FrontDeskWindow::alert(const QString &title, const QString &text) {
	QMessageBox::warning(this, title, text, QMessageBox::Ok);
}

void FrontDeskWindow::closeEvent(QCloseEvent *e) {
	if (!m_preview) {
		e->ignore();
		return;
	}
	QMainWindow::closeEvent(e);
}
