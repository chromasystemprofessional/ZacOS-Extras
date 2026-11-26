#pragma once

#include <QMetaType>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QWebEnginePage>

class NetzacWebPage;

enum class CalmJsDetection {
    IgnoreScripts = 0,
    SkipWebApps = 1,
    SkipAnyScript = 2,
};

enum class CalmLayoutMode {
    CleanPage = 0,
    ReaderFocus = 1,
};

struct CalmConfig {
    CalmJsDetection jsDetection = CalmJsDetection::SkipWebApps;
    CalmLayoutMode layoutMode = CalmLayoutMode::CleanPage;
    bool removeScripts = true;
    bool removeIframes = true;
    bool removeVideos = true;
    bool removeAds = true;
    bool normalizeTables = true;
    int maxImageWidth = 720;
    QStringList excludedHosts;

    CalmConfig();

    bool operator==(const CalmConfig &other) const
    {
        return jsDetection == other.jsDetection
            && layoutMode == other.layoutMode
            && removeScripts == other.removeScripts
            && removeIframes == other.removeIframes
            && removeVideos == other.removeVideos
            && removeAds == other.removeAds
            && normalizeTables == other.normalizeTables
            && maxImageWidth == other.maxImageWidth
            && excludedHosts == other.excludedHosts;
    }

    bool operator!=(const CalmConfig &other) const
    {
        return !(*this == other);
    }
};

class CalmPolicy {
public:
    CalmPolicy();
    explicit CalmPolicy(const QStringList &excludedHosts);

    static QStringList defaultExcludedHosts();
    static QString normalizeHost(const QString &host);
    static bool hostMatchesDomain(const QString &normalizedHost, const QString &normalizedDomain);

    void setExcludedHosts(const QStringList &hosts);
    void addExcludedHost(const QString &host);
    QStringList excludedHosts() const { return m_excludedHosts; }

    bool isExcludedUrl(const QUrl &url) const;

private:
    QStringList m_excludedHosts;
};

class CalmDomInjector {
public:
    static constexpr const char *kStyleElementId = "netzac-calm-style";
    static constexpr const char *kDocumentMarkerAttr = "netzacCalm";

    static QString buildCalmCss(const CalmConfig &config);
    static QString buildApplyScript(const CalmConfig &config);
    static QString buildRevertScript();
};

class CalmModeController : public QObject {
    Q_OBJECT

public:
    enum class Status {
        Disabled,
        ExcludedHost,
        SkippedBuiltIn,
        SkippedJavaScript,
        SkippedWebApp,
        Applied,
        AlreadyApplied,
        Error,
    };
    Q_ENUM(Status)

    struct EvaluationReport {
        Status status = Status::Disabled;
        QUrl url;
        int removedNodes = 0;
        int cappedImages = 0;
        int normalizedTables = 0;
        bool usedReaderFocus = false;
    };

    explicit CalmModeController(
        NetzacWebPage *page,
        const CalmConfig &config = CalmConfig(),
        QObject *parent = nullptr);

    bool isEnabled() const { return m_enabled; }
    bool isAppliedOnCurrentPage() const { return m_appliedOnCurrentPage; }
    const CalmConfig &config() const { return m_config; }
    int maxImageWidth() const { return m_config.maxImageWidth; }

    void setConfig(const CalmConfig &config);
    void setMaxImageWidth(int maxWidth);
    const CalmPolicy &policy() const { return m_policy; }
    CalmPolicy &policy() { return m_policy; }

public slots:
    void setEnabled(bool enabled);
    void toggle();
    void evaluateCurrentPage();

signals:
    void enabledChanged(bool enabled);
    void navigationIntercepted(const QUrl &url, QWebEnginePage::NavigationType type);
    void evaluationFinished(const CalmModeController::EvaluationReport &report);

private slots:
    void onMainFrameNavigationRequested(const QUrl &url, QWebEnginePage::NavigationType type);
    void onLoadFinished(bool ok);

private:
    NetzacWebPage *m_page = nullptr;
    CalmConfig m_config;
    CalmPolicy m_policy;
    bool m_enabled = false;
    bool m_appliedOnCurrentPage = false;
    bool m_pendingLoadEvaluation = false;
    quint64 m_evalGeneration = 0;
};

Q_DECLARE_METATYPE(CalmModeController::Status)
Q_DECLARE_METATYPE(CalmModeController::EvaluationReport)
