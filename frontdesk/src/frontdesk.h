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
class QMenu;
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

	/* One chosen folder's page. Each page keeps its own place in the
	 * folder, and its own selected item. */
	struct FolderPage {
		QString root;
		QString current;
		int page = 0;
		int selected = -1;
	};
	enum class Sort { Name, Size, Kind, Date };
	struct Cell {
		QString label;
		QIcon icon;
		int kind = 0; /* 0 app, 1 folder, 2 file */
		int index = 0;
	};
	struct Hit {
		/* Tab 0 is Front Desk Items; 1 and up are the folder pages. */
		enum class Type { None, Tab, Prev, Next, Item } type = Type::None;
		int index = -1;
	};

	void buildMenus();
	void applyMenuLevel();
	void loadFolders();
	void reloadFolders();
	void rebuildPanelsMenu();
	void editFolders();
	void loadDocuments();
	void buildCells();
	void setPanel(int panel);
	void sortCells();
	bool appsPanel() const { return m_panel == 0; }
	FolderPage *activeFolder();
	const FolderPage *activeFolder() const;
	int page() const;
	void setPage(int page);
	int selected() const;
	void setSelected(int selected);
	QString currentDir() const;
	QString folderLabel(int index) const;
	QRect outerRect() const;
	QRect bodyRect() const;
	QVector<QRect> tabRects() const;
	QRect tabRect(int index) const;
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
	std::vector<FolderPage> m_folderPages;
	bool m_foldersLoaded = false;
	int m_panel = 0; /* 0: Front Desk Items; 1 and up: folder pages */
	int m_appPage = 0;
	int m_appSelected = -1;
	Sort m_sort = Sort::Name;
	int m_hover = -1;
	int m_pressed = -1;
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
	QAction *m_actFolders = nullptr;
	QAction *m_actFinder = nullptr;
	QAction *m_actRestart = nullptr;
	QAction *m_actShutdown = nullptr;
	QMenu *m_panelsMenu = nullptr;
	QVector<QAction *> m_panelActions;
	QAction *m_actSortName = nullptr;
	QAction *m_actSortSize = nullptr;
	QAction *m_actSortKind = nullptr;
	QAction *m_actSortDate = nullptr;
	QMenu *m_viewMenu = nullptr;
};
