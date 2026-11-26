#include "adblocker.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>
#include <QWebEngineProfile>
#include <QWebEngineUrlRequestInfo>

namespace {

constexpr int kAnyParty = 0;
constexpr int kThirdParty = 1;
constexpr int kFirstParty = 2;

bool hostMatchesSuffix(const QString &host, const QString &suffix)
{
    if (host == suffix)
        return true;
    if (host.size() <= suffix.size() || !host.endsWith(suffix))
        return false;
    return host.at(host.size() - suffix.size() - 1) == QLatin1Char('.');
}

bool isSameSite(const QString &host, const QString &firstPartyHost)
{
    if (host.isEmpty() || firstPartyHost.isEmpty())
        return true;
    return hostMatchesSuffix(host, firstPartyHost) || hostMatchesSuffix(firstPartyHost, host);
}

bool isBlockableHostsDomain(const QString &domain)
{
    if (domain.isEmpty() || !domain.contains(QLatin1Char('.')))
        return false;
    if (domain == QLatin1String("localhost") || domain == QLatin1String("localhost.localdomain")
        || domain == QLatin1String("broadcasthost") || domain.startsWith(QLatin1String("ip6-"))) {
        return false;
    }
    return true;
}

// Plain "one domain per line" lists (URLHaus/domains style) are accepted, but
// only when the token looks like a hostname so adblock patterns such as
// "-ad-banner." or "/pagead/" are not mistaken for domains.
bool looksLikeDomainToken(const QString &token)
{
    if (token.size() < 4 || !token.contains(QLatin1Char('.')))
        return false;
    if (!token.at(0).isLetterOrNumber() || !token.at(token.size() - 1).isLetterOrNumber())
        return false;

    QChar previous;
    for (int i = 0; i < token.size(); ++i) {
        const QChar ch = token.at(i);
        if (ch == QLatin1Char('.')) {
            if (previous == QLatin1Char('.'))
                return false;
        } else if (!(ch.isLetterOrNumber() || ch == QLatin1Char('-')
                     || ch == QLatin1Char('_'))) {
            return false;
        }
        previous = ch;
    }
    return true;
}

int resourceTypeForOption(const QString &name)
{
    using Info = QWebEngineUrlRequestInfo;
    if (name == QLatin1String("script"))
        return Info::ResourceTypeScript;
    if (name == QLatin1String("image"))
        return Info::ResourceTypeImage;
    if (name == QLatin1String("stylesheet") || name == QLatin1String("css"))
        return Info::ResourceTypeStylesheet;
    if (name == QLatin1String("object"))
        return Info::ResourceTypeObject;
    if (name == QLatin1String("xmlhttprequest") || name == QLatin1String("xhr"))
        return Info::ResourceTypeXhr;
    if (name == QLatin1String("subdocument") || name == QLatin1String("frame"))
        return Info::ResourceTypeSubFrame;
    if (name == QLatin1String("document") || name == QLatin1String("doc"))
        return Info::ResourceTypeMainFrame;
    if (name == QLatin1String("media"))
        return Info::ResourceTypeMedia;
    if (name == QLatin1String("font"))
        return Info::ResourceTypeFontResource;
    if (name == QLatin1String("ping"))
        return Info::ResourceTypePing;
    if (name == QLatin1String("other"))
        return Info::ResourceTypeUnknown;
    return -1;
}

struct RuleOptions {
    bool valid = true;
    int party = kAnyParty;
    quint32 types = 0;
    quint32 excludedTypes = 0;
};

RuleOptions parseRuleOptions(const QString &text)
{
    RuleOptions out;
    const QString cleaned = text.trimmed();
    if (cleaned.isEmpty())
        return out;

    const QStringList parts = cleaned.split(QLatin1Char(','), Qt::SkipEmptyParts);
    for (const QString &rawPart : parts) {
        QString name = rawPart.trimmed().toLower();
        if (name.isEmpty())
            continue;
        const bool negated = name.startsWith(QLatin1Char('~'));
        if (negated)
            name = name.mid(1);

        // Rules scoped with domain= are skipped: ignoring the scope would
        // block requests on unrelated sites.
        if (name.startsWith(QLatin1String("domain="))) {
            out.valid = false;
            return out;
        }
        if (name == QLatin1String("third-party") || name == QLatin1String("3p")) {
            out.party = negated ? kFirstParty : kThirdParty;
            continue;
        }
        if (name == QLatin1String("first-party") || name == QLatin1String("1p")) {
            out.party = negated ? kThirdParty : kFirstParty;
            continue;
        }
        if (name == QLatin1String("important") || name == QLatin1String("all"))
            continue;

        const int type = resourceTypeForOption(name);
        if (type >= 0) {
            const quint32 bit = quint32(1) << quint32(type);
            if (negated)
                out.excludedTypes |= bit;
            else
                out.types |= bit;
            continue;
        }
        if (!negated) {
            // Unknown positive option (rewrite, csp, popup, ...): be conservative.
            out.valid = false;
            return out;
        }
    }
    return out;
}

}  // namespace

NetzacAdBlocker::NetzacAdBlocker(QObject *parent)
    : QWebEngineUrlRequestInterceptor(parent)
{
}

NetzacAdBlocker *NetzacAdBlocker::instance()
{
    static NetzacAdBlocker *s_instance = nullptr;
    if (!s_instance)
        s_instance = new NetzacAdBlocker(QCoreApplication::instance());
    return s_instance;
}

QString NetzacAdBlocker::cacheDirectory()
{
    QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation);
    if (cacheDir.isEmpty())
        cacheDir = QDir::homePath() + QStringLiteral("/.cache");
    return cacheDir + QStringLiteral("/zacos9/netzac/adblock");
}

QString NetzacAdBlocker::cacheFileForUrl(const QString &url)
{
    const QByteArray digest = QCryptographicHash::hash(
        url.trimmed().toUtf8(), QCryptographicHash::Sha1).toHex();
    return cacheDirectory() + QLatin1Char('/') + QString::fromLatin1(digest)
        + QStringLiteral(".txt");
}

void NetzacAdBlocker::install(QWebEngineProfile *profile)
{
    if (!profile || m_profile == profile)
        return;
    m_profile = profile;
    profile->setUrlRequestInterceptor(this);
}

void NetzacAdBlocker::configure(bool enabled, const QStringList &subscriptionUrls)
{
    QStringList cleaned;
    for (const QString &raw : subscriptionUrls) {
        const QString url = raw.trimmed();
        if (!url.isEmpty() && !cleaned.contains(url))
            cleaned.append(url);
    }

    const bool listsChanged = (cleaned != m_subscriptionUrls);
    {
        QMutexLocker locker(&m_mutex);
        m_enabled = enabled;
    }
    m_subscriptionUrls = cleaned;

    if (enabled || m_snapshot) {
        if (listsChanged || !m_snapshot)
            loadRulesFromCache();
    }

    emit stateChanged();

    if (enabled)
        scheduleUpdateCheck();
}

bool NetzacAdBlocker::isEnabled() const
{
    QMutexLocker locker(&m_mutex);
    return m_enabled;
}

int NetzacAdBlocker::ruleCount() const
{
    QMutexLocker locker(&m_mutex);
    return m_snapshot ? m_snapshot->count : 0;
}

void NetzacAdBlocker::scheduleUpdateCheck()
{
    if (m_autoCheckScheduled || !isEnabled())
        return;
    m_autoCheckScheduled = true;
    QTimer::singleShot(2500, this, [this]() {
        m_autoCheckScheduled = false;
        if (isEnabled())
            refreshLists(false);
    });
}

void NetzacAdBlocker::refreshLists(bool force)
{
    if (m_updating)
        return;

    QDir().mkpath(cacheDirectory());

    m_downloadQueue.clear();
    m_failedLists.clear();
    const QDateTime cutoff = QDateTime::currentDateTime().addDays(-1);
    for (const QString &url : m_subscriptionUrls) {
        const QFileInfo fi(cacheFileForUrl(url));
        if (force || !fi.exists() || fi.lastModified() < cutoff)
            m_downloadQueue.append(url);
    }

    if (m_downloadQueue.isEmpty()) {
        m_lastError.clear();
        loadRulesFromCache();
        return;
    }

    m_lastError.clear();
    m_updating = true;
    emit stateChanged();
    startNextDownload();
}

void NetzacAdBlocker::startNextDownload()
{
    if (m_downloadQueue.isEmpty()) {
        m_updating = false;
        m_lastError = m_failedLists.isEmpty() ? QString() : m_failedLists.first();
        loadRulesFromCache();
        return;
    }

    if (!m_network)
        m_network = new QNetworkAccessManager(this);

    const QString url = m_downloadQueue.takeFirst();
    QNetworkRequest request{QUrl(url)};
    request.setAttribute(
        QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Netzac Navigator/1.0"));
    request.setTransferTimeout(30000);

    QNetworkReply *reply = m_network->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, url]() {
        if (reply->error() == QNetworkReply::NoError) {
            const QByteArray data = reply->readAll();
            if (data.isEmpty()) {
                m_failedLists.append(url);
            } else {
                QFile out(cacheFileForUrl(url));
                if (out.open(QIODevice::WriteOnly | QIODevice::Truncate))
                    out.write(data);
                else
                    m_failedLists.append(url);
            }
        } else {
            m_failedLists.append(url);
        }
        reply->deleteLater();
        startNextDownload();
    });
}

bool NetzacAdBlocker::addRule(RuleSet &set, const QString &host, const Rule &rule)
{
    QVector<Rule> &rules = set.hosts[host];
    for (const Rule &existing : rules) {
        if (existing.path == rule.path && existing.party == rule.party
            && existing.types == rule.types
            && existing.excludedTypes == rule.excludedTypes) {
            return false;
        }
    }
    rules.append(rule);
    return true;
}

void NetzacAdBlocker::parseListFile(const QString &path, RuleSnapshot *snapshot)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return;

    while (!file.atEnd()) {
        const QString line = QString::fromUtf8(file.readLine()).trimmed();
        if (line.isEmpty())
            continue;

        const QChar first = line.at(0);
        if (first == QLatin1Char('!') || first == QLatin1Char('#') || first == QLatin1Char('['))
            continue;

        const QStringList parts = line.simplified().split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if (parts.size() >= 2
            && (parts.at(0) == QLatin1String("0.0.0.0")
                || parts.at(0) == QLatin1String("127.0.0.1")
                || parts.at(0) == QLatin1String("::1")
                || parts.at(0) == QLatin1String("::"))) {
            for (int i = 1; i < parts.size(); ++i) {
                QString domain = parts.at(i).toLower();
                const int hash = domain.indexOf(QLatin1Char('#'));
                if (hash >= 0)
                    domain = domain.left(hash);
                if (!isBlockableHostsDomain(domain))
                    continue;
                if (addRule(snapshot->block, domain, Rule()))
                    ++snapshot->count;
            }
            continue;
        }
        if (parts.size() == 1 && looksLikeDomainToken(parts.at(0))) {
            const QString domain = parts.at(0).toLower();
            if (addRule(snapshot->block, domain, Rule()))
                ++snapshot->count;
            continue;
        }

        QString text = line;
        bool isException = false;
        if (text.startsWith(QLatin1String("@@"))) {
            isException = true;
            text = text.mid(2);
        }

        QString optionsText;
        const int dollar = text.indexOf(QLatin1Char('$'));
        if (dollar >= 0) {
            optionsText = text.mid(dollar + 1);
            text = text.left(dollar);
        }
        const RuleOptions options = parseRuleOptions(optionsText);
        if (!options.valid)
            continue;

        while (text.endsWith(QLatin1Char('^')) || text.endsWith(QLatin1Char('|')))
            text.chop(1);

        QString host;
        QString rulePath;
        if (text.startsWith(QLatin1String("||"))) {
            const QString body = text.mid(2);
            if (body.isEmpty() || body.contains(QLatin1Char('*'))
                || body.contains(QLatin1Char('^'))) {
                continue;
            }
            const int slash = body.indexOf(QLatin1Char('/'));
            if (slash >= 0) {
                host = body.left(slash);
                rulePath = body.mid(slash);
            } else {
                host = body;
            }
        } else if (text.startsWith(QLatin1Char('|'))) {
            const QString body = text.mid(1);
            if (body.contains(QLatin1Char('*')))
                continue;
            const QUrl url(body);
            if (!url.isValid() || url.host().isEmpty())
                continue;
            host = url.host();
            rulePath = url.path(QUrl::FullyEncoded);
        } else {
            // Plain substring and regular-expression rules are not supported.
            continue;
        }

        host = host.trimmed().toLower();
        if (host.isEmpty() || host.startsWith(QLatin1Char('.'))
            || host.contains(QLatin1Char('?')) || host.contains(QLatin1Char('#'))
            || host.contains(QLatin1Char(':'))) {
            continue;
        }
        if (rulePath.contains(QLatin1Char('*')))
            continue;

        Rule rule;
        rule.path = rulePath;
        rule.party = options.party;
        rule.types = options.types;
        rule.excludedTypes = options.excludedTypes;

        if (isException) {
            addRule(snapshot->allow, host, rule);
        } else if (addRule(snapshot->block, host, rule)) {
            ++snapshot->count;
        }
    }
}

void NetzacAdBlocker::loadRulesFromCache()
{
    auto snapshot = std::make_shared<RuleSnapshot>();
    QDateTime newest;
    for (const QString &url : m_subscriptionUrls) {
        const QString path = cacheFileForUrl(url);
        const QFileInfo fi(path);
        if (!fi.exists())
            continue;
        parseListFile(path, snapshot.get());
        if (!newest.isValid() || fi.lastModified() > newest)
            newest = fi.lastModified();
    }

    {
        QMutexLocker locker(&m_mutex);
        m_snapshot = snapshot;
    }
    m_lastUpdate = newest;
    emit stateChanged();
}

bool NetzacAdBlocker::ruleMatches(const Rule &rule, const QString &target, int party, int type)
{
    if (rule.party != kAnyParty && rule.party != party)
        return false;

    const int bitIndex = (type >= 0 && type < 32) ? type : 31;
    const quint32 bit = quint32(1) << quint32(bitIndex);
    if (rule.types != 0 && (rule.types & bit) == 0)
        return false;
    if (rule.excludedTypes & bit)
        return false;
    if (!rule.path.isEmpty() && !target.startsWith(rule.path))
        return false;
    return true;
}

bool NetzacAdBlocker::ruleSetMatches(
    const RuleSet &set,
    const QString &host,
    const QString &target,
    int party,
    int type)
{
    QString candidate = host;
    while (!candidate.isEmpty()) {
        const auto it = set.hosts.constFind(candidate);
        if (it != set.hosts.constEnd()) {
            for (const Rule &rule : it.value()) {
                if (ruleMatches(rule, target, party, type))
                    return true;
            }
        }
        const int dot = candidate.indexOf(QLatin1Char('.'));
        if (dot < 0)
            break;
        candidate = candidate.mid(dot + 1);
    }
    return false;
}

void NetzacAdBlocker::interceptRequest(QWebEngineUrlRequestInfo &info)
{
    std::shared_ptr<const RuleSnapshot> snapshot;
    {
        QMutexLocker locker(&m_mutex);
        if (!m_enabled || !m_snapshot)
            return;
        snapshot = m_snapshot;
    }

    const QUrl url = info.requestUrl();
    const QString scheme = url.scheme().toLower();
    if (scheme != QLatin1String("http") && scheme != QLatin1String("https")
        && scheme != QLatin1String("ws") && scheme != QLatin1String("wss")) {
        return;
    }

    const QString host = url.host().toLower();
    if (host.isEmpty())
        return;

    const QString firstPartyHost = info.firstPartyUrl().host().toLower();
    const int party = isSameSite(host, firstPartyHost) ? kFirstParty : kThirdParty;
    const int type = int(info.resourceType());

    QString target = url.path(QUrl::FullyEncoded);
    if (url.hasQuery()) {
        target += QLatin1Char('?');
        target += url.query(QUrl::FullyEncoded);
    }

    if (ruleSetMatches(snapshot->allow, host, target, party, type))
        return;
    if (ruleSetMatches(snapshot->block, host, target, party, type))
        info.block(true);
}
