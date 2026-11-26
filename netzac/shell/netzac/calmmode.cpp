#include "calmmode.h"

#include "webview.h"

#include <QPointer>
#include <QVariant>
#include <QVariantMap>
#include <QWebEngineScript>

namespace {

int clampMaxImageWidth(int maxWidth)
{
    return qBound(240, maxWidth, 1920);
}

QString jsQuote(const QString &input)
{
    QString out;
    out.reserve(input.size() + 32);
    out.append(QLatin1Char('"'));
    for (const QChar ch : input) {
        switch (ch.unicode()) {
        case u'\\':
            out.append(QStringLiteral("\\\\"));
            break;
        case u'"':
            out.append(QStringLiteral("\\\""));
            break;
        case u'\n':
            out.append(QStringLiteral("\\n"));
            break;
        case u'\r':
            out.append(QStringLiteral("\\r"));
            break;
        case u'\t':
            out.append(QStringLiteral("\\t"));
            break;
        case 0x2028:
            out.append(QStringLiteral("\\u2028"));
            break;
        case 0x2029:
            out.append(QStringLiteral("\\u2029"));
            break;
        default:
            out.append(ch);
            break;
        }
    }
    out.append(QLatin1Char('"'));
    return out;
}

QString jsBool(bool value)
{
    return value ? QStringLiteral("true") : QStringLiteral("false");
}

}  // namespace

CalmConfig::CalmConfig()
    : excludedHosts(CalmPolicy::defaultExcludedHosts())
{
}

CalmPolicy::CalmPolicy()
    : m_excludedHosts(defaultExcludedHosts())
{
}

CalmPolicy::CalmPolicy(const QStringList &excludedHosts)
{
    setExcludedHosts(excludedHosts);
}

QStringList CalmPolicy::defaultExcludedHosts()
{
    return {QStringLiteral("github.com")};
}

QString CalmPolicy::normalizeHost(const QString &host)
{
    QString norm = host.trimmed().toLower();
    while (norm.endsWith(QLatin1Char('.')))
        norm.chop(1);
    return norm;
}

bool CalmPolicy::hostMatchesDomain(const QString &normalizedHost, const QString &normalizedDomain)
{
    if (normalizedHost.isEmpty() || normalizedDomain.isEmpty())
        return false;
    return normalizedHost == normalizedDomain
        || normalizedHost.endsWith(QLatin1Char('.') + normalizedDomain);
}

void CalmPolicy::setExcludedHosts(const QStringList &hosts)
{
    m_excludedHosts.clear();
    for (const QString &h : hosts)
        addExcludedHost(h);
}

void CalmPolicy::addExcludedHost(const QString &host)
{
    const QString norm = normalizeHost(host);
    if (!norm.isEmpty() && !m_excludedHosts.contains(norm))
        m_excludedHosts.append(norm);
}

bool CalmPolicy::isExcludedUrl(const QUrl &url) const
{
    if (!url.isValid())
        return true;

    const QString scheme = url.scheme().toLower();
    if (scheme != QLatin1String("http")
        && scheme != QLatin1String("https")
        && scheme != QLatin1String("file")) {
        return true;
    }

    if (scheme == QLatin1String("file"))
        return false;

    const QString host = normalizeHost(url.host());
    if (host.isEmpty())
        return true;

    for (const QString &domain : m_excludedHosts) {
        if (hostMatchesDomain(host, domain))
            return true;
    }
    return false;
}

QString CalmDomInjector::buildCalmCss(const CalmConfig &config)
{
    const int capped = clampMaxImageWidth(config.maxImageWidth);
    QString css = QString::fromUtf8(R"CALM_CSS(
html {
  background-color: #f6f4ee !important;
  overflow-x: hidden !important;
  overflow-y: auto !important;
}
body {
  background-color: #f6f4ee !important;
  color: #1f2328 !important;
  font-family: Georgia, 'Liberation Serif', 'Nimbus Roman', serif !important;
  font-size: 17px !important;
  line-height: 1.65 !important;
  max-width: 800px !important;
  width: auto !important;
  min-width: 0 !important;
  height: auto !important;
  margin: 0 auto !important;
  padding: 24px 28px 40px 28px !important;
  box-sizing: border-box !important;
  overflow-x: hidden !important;
  overflow-y: visible !important;
  float: none !important;
  position: static !important;
}
body * {
  max-width: 100% !important;
}
main, article, section, aside, nav, header, footer, div, figure {
  position: static !important;
  float: none !important;
  transform: none !important;
  top: auto !important;
  right: auto !important;
  bottom: auto !important;
  left: auto !important;
  z-index: auto !important;
  height: auto !important;
  min-height: 0 !important;
  max-height: none !important;
  overflow: visible !important;
  box-sizing: border-box !important;
}
main, article, section, aside, header, footer, div {
  display: block !important;
  width: auto !important;
  margin-left: 0 !important;
  margin-right: 0 !important;
  margin-top: 0 !important;
}
main, article, section, aside, nav, header, footer, div, p, ul, ol, li, dl, dt, dd, span, figure, figcaption {
  background-color: transparent !important;
  background-image: none !important;
  color: inherit !important;
}
main, article, section {
  max-width: 100% !important;
}
h1, h2, h3, h4, h5, h6 {
  color: #16181d !important;
  line-height: 1.3 !important;
  margin-top: 1.35em !important;
  margin-bottom: 0.45em !important;
}
p, li, blockquote, dd, dt {
  max-width: 72ch;
}
a {
  color: #1b4f9c !important;
  text-decoration: underline !important;
}
a:visited {
  color: #5b317d !important;
}
pre, code, kbd, samp {
  font-family: 'Nimbus Mono PS', 'Liberation Mono', monospace !important;
  background-color: #edeae0 !important;
  color: #1f2328 !important;
  border-radius: 2px;
}
pre {
  padding: 10px 12px !important;
  overflow-x: auto !important;
  max-width: 100% !important;
  border: 1px solid #d5d0c4 !important;
  white-space: pre !important;
}
blockquote {
  margin: 1em 0 !important;
  padding: 4px 14px !important;
  border-left: 3px solid #9d9689 !important;
  background-color: #efece3 !important;
  color: #1f2328 !important;
}
hr {
  border: 0 !important;
  border-top: 1px solid #c9c3b8 !important;
  margin: 1.5em 0 !important;
}
img, picture, svg {
  position: static !important;
  float: none !important;
  transform: none !important;
  max-width: min(100%, __MAX_IMG_WIDTH__px) !important;
  height: auto !important;
  box-sizing: border-box !important;
  vertical-align: middle;
}
figure {
  max-width: 100% !important;
  margin: 1em 0 !important;
}
figcaption, .netzac-calm-reader-meta {
  font-size: 0.9em !important;
  color: #4b525c !important;
  margin-top: 0.35em !important;
}
.netzac-calm-reader-header h1 {
  margin-top: 0 !important;
}
)CALM_CSS");

    if (config.removeIframes) {
        css.append(QStringLiteral("iframe { display: none !important; }\n"));
    } else {
        css.append(QStringLiteral("iframe { max-width: 100% !important; }\n"));
    }

    if (config.removeVideos) {
        css.append(QStringLiteral("video { display: none !important; }\n"));
    } else {
        css.append(QStringLiteral(
            "video { max-width: min(100%, __MAX_IMG_WIDTH__px) !important; height: auto !important; }\n"));
    }

    if (config.removeAds) {
        css.append(QStringLiteral(
            "[data-ad], [data-ad-slot], [data-ad-client], ins.adsbygoogle, "
            "[aria-modal=\"true\"], [role=\"dialog\"], [role=\"alertdialog\"] { display: none !important; }\n"));
    }

    if (config.normalizeTables) {
        css.append(QString::fromUtf8(R"CALM_TBL(
table {
  width: auto !important;
  max-width: 100% !important;
  border-collapse: collapse !important;
  border-spacing: 0 !important;
  margin: 1em 0 !important;
  font-size: 0.95em !important;
  line-height: 1.45 !important;
}
table th, table td {
  border: 1px solid #c5c0b5 !important;
  padding: 6px 10px !important;
  text-align: left !important;
  vertical-align: top !important;
  overflow-wrap: break-word !important;
}
table th {
  background-color: #eae6dc !important;
  color: #16181d !important;
  font-weight: 600 !important;
}
table caption {
  text-align: left !important;
  font-weight: 600 !important;
  margin-bottom: 0.4em !important;
}
.netzac-calm-table-wrap {
  display: block !important;
  width: 100% !important;
  max-width: 100% !important;
  overflow-x: auto !important;
  margin: 1em 0 !important;
}
.netzac-calm-table-wrap > table {
  margin: 0 !important;
}
)CALM_TBL"));
    }

    css.replace(QStringLiteral("__MAX_IMG_WIDTH__"), QString::number(capped));
    return css;
}

QString CalmDomInjector::buildApplyScript(const CalmConfig &config)
{
    const int capped = clampMaxImageWidth(config.maxImageWidth);
    const QString quotedCss = jsQuote(buildCalmCss(config));

    QString script = QString::fromUtf8(R"CALM_JS(
(() => {
  const STYLE_ID = "netzac-calm-style";
  const JS_DETECTION = __JS_DETECTION__;
  const LAYOUT_MODE = __LAYOUT_MODE__;
  const REMOVE_SCRIPTS = __REMOVE_SCRIPTS__;
  const REMOVE_IFRAMES = __REMOVE_IFRAMES__;
  const REMOVE_VIDEOS = __REMOVE_VIDEOS__;
  const REMOVE_ADS = __REMOVE_ADS__;
  const NORMALIZE_TABLES = __NORMALIZE_TABLES__;
  const MAX_IMG_WIDTH = __MAX_IMG_WIDTH__;
  const CALM_CSS = __CALM_CSS__;

  if (!document.documentElement || !document.body) {
    return { status: "error", removed: 0, imagesCapped: 0, tablesNormalized: 0, readerFocus: false };
  }

  // Idempotency guard: never apply Calm formatting twice to the same document.
  if (document.documentElement.dataset.netzacCalm === "1" && document.getElementById(STYLE_ID)) {
    return { status: "already-applied", removed: 0, imagesCapped: 0, tablesNormalized: 0, readerFocus: false };
  }

  // Configurable JavaScript / interactive web app detection BEFORE any DOM or style mutations.
  const DATA_SCRIPT_TYPES = new Set([
    "application/json",
    "application/ld+json",
    "application/geo+json",
    "text/template",
    "text/html",
    "text/plain"
  ]);

  const hasExecutableScripts = () => {
    const scripts = document.querySelectorAll("script");
    for (let i = 0; i < scripts.length; ++i) {
      const s = scripts[i];
      const src = s.getAttribute("src");
      if (src !== null && src.trim().length > 0) {
        return true;
      }
      const rawType = (s.getAttribute("type") || "").trim().toLowerCase().split(";")[0].trim();
      if (rawType && DATA_SCRIPT_TYPES.has(rawType)) {
        continue;
      }
      if ((s.textContent || "").trim().length > 0) {
        return true;
      }
    }
    return false;
  };

  const isInteractiveWebApp = () => {
    const bodyTextLen = (document.body.innerText || "").replace(/\s+/g, " ").trim().length;
    let pTextLen = 0;
    const paragraphs = document.querySelectorAll("p");
    for (let i = 0; i < paragraphs.length; ++i) {
      const txt = (paragraphs[i].textContent || "").trim();
      if (txt.length >= 20) {
        pTextLen += txt.length;
      }
    }

    let hasLargeCanvas = false;
    const minCanvasArea = Math.max(160000, window.innerWidth * window.innerHeight * 0.35);
    const canvases = document.querySelectorAll("canvas");
    for (let i = 0; i < canvases.length; ++i) {
      const r = canvases[i].getBoundingClientRect();
      if (r.width * r.height >= minCanvasArea) {
        hasLargeCanvas = true;
        break;
      }
    }

    const hasAppRole = document.body.getAttribute("role") === "application"
      || document.querySelector('[role="application"], [contenteditable="true"]') !== null;

    if (bodyTextLen < 60 && pTextLen === 0) {
      return true;
    }
    if (hasLargeCanvas && pTextLen < 120) {
      return true;
    }
    if (hasAppRole && pTextLen < 150 && bodyTextLen < 350) {
      return true;
    }
    return false;
  };

  if (JS_DETECTION === 2 && hasExecutableScripts()) {
    return { status: "skipped-js", removed: 0, imagesCapped: 0, tablesNormalized: 0, readerFocus: false };
  }
  if (JS_DETECTION === 1 && hasExecutableScripts() && isInteractiveWebApp()) {
    return { status: "skipped-webapp", removed: 0, imagesCapped: 0, tablesNormalized: 0, readerFocus: false };
  }

  // Rescue lazy-loaded images before stripping scripts or restructuring DOM.
  const lazyAttrs = ["data-src", "data-lazy-src", "data-original", "data-src-medium", "data-url"];
  for (let i = 0; i < document.images.length; ++i) {
    const img = document.images[i];
    const src = (img.getAttribute("src") || "").trim();
    const needsRescue = !src || /^(?:data:image\/(?:gif|svg\+xml)|about:blank)/i.test(src);
    if (needsRescue) {
      for (let j = 0; j < lazyAttrs.length; ++j) {
        const candidate = (img.getAttribute(lazyAttrs[j]) || "").trim();
        if (candidate && !/^data:image\/gif/i.test(candidate)) {
          img.setAttribute("src", candidate);
          break;
        }
      }
    }
    const lazySrcset = (img.getAttribute("data-srcset") || "").trim();
    if (!img.getAttribute("srcset") && lazySrcset) {
      img.setAttribute("srcset", lazySrcset);
    }
  }

  let removedCount = 0;

  if (REMOVE_SCRIPTS) {
    const scriptNodes = document.querySelectorAll("script, link[rel='preload'][as='script'], link[rel='modulepreload']");
    for (let i = 0; i < scriptNodes.length; ++i) {
      scriptNodes[i].remove();
      ++removedCount;
    }
  }

  if (REMOVE_VIDEOS) {
    const videos = document.querySelectorAll("video");
    for (let i = 0; i < videos.length; ++i) {
      const v = videos[i];
      try {
        v.pause();
        v.muted = true;
      } catch (e) {}
      v.removeAttribute("src");
      v.removeAttribute("autoplay");
      const sources = v.querySelectorAll("source");
      for (let j = 0; j < sources.length; ++j) {
        sources[j].remove();
      }
      try {
        v.load();
      } catch (e) {}
      v.remove();
      ++removedCount;
    }
  }

  // Batched read phase over document.body (skipping doomed subtrees via explicit stack).
  if (REMOVE_IFRAMES || REMOVE_ADS) {
    const NON_VISUAL_TAGS = new Set([
      "STYLE", "LINK", "META", "TITLE", "NOSCRIPT", "TEMPLATE",
      "DATALIST", "SOURCE", "TRACK", "PARAM", "AREA", "BR", "WBR", "SCRIPT"
    ]);
    const SEMANTIC_PROTECTED_TAGS = new Set(["BODY", "MAIN", "ARTICLE", "SECTION"]);
    const DISTRACTION_RE = /(?:^|[\s_-])(?:ad|ads|advert|advertisement|adsense|adserver|doubleclick|dfp|taboola|outbrain|mgid|teads|publicidade|publi|anuncio|anuncios|patrocinado|patrocinio|propaganda|sponsored|sponsor|promo|promotion|gdpr|paywall|popup|modal|cookie(?:banner|notice|consent|bar|popup|modal|overlay|wall)|cookies?(?=$|[\s_-](?:banner|notice|consent|bar|popup|modal|overlay|wall|policy|box|msg|message))|consent(?:[-_]?(?:banner|modal|popup|bar))?|newsletter(?:[-_]?(?:signup|box|banner|modal))?|glb[-_]?ad|cn[-_]?ad|floating[-_]?player|sticky[-_]?(?:banner|ad|footer|bar)|ad[-_]?(?:slot|unit|box|banner|container|wrapper|feedback)|banner[-_]?ad|sponsored[-_]?(?:post|content|block|link))(?=$|[\s_-])/i;

    const isHiddenElement = (el) => {
      if (el.hasAttribute("hidden") || el.getAttribute("aria-hidden") === "true") {
        return true;
      }
      if (el.style) {
        if (el.style.display === "none" || el.style.visibility === "hidden") {
          return true;
        }
      }
      const cs = window.getComputedStyle ? window.getComputedStyle(el) : null;
      if (cs && (cs.display === "none" || cs.visibility === "hidden")) {
        return true;
      }
      return false;
    };

    const isDistractionContainer = (el, tag) => {
      if (tag === "DIALOG") {
        return true;
      }
      if (tag === "INS" && typeof el.className === "string" && /\badsbygoogle\b/i.test(el.className)) {
        return true;
      }
      if (el.hasAttribute("data-ad") || el.hasAttribute("data-ad-slot")
          || el.hasAttribute("data-ad-client") || el.hasAttribute("data-sponsored")) {
        return true;
      }
      if (el.getAttribute("aria-modal") === "true") {
        return true;
      }
      const role = (el.getAttribute("role") || "").trim().toLowerCase();
      if (role === "dialog" || role === "alertdialog") {
        return true;
      }
      const testId = el.getAttribute("data-testid") || "";
      if (testId && DISTRACTION_RE.test(testId)) {
        return true;
      }
      const idStr = el.id || "";
      const clsStr = typeof el.className === "string" ? el.className : (el.getAttribute("class") || "");
      return (idStr && DISTRACTION_RE.test(idStr)) || (clsStr && DISTRACTION_RE.test(clsStr));
    };

    const toRemove = [];
    const stack = [];
    const bodyChildren = document.body.children;
    for (let i = bodyChildren.length - 1; i >= 0; --i) {
      stack.push(bodyChildren[i]);
    }

    while (stack.length > 0) {
      const el = stack.pop();
      const tag = el.tagName ? el.tagName.toUpperCase() : "";
      if (!tag || NON_VISUAL_TAGS.has(tag)) {
        continue;
      }

      if (REMOVE_IFRAMES && tag === "IFRAME") {
        toRemove.push(el);
        continue;
      }

      if (REMOVE_ADS) {
        const protectsFromHeuristics = SEMANTIC_PROTECTED_TAGS.has(tag)
          || (el.querySelector && el.querySelector("main, article") !== null);
        const protectsHidden = (tag === "MAIN" || tag === "ARTICLE" || tag === "BODY")
          || (el.querySelector && el.querySelector("main, article") !== null);

        if ((!protectsHidden && isHiddenElement(el))
            || (!protectsFromHeuristics && isDistractionContainer(el, tag))) {
          toRemove.push(el);
          continue;
        }
      }

      const children = el.children;
      for (let i = children.length - 1; i >= 0; --i) {
        stack.push(children[i]);
      }
    }

    for (let i = 0; i < toRemove.length; ++i) {
      toRemove[i].remove();
      ++removedCount;
    }
  }

  // Optional Firefox Reader-style main text extraction.
  let usedReaderFocus = false;
  if (LAYOUT_MODE === 1) {
    const POSITIVE_RE = /(?:^|[\s_-])(?:article|content|story|post|entry|materia|noticia|body[-_]?text|read|text)(?=$|[\s_-])/i;
    const NEGATIVE_RE = /(?:^|[\s_-])(?:nav|menu|sidebar|footer|header|comment|coment|related|relacionad|share|social|breadcrumb|trending|mais[-_]?lid|leia[-_]?tambem|veja[-_]?tambem|widget|author[-_]?bio)(?=$|[\s_-])/i;
    const CLUTTER_RE = /(?:^|[\s_-])(?:share|social|related|relacionad|comment|comentar|newsletter|subscribe|assine|breadcrumb|sidebar|trending|mais[-_]?lid|leia[-_]?tambem|veja[-_]?tambem|author[-_]?bio|tags|tag[-_]?list|outbrain|taboola)(?=$|[\s_-])/i;

    const getIdCls = (el) => {
      const idStr = el.id || "";
      const clsStr = typeof el.className === "string" ? el.className : (el.getAttribute("class") || "");
      return idStr + " " + clsStr;
    };

    const scoreCandidate = (el) => {
      const ps = el.querySelectorAll("p");
      if (ps.length === 0) return null;
      let pChars = 0;
      let pCount = 0;
      let commaCount = 0;
      for (let i = 0; i < ps.length; ++i) {
        const txt = (ps[i].textContent || "").replace(/\s+/g, " ").trim();
        if (txt.length >= 25) {
          pChars += txt.length;
          ++pCount;
          const punct = txt.match(/[,.;:!?]/g);
          commaCount += punct ? punct.length : 0;
        }
      }
      if (pChars < 200) return null;

      const totalChars = Math.max(1, (el.textContent || "").replace(/\s+/g, " ").trim().length);
      let linkChars = 0;
      const links = el.querySelectorAll("a");
      for (let i = 0; i < links.length; ++i) {
        linkChars += (links[i].textContent || "").trim().length;
      }
      const linkDensity = linkChars / totalChars;
      if (linkDensity >= 0.6) return null;

      const tag = el.tagName ? el.tagName.toUpperCase() : "";
      let score = pChars + commaCount * 10 + pCount * 15;
      if (tag === "ARTICLE") score += 120;
      else if (tag === "MAIN" || (el.getAttribute("role") || "").toLowerCase() === "main") score += 80;
      else if (tag === "SECTION") score += 20;

      const tokens = getIdCls(el);
      if (POSITIVE_RE.test(tokens)) score += 50;
      if (NEGATIVE_RE.test(tokens)) score -= 80;
      score *= (1.0 - linkDensity * 0.7);
      return { el, score, pChars, linkDensity };
    };

    const rawCandidates = document.querySelectorAll("article, main, [role='main'], section, div");
    let best = null;
    for (let i = 0; i < rawCandidates.length; ++i) {
      const stats = scoreCandidate(rawCandidates[i]);
      if (!stats) continue;
      if (!best || stats.score > best.score + 1
          || (Math.abs(stats.score - best.score) <= 1 && best.el.contains(stats.el))) {
        best = stats;
      }
    }

    if (best) {
      const innerArticles = best.el.querySelectorAll("article");
      for (let i = 0; i < innerArticles.length; ++i) {
        const sub = scoreCandidate(innerArticles[i]);
        if (sub && sub.pChars >= best.pChars * 0.65 && sub.linkDensity <= best.linkDensity + 0.05) {
          best = sub;
        }
      }

      const extractTitle = () => {
        const selectors = ["article h1", "main h1", "h1"];
        for (let i = 0; i < selectors.length; ++i) {
          const h = document.querySelector(selectors[i]);
          const t = h ? (h.textContent || "").replace(/\s+/g, " ").trim() : "";
          if (t.length >= 8) return t;
        }
        const og = document.querySelector('meta[property="og:title"]');
        const ogText = og ? (og.getAttribute("content") || "").trim() : "";
        if (ogText) return ogText;
        return (document.title || "").trim();
      };

      const extractMeta = () => {
        const parts = [];
        const authorMeta = document.querySelector('meta[name="author"]');
        const authorEl = document.querySelector('[rel="author"], .byline, .author');
        const author = (authorMeta ? (authorMeta.getAttribute("content") || "") : (authorEl ? authorEl.textContent : ""))
          .replace(/\s+/g, " ").trim();
        if (author && author.length <= 120) {
          parts.push(author);
        }
        const timeEl = document.querySelector("time[datetime], time");
        const timeStr = timeEl ? (timeEl.textContent || "").replace(/\s+/g, " ").trim() : "";
        if (timeStr && timeStr.length <= 60 && parts.indexOf(timeStr) === -1) {
          parts.push(timeStr);
        }
        return parts.join(" — ");
      };

      const clone = best.el.cloneNode(true);
      const structuralStripSelectors = ["nav", "aside", "footer", "form", "button", "input", "select", "textarea", "noscript"];
      if (REMOVE_SCRIPTS) structuralStripSelectors.push("script");
      if (REMOVE_IFRAMES) structuralStripSelectors.push("iframe");
      if (REMOVE_VIDEOS) structuralStripSelectors.push("video");
      const stripNodes = clone.querySelectorAll(structuralStripSelectors.join(", "));
      for (let i = 0; i < stripNodes.length; ++i) {
        stripNodes[i].remove();
        ++removedCount;
      }

      const clutterCandidates = clone.querySelectorAll("div, section, ul, header");
      for (let i = 0; i < clutterCandidates.length; ++i) {
        const c = clutterCandidates[i];
        if (!c.isConnected && !clone.contains(c)) continue;
        if (CLUTTER_RE.test(getIdCls(c))) {
          let cPChars = 0;
          const cps = c.querySelectorAll("p");
          for (let j = 0; j < cps.length; ++j) {
            cPChars += (cps[j].textContent || "").trim().length;
          }
          if (cPChars < 600) {
            c.remove();
            ++removedCount;
          }
        }
      }

      const styledNodes = clone.querySelectorAll("[style]");
      const stripProps = [
        "width", "min-width", "max-width", "height", "min-height",
        "position", "top", "left", "right", "bottom", "float",
        "background", "background-color", "background-image", "color",
        "margin", "padding", "font-size", "overflow"
      ];
      for (let i = 0; i < styledNodes.length; ++i) {
        const sn = styledNodes[i];
        if (sn.tagName === "IMG") continue;
        for (let j = 0; j < stripProps.length; ++j) {
          sn.style.removeProperty(stripProps[j]);
        }
      }

      const titleText = extractTitle();
      const metaText = extractMeta();
      const readerRoot = document.createElement("article");
      readerRoot.id = "netzac-calm-reader";

      const cloneH1 = clone.querySelector("h1");
      const cloneH1Text = cloneH1 ? (cloneH1.textContent || "").replace(/\s+/g, " ").trim() : "";
      const hasMatchingH1 = cloneH1Text && titleText
        && cloneH1Text.toLowerCase() === titleText.toLowerCase();

      if ((!hasMatchingH1 && titleText) || metaText) {
        const hdr = document.createElement("header");
        hdr.className = "netzac-calm-reader-header";
        if (!hasMatchingH1 && titleText) {
          const h1 = document.createElement("h1");
          h1.textContent = titleText;
          hdr.appendChild(h1);
        }
        if (metaText) {
          const metaDiv = document.createElement("div");
          metaDiv.className = "netzac-calm-reader-meta";
          metaDiv.textContent = metaText;
          hdr.appendChild(metaDiv);
        }
        hdr.appendChild(document.createElement("hr"));
        readerRoot.appendChild(hdr);
      }

      const contentDiv = document.createElement("div");
      contentDiv.className = "netzac-calm-reader-content";
      contentDiv.appendChild(clone);
      readerRoot.appendChild(contentDiv);

      document.body.replaceChildren(readerRoot);
      usedReaderFocus = true;
    }
  }

  // Neutralize negative margins in inline styles to prevent overlapping blocks.
  const inlineMarginEls = document.body.querySelectorAll("[style]");
  for (let i = 0; i < inlineMarginEls.length; ++i) {
    const el = inlineMarginEls[i];
    const st = el.style;
    if (!st) continue;
    for (const prop of ["margin-top", "margin-right", "margin-bottom", "margin-left"]) {
      const val = parseFloat(st.getPropertyValue(prop));
      if (Number.isFinite(val) && val < 0) {
        st.removeProperty(prop);
      }
    }
  }

  // Inject modular Calm CSS and mark document as processed.
  const styleEl = document.createElement("style");
  styleEl.id = STYLE_ID;
  styleEl.textContent = CALM_CSS;
  (document.head || document.documentElement).appendChild(styleEl);
  document.documentElement.dataset.netzacCalm = "1";

  // Enforce maxImageWidth cleanly while preserving proportional height.
  const parsePx = (raw) => {
    if (!raw) return 0;
    const trimmed = String(raw).trim();
    if (!trimmed || trimmed.endsWith("%") || trimmed.endsWith("vw")) return 0;
    const num = parseFloat(trimmed);
    return Number.isFinite(num) && num > 0 ? num : 0;
  };

  let imagesCapped = 0;
  const imgs = document.images;
  for (let i = 0; i < imgs.length; ++i) {
    const img = imgs[i];
    const rawAttrW = img.getAttribute("width");
    const rawAttrH = img.getAttribute("height");
    const attrW = parsePx(rawAttrW);
    const attrH = parsePx(rawAttrH);
    const rawStyleW = img.style ? img.style.width : "";
    const rawStyleH = img.style ? img.style.height : "";
    const inlineW = parsePx(rawStyleW);
    const inlineH = parsePx(rawStyleH);

    const hasPctWidth = (rawAttrW && rawAttrW.indexOf("%") !== -1)
      || (rawStyleW && (rawStyleW.indexOf("%") !== -1 || rawStyleW.indexOf("vw") !== -1));
    const hasExplicitDim = rawAttrW !== null || rawAttrH !== null
      || Boolean(rawStyleW) || Boolean(rawStyleH);

    const exceeds = attrW > MAX_IMG_WIDTH
      || inlineW > MAX_IMG_WIDTH
      || hasPctWidth
      || (img.naturalWidth > MAX_IMG_WIDTH && hasExplicitDim)
      || (!attrW && !inlineW && (attrH > MAX_IMG_WIDTH || inlineH > MAX_IMG_WIDTH));

    if (exceeds) {
      if (img.hasAttribute("width")) img.removeAttribute("width");
      if (img.hasAttribute("height")) img.removeAttribute("height");
      if (img.style) {
        img.style.removeProperty("width");
        img.style.removeProperty("height");
        img.style.removeProperty("min-width");
      }
      ++imagesCapped;
    }
  }

  // Organize tables cleanly when enabled.
  let tablesNormalized = 0;
  if (NORMALIZE_TABLES) {
    const tables = document.querySelectorAll("table");
    tablesNormalized = tables.length;
    for (let i = 0; i < tables.length; ++i) {
      const tbl = tables[i];
      tbl.removeAttribute("cellpadding");
      tbl.removeAttribute("cellspacing");
      tbl.removeAttribute("border");
      const tblAttrW = tbl.getAttribute("width");
      if (tblAttrW !== null) {
        const px = parsePx(tblAttrW);
        if (px > MAX_IMG_WIDTH || tblAttrW.indexOf("%") !== -1) {
          tbl.removeAttribute("width");
        }
      }
      if (tbl.style) {
        tbl.style.removeProperty("min-width");
        const sw = tbl.style.width;
        if (sw && (parsePx(sw) > MAX_IMG_WIDTH || sw.indexOf("%") !== -1)) {
          tbl.style.removeProperty("width");
        }
      }
    }

    const wrapTargets = [];
    for (let i = 0; i < tables.length; ++i) {
      const tbl = tables[i];
      if (tbl.closest(".netzac-calm-table-wrap")) continue;
      if (tbl.parentElement && tbl.parentElement.closest("table")) continue;
      const availW = tbl.parentElement ? tbl.parentElement.clientWidth : document.body.clientWidth;
      if (availW > 0 && tbl.scrollWidth > availW + 2) {
        wrapTargets.push(tbl);
      }
    }
    for (let i = 0; i < wrapTargets.length; ++i) {
      const tbl = wrapTargets[i];
      if (!tbl.parentNode) continue;
      const wrap = document.createElement("div");
      wrap.className = "netzac-calm-table-wrap";
      tbl.parentNode.insertBefore(wrap, tbl);
      wrap.appendChild(tbl);
    }
  }

  return {
    status: "applied",
    removed: removedCount,
    imagesCapped: imagesCapped,
    tablesNormalized: tablesNormalized,
    readerFocus: usedReaderFocus
  };
})()
)CALM_JS");

    script.replace(QStringLiteral("__JS_DETECTION__"), QString::number(int(config.jsDetection)));
    script.replace(QStringLiteral("__LAYOUT_MODE__"), QString::number(int(config.layoutMode)));
    script.replace(QStringLiteral("__REMOVE_SCRIPTS__"), jsBool(config.removeScripts));
    script.replace(QStringLiteral("__REMOVE_IFRAMES__"), jsBool(config.removeIframes));
    script.replace(QStringLiteral("__REMOVE_VIDEOS__"), jsBool(config.removeVideos));
    script.replace(QStringLiteral("__REMOVE_ADS__"), jsBool(config.removeAds));
    script.replace(QStringLiteral("__NORMALIZE_TABLES__"), jsBool(config.normalizeTables));
    script.replace(QStringLiteral("__MAX_IMG_WIDTH__"), QString::number(capped));
    script.replace(QStringLiteral("__CALM_CSS__"), quotedCss);
    return script;
}

QString CalmDomInjector::buildRevertScript()
{
    return QString::fromUtf8(R"CALM_JS(
(() => {
  let reverted = false;
  const styleEl = document.getElementById("netzac-calm-style");
  if (styleEl) {
    styleEl.remove();
    reverted = true;
  }
  if (document.documentElement && document.documentElement.dataset.netzacCalm) {
    delete document.documentElement.dataset.netzacCalm;
    reverted = true;
  }
  return { reverted: reverted };
})()
)CALM_JS");
}

CalmModeController::CalmModeController(
    NetzacWebPage *page,
    const CalmConfig &config,
    QObject *parent)
    : QObject(parent)
    , m_page(page)
    , m_config(config)
    , m_policy(config.excludedHosts)
{
    m_config.maxImageWidth = clampMaxImageWidth(m_config.maxImageWidth);
    qRegisterMetaType<CalmModeController::Status>("CalmModeController::Status");
    qRegisterMetaType<CalmModeController::EvaluationReport>("CalmModeController::EvaluationReport");

    if (m_page) {
        connect(
            m_page,
            &NetzacWebPage::mainFrameNavigationRequested,
            this,
            &CalmModeController::onMainFrameNavigationRequested);
        connect(
            m_page,
            &QWebEnginePage::loadFinished,
            this,
            &CalmModeController::onLoadFinished);
    }
}

void CalmModeController::setConfig(const CalmConfig &config)
{
    CalmConfig normalized = config;
    normalized.maxImageWidth = clampMaxImageWidth(normalized.maxImageWidth);
    m_policy.setExcludedHosts(normalized.excludedHosts);
    normalized.excludedHosts = m_policy.excludedHosts();

    if (normalized == m_config)
        return;

    m_config = normalized;
    if (!m_page || !m_enabled)
        return;

    if (m_appliedOnCurrentPage) {
        ++m_evalGeneration;
        m_appliedOnCurrentPage = false;
        m_pendingLoadEvaluation = true;
        m_page->triggerAction(QWebEnginePage::Reload);
    } else {
        evaluateCurrentPage();
    }
}

void CalmModeController::setMaxImageWidth(int maxWidth)
{
    CalmConfig updated = m_config;
    updated.maxImageWidth = clampMaxImageWidth(maxWidth);
    setConfig(updated);
}

void CalmModeController::setEnabled(bool enabled)
{
    if (enabled == m_enabled)
        return;

    m_enabled = enabled;
    ++m_evalGeneration;
    emit enabledChanged(m_enabled);

    if (m_enabled) {
        evaluateCurrentPage();
        return;
    }

    m_pendingLoadEvaluation = false;
    const QUrl currentUrl = m_page ? m_page->url() : QUrl();
    if (m_page && m_appliedOnCurrentPage) {
        m_appliedOnCurrentPage = false;
        QPointer<NetzacWebPage> pagePtr(m_page);
        m_page->runJavaScript(
            CalmDomInjector::buildRevertScript(),
            QWebEngineScript::ApplicationWorld,
            [pagePtr](const QVariant &) {
                if (pagePtr)
                    pagePtr->triggerAction(QWebEnginePage::Reload);
            });
    } else {
        m_appliedOnCurrentPage = false;
    }

    EvaluationReport report;
    report.status = Status::Disabled;
    report.url = currentUrl;
    emit evaluationFinished(report);
}

void CalmModeController::toggle()
{
    setEnabled(!m_enabled);
}

void CalmModeController::evaluateCurrentPage()
{
    if (!m_page || !m_enabled)
        return;

    const QUrl currentUrl = m_page->url();
    if (m_page->isBuiltInPage()) {
        m_appliedOnCurrentPage = false;
        EvaluationReport report;
        report.status = Status::SkippedBuiltIn;
        report.url = currentUrl;
        emit evaluationFinished(report);
        return;
    }

    if (m_policy.isExcludedUrl(currentUrl)) {
        m_appliedOnCurrentPage = false;
        EvaluationReport report;
        report.status = Status::ExcludedHost;
        report.url = currentUrl;
        emit evaluationFinished(report);
        return;
    }

    const quint64 gen = ++m_evalGeneration;
    QPointer<CalmModeController> self(this);
    m_page->runJavaScript(
        CalmDomInjector::buildApplyScript(m_config),
        QWebEngineScript::ApplicationWorld,
        [self, gen, currentUrl](const QVariant &result) {
            if (!self || gen != self->m_evalGeneration || !self->m_enabled)
                return;

            const QVariantMap map = result.toMap();
            const QString statusStr = map.value(QStringLiteral("status")).toString();

            EvaluationReport report;
            report.url = currentUrl;
            report.removedNodes = map.value(QStringLiteral("removed")).toInt();
            report.cappedImages = map.value(QStringLiteral("imagesCapped")).toInt();
            report.normalizedTables = map.value(QStringLiteral("tablesNormalized")).toInt();
            report.usedReaderFocus = map.value(QStringLiteral("readerFocus")).toBool();

            if (statusStr == QLatin1String("applied")) {
                report.status = Status::Applied;
                self->m_appliedOnCurrentPage = true;
            } else if (statusStr == QLatin1String("already-applied")) {
                report.status = Status::AlreadyApplied;
                self->m_appliedOnCurrentPage = true;
            } else if (statusStr == QLatin1String("skipped-js")) {
                report.status = Status::SkippedJavaScript;
                self->m_appliedOnCurrentPage = false;
            } else if (statusStr == QLatin1String("skipped-webapp")) {
                report.status = Status::SkippedWebApp;
                self->m_appliedOnCurrentPage = false;
            } else {
                report.status = Status::Error;
                self->m_appliedOnCurrentPage = false;
            }

            emit self->evaluationFinished(report);
        });
}

void CalmModeController::onMainFrameNavigationRequested(
    const QUrl &url,
    QWebEnginePage::NavigationType type)
{
    // Invalidate in-flight evaluations from the previous document; keep target URL untouched.
    ++m_evalGeneration;
    m_appliedOnCurrentPage = false;
    m_pendingLoadEvaluation = m_enabled;
    emit navigationIntercepted(url, type);
}

void CalmModeController::onLoadFinished(bool ok)
{
    m_pendingLoadEvaluation = false;
    if (ok && m_enabled)
        evaluateCurrentPage();
}
