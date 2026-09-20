#include "HomeCatalog.h"
#include "Version.h"
#include "AppSettings.h"
#include "Downloader.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QJsonObject>
#include <QCryptographicHash>
#include <QLocale>
#include <QObject>
#include <QUrl>

namespace {

QStringList toStringList(const QJsonValue &v)
{
    QStringList out;
    for (const QJsonValue &item : v.toArray())
        out << item.toString();
    return out;
}

QHash<QString, QPixmap> &cache()
{
    static QHash<QString, QPixmap> c;
    return c;
}

QString normVer(QString s)
{
    s = s.trimmed().toLower();
    if (s.startsWith(QLatin1Char('v')))
        s.remove(0, 1);
    return s;
}

// T6 mod names and versions carry inline colour codes: "^5Quality Of Life",
// "^32.16.16". The regex needs the caret escaped for the regex engine, so the
// C++ literal needs two backslashes.
QString stripColorCodes(QString s)
{
    return s.remove(QRegularExpression(QStringLiteral("\\^[0-9]")));
}

// A version we are willing to reason about: digits and dots only, after the
// optional leading "v". Release tags like "beta2" deliberately fail this.
bool isComparableVersion(const QString &normalised)
{
    static const QRegularExpression re(QStringLiteral("^[0-9]+(\\.[0-9]+)*$"));
    return re.match(normalised).hasMatch();
}

// -1 a<b, 0 equal, 1 a>b. Only valid for two isComparableVersion strings.
int compareVersions(const QString &a, const QString &b)
{
    const QStringList pa = a.split(QLatin1Char('.'));
    const QStringList pb = b.split(QLatin1Char('.'));
    for (int i = 0; i < qMax(pa.size(), pb.size()); ++i) {
        const int va = i < pa.size() ? pa.at(i).toInt() : 0;
        const int vb = i < pb.size() ? pb.at(i).toInt() : 0;
        if (va != vb)
            return va < vb ? -1 : 1;
    }
    return 0;
}

// The author's version from a mod folder's mod.json, colour codes stripped.
QString modJsonVersion(const QString &modDir)
{
    QFile f(QDir(modDir).filePath(QStringLiteral("mod.json")));
    if (!f.open(QIODevice::ReadOnly))
        return {};
    const QJsonObject o = QJsonDocument::fromJson(f.readAll()).object();
    return stripColorCodes(o.value(QStringLiteral("version")).toString()).trimmed();
}

QString githubRepoKey(const QString &url)
{
    const QUrl u(url);
    const QString host = u.host().toLower();
    if (!host.contains(QLatin1String("github.com")))
        return QString();
    QStringList parts = u.path().split(QLatin1Char('/'), Qt::SkipEmptyParts);
    if (parts.size() < 2)
        return QString();
    QString repo = parts[1];
    if (repo.endsWith(QLatin1String(".git"), Qt::CaseInsensitive))
        repo.chop(4);
    return parts[0].toLower() + QLatin1Char('/') + repo.toLower();
}

bool sameOrigin(const QString &a, const QString &b)
{
    if (a.isEmpty() || b.isEmpty())
        return false;
    const QString ra = githubRepoKey(a);
    const QString rb = githubRepoKey(b);
    if (!ra.isEmpty() && ra == rb)
        return true;
    QString na = a.trimmed().toLower();
    QString nb = b.trimmed().toLower();
    while (na.endsWith(QLatin1Char('/')))
        na.chop(1);
    while (nb.endsWith(QLatin1Char('/')))
        nb.chop(1);
    return na == nb;
}

QString imageCacheFile(const QString &url)
{
    const QByteArray hex = QCryptographicHash::hash(url.toUtf8(), QCryptographicHash::Sha1).toHex();
    return HomeCatalog::cacheDir() + QStringLiteral("/img/") + QString::fromLatin1(hex);
}

} // namespace

namespace HomeCatalog {

QString catalogUrl()
{
    return QStringLiteral(QOL_HOME_FEED_URL);
}

QString defaultPath()
{
    return catalogUrl();
}

QString cacheDir()
{
    return QDir(AppSettings::projectRoot()).filePath(QStringLiteral("cache/home"));
}

Catalog parseCatalog(const QByteArray &raw, QString &error)
{
    Catalog c;
    QJsonParseError err {};
    const QJsonDocument doc = QJsonDocument::fromJson(raw, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        error = QObject::tr("qol_home.json is not valid: %1").arg(err.errorString());
        return c;
    }

    const QJsonObject root = doc.object();
    c.title = root.value("title").toString();
    c.featured = toStringList(root.value("featured"));

    for (const QJsonValue &v : root.value("games").toArray()) {
        const QJsonObject o = v.toObject();
        Game g;
        g.id = o.value("id").toString();
        g.code = o.value("code").toString();
        g.name = o.value("name").toString();
        g.icon = o.value("icon").toString();
        g.logo = o.value("logo").toString();
        g.art = o.value("art").toString();
        if (!g.id.isEmpty())
            c.games << g;
    }

    for (const QJsonValue &v : root.value("mods").toArray()) {
        const QJsonObject o = v.toObject();
        Mod m;
        m.id = o.value("id").toString();
        m.name = o.value("name").toString();
        m.game = o.value("game").toString();
        m.author = o.value("author").toString();
        m.summary = o.value("summary").toString();
        m.description = o.value("description").toString();
        if (m.description.isEmpty())
            m.description = m.summary;
        m.image = o.value("image").toString();
        m.version = o.value("version").toString();
        m.size = o.value("size").toString();
        m.kind = o.value("kind").toString(QStringLiteral("cll"));
        m.url = o.value("url").toString();
        m.tags = toStringList(o.value("tags"));
        m.downloads = qint64(o.value("downloads").toDouble());
        if (!m.id.isEmpty())
            c.mods << m;
    }
    c.ok = true;
    return c;
}

Catalog loadFromCacheFile(QString &error)
{
    QFile f(cacheDir() + QStringLiteral("/qol_home.json"));
    if (!f.open(QIODevice::ReadOnly)) {
        error = QObject::tr("Could not load the page.");
        return {};
    }
    Catalog c = parseCatalog(f.readAll(), error);
    if (c.ok)
        c.fromCache = true;
    return c;
}

void saveCatalogCache(const QByteArray &raw)
{
    const QString path = cacheDir() + QStringLiteral("/qol_home.json");
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        f.write(raw);
}

Catalog load(const QString &path)
{
    Catalog c;
    const QString url = (path.startsWith(QLatin1String("http://"), Qt::CaseInsensitive)
                         || path.startsWith(QLatin1String("https://"), Qt::CaseInsensitive))
                            ? path
                            : catalogUrl();

    QString netError;
    const QByteArray raw = Downloader::downloadBytes(url, netError, 30000);
    if (!raw.isEmpty()) {
        QString parseError;
        c = parseCatalog(raw, parseError);
        if (c.ok) {
            saveCatalogCache(raw);
            return c;
        }
        netError = parseError;
    }

    QString cacheError;
    Catalog cached = loadFromCacheFile(cacheError);
    if (!cached.ok) {
        // Never downloaded: fall back to the copy built into the exe.
        QFile builtIn(QStringLiteral(":/qol_home.json"));
        if (builtIn.open(QIODevice::ReadOnly)) {
            QString e;
            cached = parseCatalog(builtIn.readAll(), e);
            if (cached.ok)
                cached.fromCache = true;
        }
    }
    if (cached.ok) {
        cached.error = netError.isEmpty()
                           ? QObject::tr("Showing the last downloaded catalog.")
                           : QObject::tr("Showing the last downloaded catalog. %1").arg(netError);
        return cached;
    }

    c.error = netError.isEmpty()
                  ? QObject::tr("Could not load the page.")
                  : QObject::tr("Could not load the page. %1").arg(netError);
    return c;
}

bool isRemote(const QString &ref)
{
    return ref.startsWith("http://", Qt::CaseInsensitive) || ref.startsWith("https://", Qt::CaseInsensitive);
}

QString resolveAsset(const QString &ref)
{
    if (ref.isEmpty() || isRemote(ref) || ref.startsWith(':'))
        return ref;

    const QFileInfo fi(ref);
    if (fi.isAbsolute())
        return ref;

    const QString local = QDir(AppSettings::projectRoot()).filePath(ref);
    if (QFileInfo::exists(local))
        return local;

    QString rel = ref;
    if (rel.startsWith("./"))
        rel.remove(0, 2);
    const QString qrc = ":/" + rel;
    if (QFileInfo::exists(qrc))
        return qrc;

    return local; // deixa o caminho local para a mensagem de erro fazer sentido
}

QPixmap pixmap(const QString &ref)
{
    if (ref.isEmpty() || isRemote(ref))
        return QPixmap();

    const QString path = resolveAsset(ref);
    auto it = cache().constFind(path);
    if (it != cache().constEnd())
        return it.value();

    QPixmap pm;
    pm.load(path);
    cache().insert(path, pm);
    return pm;
}

const Mod *findMod(const Catalog &c, const QString &id)
{
    for (const Mod &m : c.mods)
        if (m.id == id)
            return &m;
    return nullptr;
}

const Game *findGame(const Catalog &c, const QString &id)
{
    for (const Game &g : c.games)
        if (g.id == id)
            return &g;
    return nullptr;
}

QString formatDownloads(qint64 n)
{
    QLocale loc;
    if (n >= 1000000)
        return loc.toString(n / 1000000.0, 'f', 1) + "M";
    if (n >= 1000)
        return loc.toString(n / 1000.0, 'f', 1) + "K";
    return QString::number(n);
}

InstallIndex scanInstalled(const QString &plutoniumRoot)
{
    InstallIndex out;
    if (plutoniumRoot.isEmpty())
        return out;
    const QString pu = QDir::cleanPath(plutoniumRoot);
    auto ingest = [&](const QString &path, const QString &gameCode, const QString &folder) {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly))
            return;
        const QJsonObject o = QJsonDocument::fromJson(f.readAll()).object();
        if (o.isEmpty())
            return;
        LocalInstall it;
        it.sourceUrl = o.value(QStringLiteral("sourceUrl")).toString();
        if (it.sourceUrl.isEmpty())
            it.sourceUrl = o.value(QStringLiteral("archive")).toString();
        it.releaseVersion = o.value(QStringLiteral("releaseVersion")).toString();
        if (it.releaseVersion.isEmpty())
            it.releaseVersion = o.value(QStringLiteral("releaseTag")).toString();
        it.shortHash = o.value(QStringLiteral("shortHash")).toString();
        it.folder = folder;
        it.gameCode = gameCode;
        // The catalog quotes the author's version, so carry that too: the
        // release tag it came from is often a different naming scheme entirely
        // ("beta2" for a mod whose mod.json says 2.0).
        it.modVersion = modJsonVersion(QFileInfo(path).absolutePath());
        if (it.sourceUrl.isEmpty() && it.releaseVersion.isEmpty())
            return;
        out << it;
    };

    for (const QString &code : QStringList{QStringLiteral("t4"), QStringLiteral("t5"),
                                           QStringLiteral("t6"), QStringLiteral("iw5")}) {
        const QString mods = pu + QStringLiteral("/storage/") + code + QStringLiteral("/mods");
        for (const QString &folder : QDir(mods).entryList(QDir::Dirs | QDir::NoDotAndDotDot))
            ingest(mods + QLatin1Char('/') + folder + QStringLiteral("/download.json"), code, folder);
        const QString host = pu + QStringLiteral("/storage/") + code + QStringLiteral("/.cll_host");
        for (const QString &folder : QDir(host).entryList(QDir::Dirs | QDir::NoDotAndDotDot))
            ingest(host + QLatin1Char('/') + folder + QStringLiteral("/download.json"), code, folder);
    }
    // Quality of Life series mods are installed by QolService with no
    // download.json; read their mod.json so the Home badge is right.
    for (const QString &code : QStringList{QStringLiteral("t4"), QStringLiteral("t5"),
                                           QStringLiteral("t6"), QStringLiteral("iw5")}) {
        const QString mods = pu + QStringLiteral("/storage/") + code + QStringLiteral("/mods");
        for (const QString &folder : QDir(mods).entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            if (QFileInfo::exists(mods + QLatin1Char('/') + folder + QStringLiteral("/download.json")))
                continue;
            QFile f(mods + QLatin1Char('/') + folder + QStringLiteral("/mod.json"));
            if (!f.open(QIODevice::ReadOnly))
                continue;
            const QJsonObject o = QJsonDocument::fromJson(f.readAll()).object();
            const QString name = stripColorCodes(o.value(QStringLiteral("name")).toString());
            if (name.compare(QLatin1String("Quality Of Life"), Qt::CaseInsensitive) != 0
                && folder.compare(QLatin1String("zm_qol"), Qt::CaseInsensitive) != 0)
                continue;
            LocalInstall it;
            it.sourceUrl = QStringLiteral("https://github.com/DavidHiFi/T6-QoL");
            it.modVersion = stripColorCodes(o.value(QStringLiteral("version")).toString()).trimmed();
            it.releaseVersion = it.modVersion;
            it.folder = folder;
            it.gameCode = code;
            out << it;
        }
    }
    return out;
}

// Offer an update only when we can show the catalog is genuinely ahead of what
// is installed. Anything less certain reads as Installed, because a card that
// says "Update" forever - and still says it after the user updates - is worse
// than one that occasionally stays quiet about a new release.
Presence presenceOf(const Mod &mod, const InstallIndex &index)
{
    for (const LocalInstall &it : index) {
        if (!sameOrigin(mod.url, it.sourceUrl))
            continue;

        const QString catalogVer = normVer(mod.version);
        if (catalogVer.isEmpty())
            return Presence::Installed;

        // mod.json first: the catalog quotes the author's version, so that is
        // the like-for-like comparison. The release tag is the fallback, and
        // often is not a version at all.
        for (const QString &raw : {it.modVersion, it.releaseVersion}) {
            const QString localVer = normVer(raw);
            if (localVer.isEmpty())
                continue;
            if (localVer == catalogVer)
                return Presence::Installed;
            if (!isComparableVersion(localVer) || !isComparableVersion(catalogVer))
                continue; // e.g. "beta2" against "2.0": not the same scheme
            // Strictly newer upstream is the only thing worth a prompt; an
            // install ahead of the catalog is not out of date.
            return compareVersions(catalogVer, localVer) > 0 ? Presence::Update
                                                             : Presence::Installed;
        }
        return Presence::Installed;
    }
    return Presence::Missing;
}

QPixmap cachedRemotePixmap(const QString &url)
{
    if (url.isEmpty())
        return QPixmap();
    const QString path = imageCacheFile(url);
    auto it = cache().constFind(path);
    if (it != cache().constEnd())
        return it.value();
    QPixmap pm;
    if (pm.load(path))
        cache().insert(path, pm);
    return pm;
}

void storeRemotePixmap(const QString &url, const QByteArray &bytes)
{
    if (url.isEmpty() || bytes.isEmpty())
        return;
    const QString path = imageCacheFile(url);
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        f.write(bytes);
    QPixmap pm;
    if (pm.loadFromData(bytes))
        cache().insert(path, pm);
}

} // namespace HomeCatalog
