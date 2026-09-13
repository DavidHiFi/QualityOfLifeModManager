#include "HomeCatalog.h"
#include "AppSettings.h"
#include "Downloader.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
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
    return QStringLiteral("https://mestretm.github.io/CLL-Cod-Lan-Launcher/cll_home.json");
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
        error = QObject::tr("cll_home.json invalido: %1").arg(err.errorString());
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
    QFile f(cacheDir() + QStringLiteral("/cll_home.json"));
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
    const QString path = cacheDir() + QStringLiteral("/cll_home.json");
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
    return out;
}

Presence presenceOf(const Mod &mod, const InstallIndex &index)
{
    for (const LocalInstall &it : index) {
        if (!sameOrigin(mod.url, it.sourceUrl))
            continue;
        const QString catalogVer = normVer(mod.version);
        const QString localVer = normVer(it.releaseVersion);
        if (!catalogVer.isEmpty() && !localVer.isEmpty() && catalogVer != localVer)
            return Presence::Update;
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
