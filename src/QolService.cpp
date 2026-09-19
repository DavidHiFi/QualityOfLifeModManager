#include "QolService.h"
#include "AppSettings.h"
#include "ArchiveTool.h"
#include "Downloader.h"
#include "Version.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QTextStream>

namespace {

const QStringList kModFiles = {"mod.ff", "mod.iwd", "mod.json", "mod.all.sabl", "mod.all.sabs"};
const QStringList kReShadeFiles = {"ReShade.ini", "Cinematic Colour Grading.ini", "BO2.ini",
                                   "BO1.ini", "MW3.ini", "WAW.ini", "dxgi.dll"};
// The 20 controller glyphs the icon packs replace. Kept out of the texture pack
// copy so a texture install never undoes the chosen controller.
const QStringList kControllerNames = {
    "xenon_controller_top.iwi",
    "xenonbutton_a.iwi", "xenonbutton_b.iwi", "xenonbutton_x.iwi", "xenonbutton_y.iwi",
    "xenonbutton_back.iwi", "xenonbutton_start.iwi", "xenonbutton_lb.iwi", "xenonbutton_rb.iwi",
    "xenonbutton_lt.iwi", "xenonbutton_rt.iwi", "xenonbutton_ls.iwi", "xenonbutton_rs.iwi",
    "xenonbutton_dpad_all.iwi", "xenonbutton_dpad_up.iwi", "xenonbutton_dpad_down.iwi",
    "xenonbutton_dpad_left.iwi", "xenonbutton_dpad_right.iwi", "xenonbutton_dpad_ud.iwi",
    "xenonbutton_dpad_rl.iwi"};

QString manifestPath(const AppSettings &s, const QString &kind)
{
    return QDir(QolService::stateDir(s)).filePath(QStringLiteral("installed-%1.txt").arg(kind));
}

QStringList readManifest(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    QStringList out;
    QTextStream in(&f);
    while (!in.atEnd()) {
        const QString line = in.readLine().trimmed();
        if (!line.isEmpty())
            out << line;
    }
    return out;
}

bool writeManifest(const QString &path, const QStringList &rel)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
        return false;
    QTextStream out(&f);
    for (const QString &r : rel)
        out << r << '\n';
    return true;
}

// Copies src/** into dest, recording every relative path written.
bool copyTree(const QString &src, const QString &dest, const QStringList &skipNames,
              QStringList &written, QString &error)
{
    QDirIterator it(src, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString from = it.next();
        const QString rel = QDir(src).relativeFilePath(from);
        if (skipNames.contains(QFileInfo(from).fileName(), Qt::CaseInsensitive))
            continue;
        const QString to = QDir(dest).filePath(rel);
        QDir().mkpath(QFileInfo(to).absolutePath());
        if (QFileInfo::exists(to))
            QFile::remove(to);
        if (!QFile::copy(from, to)) {
            error = QObject::tr("Could not copy %1").arg(rel);
            return false;
        }
        written << rel;
    }
    return true;
}

// Deletes the files a manifest lists, then the manifest. Refuses paths that
// escape the destination.
bool removeManifest(const AppSettings &s, const QString &kind, const QString &dest, QString &error)
{
    const QString mf = manifestPath(s, kind);
    const QString root = QDir::cleanPath(dest);
    for (const QString &rel : readManifest(mf)) {
        const QString full = QDir::cleanPath(QDir(dest).filePath(rel));
        if (!full.startsWith(root + QLatin1Char('/'), Qt::CaseInsensitive))
            continue;
        if (QFileInfo::exists(full) && !QFile::remove(full)) {
            error = QObject::tr("Could not delete %1").arg(rel);
            return false;
        }
    }
    QFile::remove(mf);
    return true;
}

QJsonArray githubReleases(const QString &repo, QString &error)
{
    const QByteArray raw = Downloader::downloadBytes(
        QStringLiteral("https://api.github.com/repos/%1/releases?per_page=30").arg(repo), error, 30000);
    if (raw.isEmpty())
        return {};
    const QJsonDocument doc = QJsonDocument::fromJson(raw);
    if (!doc.isArray()) {
        error = QObject::tr("GitHub did not return a release list (rate limited? it allows 60 checks an hour).");
        return {};
    }
    return doc.array();
}

// First .zip asset across releases (newest first) whose name matches `want`
// and none of `avoid` (case-insensitive substrings).
QString findAssetUrl(const QJsonArray &releases, const QStringList &want, const QStringList &avoid,
                     QString *tagOut)
{
    for (const QJsonValue &rv : releases) {
        const QJsonObject rel = rv.toObject();
        for (const QJsonValue &av : rel.value(QStringLiteral("assets")).toArray()) {
            const QJsonObject a = av.toObject();
            const QString name = a.value(QStringLiteral("name")).toString();
            if (!name.endsWith(QLatin1String(".zip"), Qt::CaseInsensitive))
                continue;
            bool ok = true;
            for (const QString &w : want)
                if (!name.contains(w, Qt::CaseInsensitive)) ok = false;
            for (const QString &x : avoid)
                if (name.contains(x, Qt::CaseInsensitive)) ok = false;
            if (!ok)
                continue;
            if (tagOut)
                *tagOut = rel.value(QStringLiteral("tag_name")).toString();
            return a.value(QStringLiteral("browser_download_url")).toString();
        }
    }
    return QString();
}

QString tempDir(const QString &tag)
{
    const QString d = QDir::temp().filePath(QStringLiteral("qol-%1-%2").arg(tag).arg(QDateTime::currentMSecsSinceEpoch()));
    QDir().mkpath(d);
    return d;
}

bool downloadAndUnpack(const QString &url, const QString &tag, const QolService::Progress &p,
                       QString &unpacked, QString &error)
{
    const QString work = tempDir(tag);
    const QString zip = QDir(work).filePath(QStringLiteral("pack.zip"));
    if (p) p(QObject::tr("Downloading..."), 5);
    const bool ok = Downloader::downloadToFile(url, zip, error, [&](qint64 got, qint64 total) {
        if (p) p(QObject::tr("Downloading..."), total > 0 ? int(5 + got * 65 / total) : 10);
    }, 30 * 60 * 1000);
    if (!ok)
        return false;
    if (p) p(QObject::tr("Unpacking..."), 72);
    unpacked = QDir(work).filePath(QStringLiteral("unpacked"));
    QDir().mkpath(unpacked);
    if (!ArchiveTool::extractToDirectory(zip, unpacked, &error))
        return false;
    QFile::remove(zip);
    return true;
}

// The folder whose contents belong at the destination: <unpacked>/<inner>,
// else the single subfolder, else <unpacked> itself.
QString packRoot(const QString &unpacked, const QString &inner)
{
    if (!inner.isEmpty() && QDir(QDir(unpacked).filePath(inner)).exists())
        return QDir(unpacked).filePath(inner);
    const QStringList subs = QDir(unpacked).entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    const QStringList files = QDir(unpacked).entryList(QDir::Files);
    if (subs.size() == 1 && files.isEmpty()) {
        const QString one = QDir(unpacked).filePath(subs.first());
        if (!inner.isEmpty() && QDir(QDir(one).filePath(inner)).exists())
            return QDir(one).filePath(inner);
        return one;
    }
    return unpacked;
}

// Recursive search for the directory that holds mod.json plus the other mod files.
QString findModDir(const QString &root)
{
    QDirIterator it(root, {"mod.json"}, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString dir = QFileInfo(it.next()).absolutePath();
        bool all = true;
        for (const QString &f : kModFiles)
            if (!QFileInfo::exists(QDir(dir).filePath(f))) all = false;
        if (all)
            return dir;
    }
    return QString();
}

} // namespace

namespace QolService {

QList<SeriesMod> series()
{
    return {
        {"t6", "Black Ops II",  "DavidHiFi/T6-QoL", "zm_qol", true},
        {"t4", "World at War",  "DavidHiFi/T4-QoL", "",       false},
        {"t5", "Black Ops",     "DavidHiFi/T5-QoL", "",       false},
        {"t7", "Black Ops III", "DavidHiFi/T7-QoL", "",       false},
    };
}

SeriesMod seriesFor(const QString &gameCode)
{
    for (const SeriesMod &m : series())
        if (m.gameCode == gameCode)
            return m;
    return SeriesMod{};
}

QString t6Storage(const AppSettings &s) { return QDir(s.plutoniumInstance).filePath(QStringLiteral("storage/t6")); }
QString stateDir(const AppSettings &s)  { return QDir(t6Storage(s)).filePath(QStringLiteral("_zm_qol_installer")); }
QString reShadeVault(const AppSettings &s) { return QDir(stateDir(s)).filePath(QStringLiteral("reshade-vault")); }
QString modsDir(const AppSettings &s, const QString &gameCode)
{
    return QDir(s.plutoniumInstance).filePath(QStringLiteral("storage/%1/mods").arg(gameCode));
}

QString installedModVersion(const AppSettings &s, const SeriesMod &m)
{
    if (m.folder.isEmpty())
        return QString();
    QFile f(QDir(modsDir(s, m.gameCode)).filePath(m.folder + QStringLiteral("/mod.json")));
    if (!f.open(QIODevice::ReadOnly))
        return QString();
    const QJsonObject o = QJsonDocument::fromJson(f.readAll()).object();
    QString v = o.value(QStringLiteral("version")).toString();
    if (v.isEmpty())
        v = o.value(QStringLiteral("name")).toString();
    v.remove(QRegularExpression(QStringLiteral("\\^[0-9]")));
    if (v.isEmpty())
        return QStringLiteral("installed");
    return v.startsWith(QLatin1Char('v')) ? v : QStringLiteral("v") + v;
}

QString latestModVersion(const SeriesMod &m, QString &error)
{
    const QByteArray raw = Downloader::downloadBytes(
        QStringLiteral("https://api.github.com/repos/%1/releases/latest").arg(m.repo), error, 20000);
    if (raw.isEmpty())
        return QString();
    return QJsonDocument::fromJson(raw).object().value(QStringLiteral("tag_name")).toString();
}

bool installMod(const AppSettings &s, const SeriesMod &m, const Progress &p, QString &error)
{
    if (!m.released || m.folder.isEmpty()) {
        error = QObject::tr("%1 has no release yet.").arg(m.gameTitle);
        return false;
    }
    if (p) p(QObject::tr("Looking up the latest release..."), 2);
    const QJsonArray rels = githubReleases(m.repo, error);
    if (rels.isEmpty())
        return false;
    QString tag;
    const QString url = findAssetUrl(rels, {}, {"texture", "sound", "controller", "icon", "portable"}, &tag);
    if (url.isEmpty()) {
        error = QObject::tr("No mod package found in the releases of %1.").arg(m.repo);
        return false;
    }
    QString unpacked;
    if (!downloadAndUnpack(url, QStringLiteral("mod"), p, unpacked, error))
        return false;
    const QString src = findModDir(unpacked);
    if (src.isEmpty()) {
        error = QObject::tr("The package did not contain mod.ff, mod.iwd and mod.json together.");
        return false;
    }
    if (p) p(QObject::tr("Installing %1...").arg(m.folder), 85);
    const QString dest = QDir(modsDir(s, m.gameCode)).filePath(m.folder);
    QDir().mkpath(dest);
    for (const QString &old : QDir(dest).entryList({"*.ff", "*.iwd", "*.json", "*.sabl", "*.sabs"}, QDir::Files))
        QFile::remove(QDir(dest).filePath(old));
    for (const QString &f : kModFiles) {
        if (!QFile::copy(QDir(src).filePath(f), QDir(dest).filePath(f))) {
            error = QObject::tr("Could not copy %1").arg(f);
            return false;
        }
    }
    QDir(unpacked).removeRecursively();
    if (p) p(QObject::tr("Done"), 100);
    return true;
}

namespace {
QString packKind(Pack pack) { return pack == Pack::Textures ? QStringLiteral("images") : QStringLiteral("zone"); }
QString packDest(const AppSettings &s, Pack pack) { return QDir(t6Storage(s)).filePath(packKind(pack)); }
}

bool packInstalled(const AppSettings &s, Pack pack)
{
    return QFileInfo::exists(manifestPath(s, packKind(pack)));
}

bool installPack(const AppSettings &s, Pack pack, const Progress &p, QString &error)
{
    const SeriesMod t6 = seriesFor(QStringLiteral("t6"));
    if (p) p(QObject::tr("Looking up the latest release..."), 2);
    const QJsonArray rels = githubReleases(t6.repo, error);
    if (rels.isEmpty())
        return false;
    const QString keyword = pack == Pack::Textures ? QStringLiteral("texture") : QStringLiteral("sound");
    const QString url = findAssetUrl(rels, {keyword}, {}, nullptr);
    if (url.isEmpty()) {
        error = QObject::tr("No %1 pack found in the releases of %2.").arg(keyword, t6.repo);
        return false;
    }
    QString unpacked;
    if (!downloadAndUnpack(url, packKind(pack), p, unpacked, error))
        return false;
    const QString src = packRoot(unpacked, packKind(pack));
    if (p) p(QObject::tr("Copying files..."), 85);
    QStringList written;
    // Controller glyphs are owned by the controller pack; a texture install
    // must not overwrite the one the user chose.
    const QStringList skip = pack == Pack::Textures
        ? kControllerNames + QStringList{QStringLiteral("hud_dpad_blood.iwi")}
        : QStringList{};
    if (!copyTree(src, packDest(s, pack), skip, written, error))
        return false;
    writeManifest(manifestPath(s, packKind(pack)), written);
    QDir(unpacked).removeRecursively();
    if (p) p(QObject::tr("Done"), 100);
    return true;
}

bool removePack(const AppSettings &s, Pack pack, QString &error)
{
    return removeManifest(s, packKind(pack), packDest(s, pack), error);
}

QString installedController(const AppSettings &s)
{
    if (!QFileInfo::exists(manifestPath(s, QStringLiteral("controller"))))
        return QString();
    QFile f(QDir(stateDir(s)).filePath(QStringLiteral("controller-pack.txt")));
    if (!f.open(QIODevice::ReadOnly))
        return QStringLiteral("installed");
    return QString::fromUtf8(f.readAll()).trimmed();
}

bool installController(const AppSettings &s, const QString &pack, const Progress &p, QString &error)
{
    const QString folderName = pack == QLatin1String("ps5")    ? QStringLiteral("Dualsense Icons")
                             : pack == QLatin1String("switch") ? QStringLiteral("Nintendo Switch Icons")
                                                               : QStringLiteral("Xbox One Buttons");
    // Cached per pack under the state folder so switching packs is offline after the first download.
    const QString cache = QDir(stateDir(s)).filePath(QStringLiteral("controller-packs"));
    QString src;
    {
        QDirIterator it(cache, QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString d = it.next();
            if (QFileInfo(d).fileName().compare(folderName, Qt::CaseInsensitive) == 0) { src = d; break; }
        }
    }
    if (src.isEmpty()) {
        const SeriesMod t6 = seriesFor(QStringLiteral("t6"));
        if (p) p(QObject::tr("Looking up the latest release..."), 2);
        const QJsonArray rels = githubReleases(t6.repo, error);
        if (rels.isEmpty())
            return false;
        const QString url = findAssetUrl(rels, {"controller"}, {}, nullptr);
        if (url.isEmpty()) {
            error = QObject::tr("No controller icon pack found in the releases of %1.").arg(t6.repo);
            return false;
        }
        QString unpacked;
        if (!downloadAndUnpack(url, QStringLiteral("controller"), p, unpacked, error))
            return false;
        QDir(cache).removeRecursively();
        QDir().mkpath(QFileInfo(cache).absolutePath());
        if (!QDir().rename(unpacked, cache)) {
            QStringList tmp;
            if (!copyTree(unpacked, cache, {}, tmp, error))
                return false;
        }
        QDirIterator it(cache, QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString d = it.next();
            if (QFileInfo(d).fileName().compare(folderName, Qt::CaseInsensitive) == 0) { src = d; break; }
        }
        if (src.isEmpty()) {
            error = QObject::tr("The pack did not contain a \"%1\" folder.").arg(folderName);
            return false;
        }
    }
    // Pick the directory holding the most .iwi files, in case of a nested layout.
    {
        QString best = src; int bestN = QDir(src).entryList({"*.iwi"}, QDir::Files).size();
        QDirIterator it(src, QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString d = it.next();
            const int n = QDir(d).entryList({"*.iwi"}, QDir::Files).size();
            if (n > bestN) { best = d; bestN = n; }
        }
        src = best;
    }
    if (p) p(QObject::tr("Installing %1...").arg(folderName), 85);
    const QString dest = QDir(t6Storage(s)).filePath(QStringLiteral("images"));
    QString ignore;
    removeManifest(s, QStringLiteral("controller"), dest, ignore);
    QStringList written;
    if (!copyTree(src, dest, {}, written, error))
        return false;
    writeManifest(manifestPath(s, QStringLiteral("controller")), written);
    QFile tag(QDir(stateDir(s)).filePath(QStringLiteral("controller-pack.txt")));
    if (tag.open(QIODevice::WriteOnly | QIODevice::Truncate))
        tag.write(pack.toUtf8());
    if (p) p(QObject::tr("Done"), 100);
    return true;
}

bool removeController(const AppSettings &s, QString &error)
{
    const QString dest = QDir(t6Storage(s)).filePath(QStringLiteral("images"));
    if (!removeManifest(s, QStringLiteral("controller"), dest, error))
        return false;
    QFile::remove(QDir(stateDir(s)).filePath(QStringLiteral("controller-pack.txt")));
    return true;
}

bool reShadeInstalled(const AppSettings &s)
{
    return QFileInfo::exists(QDir(s.plutoniumInstance).filePath(QStringLiteral("bin/dxgi.dll")));
}

bool installReShade(const AppSettings &s, QString &error)
{
    const QString bin = QDir(s.plutoniumInstance).filePath(QStringLiteral("bin"));
    if (!QDir(bin).exists()) {
        error = QObject::tr("Plutonium's bin folder was not found under %1.").arg(QDir::toNativeSeparators(s.plutoniumInstance));
        return false;
    }
    // One-time backup of whatever ReShade files were there before us.
    const QString backup = QDir(t6Storage(s)).filePath(QStringLiteral("backups/reshade"));
    if (!QDir(backup).exists()) {
        QDir().mkpath(backup);
        for (const QString &f : kReShadeFiles) {
            const QString from = QDir(bin).filePath(f);
            if (QFileInfo::exists(from))
                QFile::copy(from, QDir(backup).filePath(f));
        }
    }
    QStringList written;
    const QString vault = reShadeVault(s);
    QDir(vault).removeRecursively();
    QDir().mkpath(vault);
    for (const QString &f : kReShadeFiles) {
        QFile res(QStringLiteral(":/reshade/") + f);
        if (!res.open(QIODevice::ReadOnly)) {
            error = QObject::tr("%1 is missing from this build.").arg(f);
            return false;
        }
        const QByteArray data = res.readAll();
        for (const QString &dir : {bin, vault}) {
            QFile out(QDir(dir).filePath(f));
            if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate) || out.write(data) != data.size()) {
                error = QObject::tr("Could not write %1").arg(QDir::toNativeSeparators(out.fileName()));
                return false;
            }
        }
        written << f;
    }
    writeManifest(manifestPath(s, QStringLiteral("reshade")), written);
    return true;
}

bool removeReShade(const AppSettings &s, QString &error)
{
    const QString bin = QDir(s.plutoniumInstance).filePath(QStringLiteral("bin"));
    if (!removeManifest(s, QStringLiteral("reshade"), bin, error))
        return false;
    QDir(reShadeVault(s)).removeRecursively();
    return true;
}

} // namespace QolService
