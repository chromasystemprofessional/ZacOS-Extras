#include "bookmarks.h"

#include "i18n.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTextStream>

namespace {

QString decodeBasicHtmlEntities(QString text)
{
    text.replace(QStringLiteral("&amp;"), QStringLiteral("&"));
    text.replace(QStringLiteral("&lt;"), QStringLiteral("<"));
    text.replace(QStringLiteral("&gt;"), QStringLiteral(">"));
    text.replace(QStringLiteral("&quot;"), QStringLiteral("\""));
    text.replace(QStringLiteral("&#39;"), QStringLiteral("'"));
    text.replace(QStringLiteral("&apos;"), QStringLiteral("'"));
    return text;
}

void collectChromeJsonNode(const QJsonObject &obj, BookmarkStorage::BookmarkList &out)
{
    const QString type = obj.value(QStringLiteral("type")).toString();
    if (type == QLatin1String("url")) {
        const QString url = obj.value(QStringLiteral("url")).toString().trimmed();
        if (!url.isEmpty()) {
            const QString name = obj.value(QStringLiteral("name")).toString().trimmed();
            out.append({name.isEmpty() ? url : name, url});
        }
        return;
    }

    const QJsonArray children = obj.value(QStringLiteral("children")).toArray();
    for (const QJsonValue &val : children) {
        if (val.isObject())
            collectChromeJsonNode(val.toObject(), out);
    }
}

BookmarkStorage::BookmarkList parseNetscapeHtmlBookmarks(const QString &html)
{
    BookmarkStorage::BookmarkList out;
    // Matches <A HREF="...">Title</A> (case-insensitive, supports single or double quotes)
    static const QRegularExpression linkRe(
        QStringLiteral("<a\\b[^>]*\\bhref\\s*=\\s*(?:\"([^\"]*)\"|'([^']*)')[^>]*>(.*?)</a>"),
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption);
    static const QRegularExpression tagStripRe(QStringLiteral("<[^>]*>"));

    QRegularExpressionMatchIterator it = linkRe.globalMatch(html);
    while (it.hasNext()) {
        const QRegularExpressionMatch m = it.next();
        QString url = m.captured(1);
        if (url.isEmpty())
            url = m.captured(2);
        url = decodeBasicHtmlEntities(url).trimmed();
        if (url.isEmpty())
            continue;

        QString title = m.captured(3);
        title.remove(tagStripRe);
        title = decodeBasicHtmlEntities(title).simplified();
        if (title.isEmpty())
            title = url;
        out.append({title, url});
    }
    return out;
}

}  // namespace

QString BookmarkStorage::defaultFilePath()
{
    QString cfgDir = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    if (cfgDir.isEmpty())
        cfgDir = QDir::homePath() + QStringLiteral("/.config");
    return cfgDir + QStringLiteral("/zacos9/netzac-bookmarks.html");
}

BookmarkStorage::BookmarkList BookmarkStorage::defaultBookmarks()
{
    return {
        {ztr("FrogFind! Search"), QStringLiteral("https://frogfind.com")},
        {ztr("Wiby - Old Web Search"), QStringLiteral("https://wiby.me")},
        {ztr("68k.news - Headlines"), QStringLiteral("http://68k.news")},
        {ztr("The Old Net"), QStringLiteral("https://theoldnet.com")},
        {ztr("NPR Text Only"), QStringLiteral("https://text.npr.org")},
        {ztr("Welcome to Netzac"), QStringLiteral("about:netzac")},
    };
}

BookmarkStorage::BookmarkList BookmarkStorage::load(const QString &filePath)
{
    QFile f(filePath);
    if (!f.exists())
        return defaultBookmarks();
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return defaultBookmarks();

    const QString content = QString::fromUtf8(f.readAll());
    return parseNetscapeHtmlBookmarks(content);
}

bool BookmarkStorage::save(const BookmarkList &bookmarks, const QString &filePath)
{
    return exportToHtmlFile(bookmarks, filePath);
}

BookmarkStorage::BookmarkList BookmarkStorage::importFromFile(const QString &filePath, bool *ok)
{
    if (ok)
        *ok = false;

    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly))
        return {};

    const QByteArray raw = f.readAll();
    const QByteArray trimmed = raw.trimmed();

    if (trimmed.startsWith('{')) {
        QJsonParseError err;
        const QJsonDocument doc = QJsonDocument::fromJson(raw, &err);
        if (err.error != QJsonParseError::NoError || !doc.isObject())
            return {};

        BookmarkList out;
        const QJsonObject roots = doc.object().value(QStringLiteral("roots")).toObject();
        for (const QString &key : roots.keys()) {
            const QJsonValue v = roots.value(key);
            if (v.isObject())
                collectChromeJsonNode(v.toObject(), out);
        }
        if (ok)
            *ok = true;
        return out;
    }

    BookmarkList out = parseNetscapeHtmlBookmarks(QString::fromUtf8(raw));
    if (ok)
        *ok = true;
    return out;
}

bool BookmarkStorage::exportToHtmlFile(const BookmarkList &bookmarks, const QString &filePath)
{
    const QFileInfo fi(filePath);
    QDir().mkpath(fi.absolutePath());

    QFile f(filePath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;

    QTextStream out(&f);
    out << "<!DOCTYPE NETSCAPE-Bookmark-file-1>\n"
        << "<!-- This is an automatically generated file.\n"
        << "     It will be read and overwritten.\n"
        << "     DO NOT EDIT! -->\n"
        << "<META HTTP-EQUIV=\"Content-Type\" CONTENT=\"text/html; charset=UTF-8\">\n"
        << "<TITLE>Bookmarks</TITLE>\n"
        << "<H1>Bookmarks</H1>\n"
        << "<DL><p>\n";

    for (const auto &entry : bookmarks) {
        const QString href = entry.second.toHtmlEscaped();
        const QString title = entry.first.toHtmlEscaped();
        out << "    <DT><A HREF=\"" << href << "\">" << title << "</A>\n";
    }

    out << "</DL><p>\n";
    f.close();
    return true;
}
