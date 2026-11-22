#pragma once

#include <QFileIconProvider>
#include <QFileInfo>
#include <QIcon>
#include <QMainWindow>
#include <QTimer>
#include <QVector>

#include <vector>

#include "apps.h"
#include "settings.h"
#include "sound.h"

class QAction;
class QCloseEvent;
class QMouseEvent;
class QPainter;
class Canvas;

class FrontDeskWindow : public QMainWindow {
public:
	explicit FrontDeskWindow(bool preview);
	void reload();
	void paintCanvas(QPainter &p);
	void canvasMousePress(QMouseEvent *e);
	void canvasMouseMove(QMouseEvent *e);
	void canvasMouseRelease(QMouseEvent *e);

protected:
	void closeEvent(QCloseEvent *e) override;

private:
	friend class Canvas;

	enum class Panel { Apps, Documents };
	enum class Sort { Name, Size, Kind, Date };
	struct Cell {
		QString label;
		QIcon icon;
		int kind = 0; /* 0 app, 1 folder, 2 file */
		int index = 0;
	};
	struct Hit {
		enum class Type { None, AppsTab, UserTab, Prev, Next, Item } type = Type::None;
		int index = -1;
	};

	void buildMenus();
	void applyMenuLevel();
	void loadDocuments();
	void buildCells();
	void setPanel(Panel panel);
	void sortCells();
	QString currentDir() const;
	QRect outerRect() const;
	QRect bodyRect() const;
	QRect appsTabRect() const;
	QRect userTabRect() const;
	QRect gridRect() const;
	QRect cellRect(int pageIndex) const;
	QRect pagePrevRect() const;
	QRect pageNextRect() const;
	int pageCount() const;
	int itemCount() const;
	Hit hitTest(const QPoint &pos) const;
	void activate(const Hit &hit);
	void launchApp(int index);
	void openDocument(const QFileInfo &file);
	void openFolder(const QString &path);
	void goUp();
	void newFolder();
	void renameSelected();
	void deleteSelected();
	void getInfo();
	void findItem();
	void openOther();
	void goToSetup();
	void goToFinder();
	void logOut();
	void power(const QString &action);
	void speak(const QString &text);
	void clickSound();
	void openSound();
	void alertSound();
	void alert(const QString &title, const QString &text);
	QString selectedPath() const;

	bool m_preview = false;
	FrontDeskSettings m_settings;
	std::vector<DeskApp> m_apps;
	QVector<QFileInfo> m_docs;
	std::vector<Cell> m_cells;
	QString m_docsDir;
	Panel m_panel = Panel::Documents;
	Sort m_sort = Sort::Name;
	int m_page = 0;
	int m_hover = -1;
	int m_pressed = -1;
	int m_selected = -1;
	Hit m_pressHit;
	bool m_sound = true;
	FrontDeskSound m_sounds;
	QFileIconProvider m_icons;
	QTimer m_speechTimer;

	QAction *m_actNewFolder = nullptr;
	QAction *m_actOpenOther = nullptr;
	QAction *m_actClose = nullptr;
	QAction *m_actRename = nullptr;
	QAction *m_actDelete = nullptr;
	QAction *m_actGetInfo = nullptr;
	QAction *m_actFind = nullptr;
	QAction *m_actCopyTo = nullptr;
	QAction *m_actCopyFrom = nullptr;
	QAction *m_actQuit = nullptr;
	QAction *m_actSound = nullptr;
	QAction *m_actEject = nullptr;
	QAction *m_actSetup = nullptr;
	QAction *m_actFinder = nullptr;
	QAction *m_actRestart = nullptr;
	QAction *m_actShutdown = nullptr;
	QAction *m_actPanelApps = nullptr;
	QAction *m_actPanelUser = nullptr;
	QAction *m_actSortName = nullptr;
	QAction *m_actSortSize = nullptr;
	QAction *m_actSortKind = nullptr;
	QAction *m_actSortDate = nullptr;
	QMenu *m_viewMenu = nullptr;
};
