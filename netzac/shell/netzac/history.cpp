#include "history.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

QString HistoryStorage::defaultFilePath()
{
    QString cfgDir = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    if (cfgDir.isEmpty())
        cfgDir = QDir::homePath() + QStringLiteral("/.config");
    return cfgDir + QStringLiteral("/zacos9/netzac-history.json");
}

QVector<HistoryEntry> HistoryStorage::load(const QString &filePath)
{
    QVector<HistoryEntry> out;
    QFile f(filePath);
    if (!f.exists() || !f.open(QIODevice::ReadOnly))
        return out;

    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isArray())
        return out;

    const QJsonArray arr = doc.array();
    out.reserve(arr.size());
    for (const QJsonValue &val : arr) {
        if (!val.isObject())
            continue;
        const QJsonObject obj = val.toObject();
        const QString url = obj.value(QStringLiteral("url")).toString().trimmed();
        if (url.isEmpty())
            continue;
        HistoryEntry entry;
        entry.url = url;
        entry.title = obj.value(QStringLiteral("title")).toString().trimmed();
        entry.visitedAt = qint64(obj.value(QStringLiteral("visitedAt")).toDouble(0));
        out.append(entry);
    }
    return out;
}

bool HistoryStorage::save(const QVector<HistoryEntry> &entries, const QString &filePath)
{
    const QFileInfo fi(filePath);
    QDir().mkpath(fi.absolutePath());

    QJsonArray arr;
    for (const HistoryEntry &entry : entries) {
        if (entry.url.isEmpty())
            continue;
        QJsonObject obj;
        obj.insert(QStringLiteral("title"), entry.title);
        obj.insert(QStringLiteral("url"), entry.url);
        obj.insert(QStringLiteral("visitedAt"), double(entry.visitedAt));
        arr.append(obj);
    }

    QFile f(filePath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    f.write(QJsonDocument(arr).toJson(QJsonDocument::Compact));
    f.close();
    return true;
}

void HistoryStorage::recordVisit(
    QVector<HistoryEntry> &entries,
    const QString &url,
    const QString &title,
    int maxEntries)
{
    const QString cleanUrl = url.trimmed();
    if (cleanUrl.isEmpty()
        || cleanUrl == QLatin1String("about:blank")
        || cleanUrl == QLatin1String("https://localhost/")
        || cleanUrl.startsWith(QLatin1String("data:"))) {
        return;
    }

    QString resolvedTitle = title.trimmed();
    for (int i = 0; i < entries.size(); ++i) {
        if (entries.at(i).url == cleanUrl) {
            if (resolvedTitle.isEmpty())
                resolvedTitle = entries.at(i).title;
            entries.removeAt(i);
            break;
        }
    }

    HistoryEntry entry;
    entry.url = cleanUrl;
    entry.title = resolvedTitle;
    entry.visitedAt = QDateTime::currentSecsSinceEpoch();
    entries.prepend(entry);

    if (maxEntries > 0 && entries.size() > maxEntries)
        entries.resize(maxEntries);
}
