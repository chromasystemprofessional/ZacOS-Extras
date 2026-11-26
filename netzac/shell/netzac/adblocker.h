#pragma once

#include <memory>

#include <QDateTime>
#include <QHash>
#include <QMutex>
#include <QObject>
#include <QStringList>
#include <QVector>
#include <QWebEngineUrlRequestInterceptor>

class QNetworkAccessManager;
class QNetworkReply;
class QWebEngineProfile;

// Netzac Ad Blocker: downloads EasyList-style filter lists (plus hosts-style
// malware lists), caches them locally and blocks matching requests through a
// QWebEngineUrlRequestInterceptor. Supported rules are host/anchor network
// rules with the usual party/resource-type options; cosmetic (element hiding)
// rules are ignored.
class NetzacAdBlocker : public QWebEngineUrlRequestInterceptor {
    Q_OBJECT

public:
    static NetzacAdBlocker *instance();

    static QString cacheDirectory();

    void install(QWebEngineProfile *profile);
    void configure(bool enabled, const QStringList &subscriptionUrls);

    bool isEnabled() const;
    bool isUpdating() const { return m_updating; }
    int ruleCount() const;
    QDateTime lastUpdate() const { return m_lastUpdate; }
    QString lastError() const { return m_lastError; }

    void refreshLists(bool force = false);

signals:
    void stateChanged();

protected:
    void interceptRequest(QWebEngineUrlRequestInfo &info) override;

private:
    explicit NetzacAdBlocker(QObject *parent = nullptr);

    struct Rule {
        QString path;
        int party = 0;  // 0 = any, 1 = third-party only, 2 = first-party only
        quint32 types = 0;
        quint32 excludedTypes = 0;
    };

    struct RuleSet {
        QHash<QString, QVector<Rule>> hosts;
    };

    struct RuleSnapshot {
        RuleSet block;
        RuleSet allow;
        int count = 0;
    };

    static QString cacheFileForUrl(const QString &url);
    static bool addRule(RuleSet &set, const QString &host, const Rule &rule);
    static void parseListFile(const QString &path, RuleSnapshot *snapshot);
    static bool ruleMatches(const Rule &rule, const QString &target, int party, int type);
    static bool ruleSetMatches(
        const RuleSet &set,
        const QString &host,
        const QString &target,
        int party,
        int type);

    void loadRulesFromCache();
    void scheduleUpdateCheck();
    void startNextDownload();

    mutable QMutex m_mutex;
    std::shared_ptr<const RuleSnapshot> m_snapshot;
    bool m_enabled = false;

    QStringList m_subscriptionUrls;
    QStringList m_downloadQueue;
    QStringList m_failedLists;
    QString m_lastError;
    QDateTime m_lastUpdate;
    bool m_updating = false;
    bool m_autoCheckScheduled = false;
    QNetworkAccessManager *m_network = nullptr;
    QWebEngineProfile *m_profile = nullptr;
};
