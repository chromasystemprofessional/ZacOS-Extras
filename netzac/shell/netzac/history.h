#pragma once

#include <QString>
#include <QVector>

struct HistoryEntry {
    QString title;
    QString url;
    qint64 visitedAt = 0;
};

class HistoryStorage {
public:
    static QString defaultFilePath();
    static QVector<HistoryEntry> load(const QString &filePath = defaultFilePath());
    static bool save(
        const QVector<HistoryEntry> &entries,
        const QString &filePath = defaultFilePath());
    static void recordVisit(
        QVector<HistoryEntry> &entries,
        const QString &url,
        const QString &title,
        int maxEntries = 1000);
};
