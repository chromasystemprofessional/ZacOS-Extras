#pragma once

#include <QPair>
#include <QString>
#include <QVector>

class BookmarkStorage {
public:
    using BookmarkList = QVector<QPair<QString, QString>>;

    static QString defaultFilePath();
    static BookmarkList defaultBookmarks();

    static BookmarkList load(const QString &filePath = defaultFilePath());
    static bool save(const BookmarkList &bookmarks, const QString &filePath = defaultFilePath());

    // Imports Netscape/Chrome/Firefox HTML bookmarks or Chrome JSON Bookmarks file.
    static BookmarkList importFromFile(const QString &filePath, bool *ok = nullptr);
    static bool exportToHtmlFile(const BookmarkList &bookmarks, const QString &filePath);
};
