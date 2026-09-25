#include "QolService.h"
#include "AppSettings.h"
#include "ArchiveTool.h"
#include "Downloader.h"
#include "GameLauncher.h"
#include "Version.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QRegularExpression>
#include <QUrl>
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

// The 18 first-person arm re-textures for the Victis, Mob of the Dead and
// Richtofen crews. The user wants stock arms on every map, so a texture
// install must never lay these down.
const QStringList kViewArmNames = {
    "~-gviewarm_zom_armhair_alpha_c.iwi",
    "~-gviewarm_zom_deluca_longsleeve_c.iwi", "viewarm_zom_deluca_longsleeve_n.iwi",
    "~-gviewarm_zom_engineer_c.iwi", "viewarm_zom_engineer_n.iwi",
    "~-gviewarm_zom_handsome_barea~031b2e1b.iwi",
    "~-gviewarm_zom_handsome_barearm_left_c.iwi", "viewarm_zom_handsome_barearm_n.iwi",
    "~-gviewarm_zom_oldman_c.iwi", "viewarm_zom_oldman_n.iwi",
    "~-gviewarm_zom_oleary_shortsleeve_c.iwi", "viewarm_zom_oleary_shortsleeve_n.iwi",
    "~-gviewarm_zom_reporter_c.iwi", "viewarm_zom_reporter_n.iwi",
    "~-gviewarm_zom_richtofen_l_c.iwi", "~-gviewarm_zom_richtofen_r_c.iwi",
    "viewarm_zom_richtofen_n.iwi",
    "~~-gviewarm_zom_strands_alpha~b94bebe4.iwi"};

// The pack's 41 hash-named overrides. A numerically-named .iwi binds its pixels
// onto whatever stock image owns that hash, so these paint custom art
// (flagstone paving, house siding, wallpaper, signage, blinds) onto stock
// texture slots the base game never used them on. Published packs still carry
// them, so refuse them at the copy rather than trusting the archive.
const QStringList kHashBoundNames = {
    "184130943.iwi", "203218850.iwi", "213664688.iwi", "248893139.iwi",
    "310690366.iwi", "387107309.iwi", "414497708.iwi", "712299166.iwi",
    "749883228.iwi", "1242089752.iwi", "1361229722.iwi", "1402996581.iwi",
    "2139588580.iwi", "2193983524.iwi", "2402415086.iwi", "2463049788.iwi",
    "2531095777.iwi", "2532865902.iwi", "2596739401.iwi", "2652134348.iwi",
    "2696824118.iwi", "2696825333.iwi", "2897555984.iwi", "2963536512.iwi",
    "3009761504.iwi", "3104746560.iwi", "3113946313.iwi", "3195629244.iwi",
    "3277816579.iwi", "3343758740.iwi", "3353894529.iwi", "3355417974.iwi",
    "3411929122.iwi", "3449947920.iwi", "3566024970.iwi", "3718463383.iwi",
    "3818552282.iwi", "3958649079.iwi", "3971393839.iwi", "4094420848.iwi",
    "4238982186.iwi"};

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

// A release asset, with everything GitHub will tell us about it. The digest
// matters: these packs are hundreds of megabytes, and an 843 MB download that
// arrives one bad block short only announces itself as "7-Zip failed (code 2)
// ... CRC Failed" partway through unpacking, which reads like a broken pack
// rather than a broken transfer.
struct Asset {
    QString url;
    QString name;
    QString tag;
    QString sha256; // GitHub's own digest for the asset, bare hex
    qint64 size = 0;
    bool isValid() const { return !url.isEmpty(); }
};

// First .zip asset across releases (newest first) whose name matches `want`
// and none of `avoid` (case-insensitive substrings).
Asset findAsset(const QJsonArray &releases, const QStringList &want, const QStringList &avoid)
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
            Asset out;
            out.url = a.value(QStringLiteral("browser_download_url")).toString();
            out.name = name;
            out.tag = rel.value(QStringLiteral("tag_name")).toString();
            out.size = static_cast<qint64>(a.value(QStringLiteral("size")).toDouble());
            // "sha256:<hex>" in the API; older responses omit it entirely.
            const QString digest = a.value(QStringLiteral("digest")).toString().trimmed().toLower();
            if (digest.startsWith(QLatin1String("sha256:"))) {
                const QString hex = digest.mid(7);
                if (hex.size() == 64)
                    out.sha256 = hex;
            }
            return out;
        }
    }
    return {};
}

QString tempDir(const QString &tag)
{
    const QString d = QDir::temp().filePath(QStringLiteral("qol-%1-%2").arg(tag).arg(QDateTime::currentMSecsSinceEpoch()));
    QDir().mkpath(d);
    return d;
}

QString sha256Of(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    QCryptographicHash h(QCryptographicHash::Sha256);
    while (!f.atEnd())
        h.addData(f.read(4 * 1024 * 1024));
    return QString::fromLatin1(h.result().toHex());
}

// Download the asset, check it against what GitHub published for it, then
// unpack it.
//
// Two failures used to look identical here and they have opposite fixes, so
// they are told apart. A download that does not match GitHub's digest arrived
// damaged - worth retrying once, and worth saying so. A download that DOES
// match and still will not unpack is a damaged *pack*: zm_qol-sounds.zip sat on
// the releases page with one CRC-broken member inside it, and everyone who
// tried to install it was shown raw "7-Zip failed (code 2) ... CRC Failed"
// output, which reads like a fault on their own machine. It was not, and no
// amount of retrying was ever going to fix it.
bool downloadAndUnpack(const Asset &a, const QString &tag, const QolService::Progress &p,
                       QString &unpacked, QString &error)
{
    const QString work = tempDir(tag);
    const QString zip = QDir(work).filePath(QStringLiteral("pack.zip"));
    const bool canVerify = !a.sha256.isEmpty() || a.size > 0;
    bool verified = false;

    for (int attempt = 1; attempt <= 2 && !verified; ++attempt) {
        if (p) p(attempt == 1 ? QObject::tr("Downloading...")
                              : QObject::tr("The download was damaged; trying again..."), 5);
        if (!Downloader::downloadToFile(a.url, zip, error, [&](qint64 got, qint64 total) {
                if (p) p(QObject::tr("Downloading..."), total > 0 ? int(5 + got * 65 / total) : 10);
            }, 30 * 60 * 1000))
            return false;

        if (!canVerify)
            break; // nothing published to check against; carry on as before

        if (p) p(QObject::tr("Checking the download..."), 70);
        QString bad;
        if (a.size > 0 && QFileInfo(zip).size() != a.size)
            bad = QObject::tr("it is %1 bytes and should be %2")
                      .arg(QFileInfo(zip).size()).arg(a.size);
        else if (!a.sha256.isEmpty() && sha256Of(zip).compare(a.sha256, Qt::CaseInsensitive) != 0)
            bad = QObject::tr("its contents do not match the published checksum");

        if (bad.isEmpty()) {
            verified = true;
        } else {
            QFile::remove(zip);
            error = QObject::tr("%1 did not download correctly - %2. Check your "
                                "connection and try again.").arg(a.name, bad);
        }
    }
    if (canVerify && !verified)
        return false;

    if (p) p(QObject::tr("Unpacking..."), 72);
    unpacked = QDir(work).filePath(QStringLiteral("unpacked"));
    QDir().mkpath(unpacked);
    if (!ArchiveTool::extractToDirectory(zip, unpacked, &error)) {
        if (verified) {
            error = QObject::tr(
                        "%1 downloaded correctly, but it cannot be unpacked: the pack "
                        "published on GitHub is itself damaged. This is not your "
                        "connection or your PC, and retrying will not help. Nothing on "
                        "your game was changed.\n\n%2").arg(a.name, error);
        }
        QDir(work).removeRecursively();
        return false;
    }
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

QString runningGame()
{
    // A running Plutonium game holds mod.iwd and the sound banks open, so any
    // install into storage would fail half-way. Same guard the 1.x app had.
    // The DLSS 5 helper outlives the game by a moment and holds host64\ open
    // while it does, so it counts as running too.
    static const char *const kNames[] = {"plutonium-bootstrapper-win32.exe", "t6zm.exe", "t6mp.exe",
                                         "t5sp.exe", "t5mp.exe", "t4sp.exe", "t4mp.exe", "iw5mp.exe",
                                         "dlss5-feed-host64.exe"};
    // -selftest works on a scratch root no game can have open, and must not
    // refuse to run just because a real session is up elsewhere on the PC.
    if (qEnvironmentVariableIsSet("QOL_SELFTEST_SCRATCH"))
        return QString();
    for (const char *name : kNames) {
        const QString n = QString::fromLatin1(name);
        if (GameLauncher::findProcessByName(n) > 0)
            return n;
    }
    return QString();
}

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
    // /releases/latest redirects to /releases/tag/<tag>, which is the whole
    // answer. Plain github.com, so this works where api.github.com is blocked
    // or rate limited - and it is blocked outright on some machines, which
    // left this returning nothing and the page unable to see any new release.
    const QString target = Downloader::redirectTargetOf(
        QStringLiteral("https://github.com/%1/releases/latest").arg(m.repo), error, 15000);
    static const QRegularExpression re(QStringLiteral("/releases/tag/([^/?#]+)"));
    const QRegularExpressionMatch hit = re.match(target);
    if (hit.hasMatch()) {
        error.clear();
        return QUrl::fromPercentEncoding(hit.captured(1).toUtf8());
    }

    // Fall back to the API for anything that answers differently.
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
    if (const QString g = runningGame(); !g.isEmpty()) {
        error = QObject::tr("Close Plutonium first (%1 is running). The game keeps the mod files open while it runs.").arg(g);
        return false;
    }
    if (p) p(QObject::tr("Looking up the latest release..."), 2);
    const QJsonArray rels = githubReleases(m.repo, error);
    if (rels.isEmpty())
        return false;
    const Asset asset = findAsset(rels, {}, {"texture", "sound", "controller", "icon", "portable"});
    if (!asset.isValid()) {
        error = QObject::tr("No mod package found in the releases of %1.").arg(m.repo);
        return false;
    }
    QString unpacked;
    if (!downloadAndUnpack(asset, QStringLiteral("mod"), p, unpacked, error))
        return false;
    const QString src = findModDir(unpacked);
    if (src.isEmpty()) {
        error = QObject::tr("The package did not contain mod.ff, mod.iwd and mod.json together.");
        return false;
    }
    if (p) p(QObject::tr("Installing %1...").arg(m.folder), 85);
    const QString dest = QDir(modsDir(s, m.gameCode)).filePath(m.folder);
    QDir().mkpath(dest);
    // Copy beside the live files first, then swap, so a failure mid-way
    // leaves the previous install intact.
    QStringList staged;
    bool ok = true;
    for (const QString &f : kModFiles) {
        const QString tmp = QDir(dest).filePath(f + QStringLiteral(".new"));
        QFile::remove(tmp);
        if (!QFile::copy(QDir(src).filePath(f), tmp)) {
            error = QObject::tr("Could not copy %1 into %2").arg(f, QDir::toNativeSeparators(dest));
            ok = false;
            break;
        }
        staged << tmp;
    }
    if (ok) {
        for (const QString &f : kModFiles) {
            const QString live = QDir(dest).filePath(f);
            if (QFileInfo::exists(live) && !QFile::remove(live)) {
                error = QObject::tr("%1 is in use; close the game and try again.").arg(f);
                ok = false;
                break;
            }
            if (!QFile::rename(live + QStringLiteral(".new"), live)) {
                error = QObject::tr("Could not replace %1").arg(f);
                ok = false;
                break;
            }
        }
    }
    if (!ok)
        for (const QString &t : staged) QFile::remove(t);
    QDir(QFileInfo(unpacked).absolutePath()).removeRecursively();
    if (ok && p) p(QObject::tr("Done"), 100);
    return ok;
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
    if (const QString g = runningGame(); !g.isEmpty()) {
        error = QObject::tr("Close Plutonium first (%1 is running). The game keeps the mod files open while it runs.").arg(g);
        return false;
    }
    const SeriesMod t6 = seriesFor(QStringLiteral("t6"));
    if (p) p(QObject::tr("Looking up the latest release..."), 2);
    const QJsonArray rels = githubReleases(t6.repo, error);
    if (rels.isEmpty())
        return false;
    const QString keyword = pack == Pack::Textures ? QStringLiteral("texture") : QStringLiteral("sound");
    const Asset asset = findAsset(rels, {keyword}, {});
    if (!asset.isValid()) {
        error = QObject::tr("No %1 pack found in the releases of %2.").arg(keyword, t6.repo);
        return false;
    }
    QString unpacked;
    if (!downloadAndUnpack(asset, packKind(pack), p, unpacked, error))
        return false;
    const QString src = packRoot(unpacked, packKind(pack));
    if (p) p(QObject::tr("Copying files..."), 85);
    QStringList written;
    // Controller glyphs are owned by the controller pack; a texture install
    // must not overwrite the one the user chose.
    const QStringList skip = pack == Pack::Textures
        ? kControllerNames + kViewArmNames + kHashBoundNames
              + QStringList{QStringLiteral("hud_dpad_blood.iwi")}
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
    if (const QString g = runningGame(); !g.isEmpty()) {
        error = QObject::tr("Close Plutonium first (%1 is running). The game keeps the mod files open while it runs.").arg(g);
        return false;
    }
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
        const Asset asset = findAsset(rels, {"controller"}, {});
        if (!asset.isValid()) {
            error = QObject::tr("No controller icon pack found in the releases of %1.").arg(t6.repo);
            return false;
        }
        QString unpacked;
        if (!downloadAndUnpack(asset, QStringLiteral("controller"), p, unpacked, error))
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
    if (const QString g = runningGame(); !g.isEmpty()) {
        error = QObject::tr("Close Plutonium first (%1 is running).").arg(g);
        return false;
    }
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
    // Only the seven loose files are rewritten. The vault also holds the
    // shader pack - 473 effects healed into it by reshade-verify.ps1 - and
    // wiping the folder here used to throw that away on every install, which
    // is how a vault ends up empty while every check still says it exists.
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
    if (const QString g = runningGame(); !g.isEmpty()) {
        error = QObject::tr("Close Plutonium first (%1 is running).").arg(g);
        return false;
    }
    // The watchdog puts back anything that leaves bin, so it has to go first or
    // it would undo this removal a second later.
    GameLauncher::stopReShadeWatchdog();

    const QString bin = QDir(s.plutoniumInstance).filePath(QStringLiteral("bin"));
    if (!removeManifest(s, QStringLiteral("reshade"), bin, error))
        return false;
    // The manifest only covers what this app installed. ReShade also arrives by
    // hand, from a Plutonium repair, or from a script that restores bin, and
    // then there is no manifest at all - which used to make Remove a silent
    // no-op that still reported success. Remove asks for ReShade to be gone
    // from bin, so take the whole known set either way.
    for (const QString &f : kReShadeFiles) {
        const QString full = QDir(bin).filePath(f);
        if (QFileInfo::exists(full) && !QFile::remove(full)) {
            error = QObject::tr("Could not delete %1. Close Plutonium and any ReShade "
                                "overlay, then try again.")
                        .arg(QDir::toNativeSeparators(full));
            return false;
        }
    }
    // Only the loose files leave the vault. The shader pack under it - 473
    // effects, healed in by reshade-verify.ps1 and not shipped in this exe -
    // is the one thing here that cannot be rebuilt, and wiping it on a Remove
    // meant that turning ReShade back on came up with a near-empty tree and
    // the "ReShade with nothing" session all over again.
    for (const QString &f : kReShadeFiles)
        QFile::remove(QDir(reShadeVault(s)).filePath(f));

    // Never report success on a removal that did not happen.
    if (reShadeInstalled(s)) {
        error = QObject::tr("ReShade is still in %1 after removing it. Something is "
                            "putting it back - stop the ReShade watchdog and try again.")
                    .arg(QDir::toNativeSeparators(bin));
        return false;
    }
    return true;
}

// ===========================================================================
//  LAN: the add-on build, and DLSS 5 on top of it
// ===========================================================================

QString reShadeLanVault(const AppSettings &s)
{
    return QDir(stateDir(s)).filePath(QStringLiteral("reshade-lan"));
}

namespace {

// Everything LAN adds to bin on top of the shared install, relative to bin.
// The switch back to online walks this list, so nothing here can survive into
// an online session by being forgotten.
const QStringList kLanOnlyFiles = {
    QStringLiteral("dlss5-feed.addon32"),
    QStringLiteral("dlss5-feed.cfg"),
    QStringLiteral("dlss5-feed.log"),
    QStringLiteral("dlss5-feed-crash.dmp"),
    QStringLiteral("reshade-shaders/Shaders/DLSS5_Feed.fx"),
};
// The 64-bit helper and its NVIDIA runtimes. A whole folder, removed as one.
const QString kHostDir = QStringLiteral("host64");
// Every file the helper and the two shaders need. A payload short of any of
// these cannot run, however complete it looks. ReShade.fxh is on the list
// because both shaders include it and a PC with no shader pack has none: the
// first payload leaned on this PC's pack and would not have compiled anywhere
// else.
const QStringList kDlssRequired = {
    QStringLiteral("dlss5-feed.addon32"), QStringLiteral("dlss5-feed.cfg"),
    QStringLiteral("reshade-shaders/Shaders/DLSS5_Feed.fx"),
    QStringLiteral("reshade-shaders/Shaders/ReShade.fxh"),
    QStringLiteral("reshade-shaders/Shaders/LumeniteFX/lumenite_Kernel.fx"),
    QStringLiteral("host64/dlss5-feed-host64.exe"), QStringLiteral("host64/d3d12.dll"),
    QStringLiteral("host64/renodx-dlss5.addon64"),
    QStringLiteral("host64/nvngx_dlss.dll"), QStringLiteral("host64/nvngx_dlssnr.dll"),
    QStringLiteral("host64/ReShade.ini")};
// The DLSS preset. One fixed name, so ReShade's own preset switcher has an
// obvious pair to flip between: DLSS5 for playing with the neural pass, and
// whichever grading preset the user keeps for looks.
const QString kLanPresetName = QStringLiteral("DLSS5.ini");
// v1 named it "<their preset> DLSS.ini". Swept out of bin on sight.
const QString kOldLanPresetTag = QStringLiteral(" DLSS.ini");

// Takes the DLSS preset out of bin, keeping the latest copy in the LAN vault
// first. It is a generated file, but it is also where any tuning done during a
// LAN session lives, and an unticked box must not cost the user that.
void parkLanPresets(const QString &bin, const QString &lanVault)
{
    // v1's derived presets are not kept. They were a copy of the user's
    // grading with the DLSS techniques bolted on, which is the arrangement
    // this route exists to stop making.
    for (const QString &old : QDir(bin).entryList({QLatin1Char('*') + kOldLanPresetTag}, QDir::Files)) {
        QFile::remove(QDir(bin).filePath(old));
        QFile::remove(QDir(lanVault).filePath(old));
    }
    QStringList names;
    if (QFileInfo::exists(QDir(bin).filePath(kLanPresetName)))
        names << kLanPresetName;
    for (const QString &name : names) {
        QDir().mkpath(lanVault);
        const QString kept = QDir(lanVault).filePath(name);
        QFile::remove(kept);
        QFile::copy(QDir(bin).filePath(name), kept);
        QFile::remove(QDir(bin).filePath(name));
    }
}

// The two techniques the feeder needs enabled in whichever preset is loaded:
// the kernel that produces motion vectors (DLSS5_MV_PROVIDER=3) and the feed
// itself. Added to the user's own preset and taken out again, leaving every
// slider they have tuned untouched.
const QStringList kDlssTechniques = {
    QStringLiteral("Lumenite_Kernel@lumenite_Kernel.fx"),
    QStringLiteral("DLSS5_Feed@DLSS5_Feed.fx"),
};

QString activeMarkerPath(const AppSettings &s)
{
    return QDir(QolService::stateDir(s)).filePath(QStringLiteral("reshade-active.txt"));
}

// dlss5-feed.cfg is plain key=value, one per line, and the feeder re-reads it
// every 60 delivered frames. Only the named keys are touched; every other
// value, including anything set from the in-game panel, is left as it is.
void setFeederKeys(const QString &bin, const QList<QPair<QString, QString>> &keys)
{
    const QString path = QDir(bin).filePath(QStringLiteral("dlss5-feed.cfg"));
    QFile in(path);
    if (!in.open(QIODevice::ReadOnly))
        return;
    QList<QByteArray> lines = in.readAll().split('\n');
    in.close();
    for (const auto &kv : keys) {
        const QByteArray prefix = kv.first.toUtf8() + "=";
        bool found = false;
        for (QByteArray &line : lines) {
            const bool cr = line.endsWith('\r');
            if (!line.trimmed().startsWith(prefix))
                continue;
            line = prefix + kv.second.toUtf8() + (cr ? "\r" : "");
            found = true;
            break;
        }
        if (!found)
            lines << prefix + kv.second.toUtf8() + "\r";
    }
    QFile out(path);
    if (out.open(QIODevice::WriteOnly | QIODevice::Truncate))
        out.write(lines.join('\n'));
}

// The payload holds 237 MB of NVIDIA runtimes. Re-copying those on every
// launch would add seconds to a Play click for no gain, so anything already
// in bin byte for byte is left alone.
//
// Size alone is not enough, and one file in the payload proves it: the LAN
// copy of lumenite_Kernel.fx is a different build of the same shader the pack
// already ships. Two builds of one text file can easily land on one size, and
// the wrong one would be silently kept - the feeder reads its motion vectors
// from that exact build. Small files are compared; only the multi-megabyte
// runtimes, which are pinned release artefacts and never edited in place, are
// taken on size.
const qint64 kCompareContentsUnder = 4 * 1024 * 1024;

bool copyIfDifferent(const QString &from, const QString &to, QString &error)
{
    const QFileInfo src(from);
    const QFileInfo dst(to);
    if (dst.exists() && dst.size() == src.size()) {
        if (src.size() >= kCompareContentsUnder) {
            if (sha256Of(from) == sha256Of(to))
                return true;
        } else {
            QFile a(from), b(to);
            if (a.open(QIODevice::ReadOnly) && b.open(QIODevice::ReadOnly) && a.readAll() == b.readAll())
                return true;
        }
    }
    QDir().mkpath(dst.absolutePath());
    if (dst.exists() && !QFile::remove(to)) {
        error = QObject::tr("Could not replace %1").arg(QDir::toNativeSeparators(to));
        return false;
    }
    if (!QFile::copy(from, to)) {
        error = QObject::tr("Could not copy %1 to %2")
                    .arg(QDir::toNativeSeparators(from), QDir::toNativeSeparators(to));
        return false;
    }
    QFile::setPermissions(to, QFile::permissions(to) | QFileDevice::WriteOwner);
    return true;
}

bool writeResource(const QString &resource, const QString &dest, QString &error)
{
    QFile res(resource);
    if (!res.open(QIODevice::ReadOnly)) {
        error = QObject::tr("%1 is missing from this build.").arg(resource);
        return false;
    }
    const QByteArray data = res.readAll();
    QFileInfo have(dest);
    if (have.exists() && have.size() == data.size()) {
        QFile cur(dest);
        if (cur.open(QIODevice::ReadOnly) && cur.readAll() == data)
            return true;
    }
    QDir().mkpath(have.absolutePath());
    QFile out(dest);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate) || out.write(data) != data.size()) {
        error = QObject::tr("Could not write %1. Close Plutonium and any ReShade overlay, then try again.")
                    .arg(QDir::toNativeSeparators(dest));
        return false;
    }
    return true;
}

// --- the smallest .ini editor that is honest about ReShade's file ----------
//
// ReShade owns these files and rewrites them itself. Only the named key is
// touched; every other line is written back exactly as it was, which is what
// keeps the user's tuning across a mode switch.
QString iniGet(const QByteArray &text, const QString &section, const QString &key)
{
    QString current;
    for (const QByteArray &raw : text.split('\n')) {
        const QString line = QString::fromUtf8(raw).trimmed();
        if (line.startsWith(QLatin1Char('[')) && line.endsWith(QLatin1Char(']')))
            current = line.mid(1, line.size() - 2);
        else if (current.compare(section, Qt::CaseInsensitive) == 0
                 && line.startsWith(key + QLatin1Char('=')))
            return line.mid(key.size() + 1);
    }
    return QString();
}

QByteArray iniSet(const QByteArray &text, const QString &section, const QString &key, const QString &value)
{
    const QByteArray eol = text.contains("\r\n") ? QByteArray("\r\n") : QByteArray("\n");
    QList<QByteArray> lines = text.split('\n');
    for (QByteArray &l : lines)
        if (l.endsWith('\r'))
            l.chop(1);

    QString current;
    int sectionEnd = -1;       // last line of the section, for an insert
    bool inSection = section.isEmpty();
    for (int i = 0; i < lines.size(); ++i) {
        const QString line = QString::fromUtf8(lines[i]).trimmed();
        if (line.startsWith(QLatin1Char('[')) && line.endsWith(QLatin1Char(']'))) {
            const QString name = line.mid(1, line.size() - 2);
            if (inSection && sectionEnd < 0)
                sectionEnd = i;                      // section ended here
            inSection = name.compare(section, Qt::CaseInsensitive) == 0;
            continue;
        }
        if (inSection && line.startsWith(key + QLatin1Char('='))) {
            lines[i] = (key + QLatin1Char('=') + value).toUtf8();
            return lines.join(eol);
        }
    }
    const QByteArray entry = (key + QLatin1Char('=') + value).toUtf8();
    if (inSection || sectionEnd >= 0) {
        lines.insert(inSection ? lines.size() : sectionEnd, entry);
        return lines.join(eol);
    }
    if (!lines.isEmpty() && !lines.last().isEmpty())
        lines << QByteArray();
    lines << ('[' + section.toUtf8() + ']') << entry;
    return lines.join(eol);
}

bool rewriteIni(const QString &path, const QList<std::function<QByteArray(const QByteArray &)>> &edits,
                QString &error)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        error = QObject::tr("Could not read %1").arg(QDir::toNativeSeparators(path));
        return false;
    }
    const QByteArray before = f.readAll();
    f.close();
    QByteArray after = before;
    for (const auto &edit : edits)
        after = edit(after);
    if (after == before)
        return true;
    QFile out(path);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate) || out.write(after) != after.size()) {
        error = QObject::tr("Could not write %1").arg(QDir::toNativeSeparators(path));
        return false;
    }
    return true;
}

QString mergeTokens(const QString &csv, const QStringList &add, const QStringList &drop)
{
    QStringList out;
    for (const QString &t : csv.split(QLatin1Char(','), Qt::SkipEmptyParts)) {
        const QString v = t.trimmed();
        if (v.isEmpty() || out.contains(v, Qt::CaseInsensitive))
            continue;
        bool dropped = false;
        for (const QString &d : drop) {
            // A definition is dropped by its name, whatever it was set to.
            const QString name = d.section(QLatin1Char('='), 0, 0);
            if (v.compare(d, Qt::CaseInsensitive) == 0
                || v.section(QLatin1Char('='), 0, 0).compare(name, Qt::CaseInsensitive) == 0) {
                dropped = true;
                break;
            }
        }
        if (!dropped)
            out << v;
    }
    for (const QString &a : add)
        if (!out.contains(a, Qt::CaseInsensitive))
            out << a;
    return out.join(QLatin1Char(','));
}

// ---------------------------------------------------------------------------
//  THE USER'S PRESET IS NEVER EDITED. DLSS GETS ITS OWN.
//
//  The first version of this added the two techniques to whichever preset was
//  loaded. It worked, and then ReShade threw the file away: with all 473
//  effects reloading, a preset save landed while most of them had not compiled
//  yet, and ReShade writes Techniques= from what is enabled AT THAT MOMENT.
//  The user's four techniques and every slider under them were gone from both
//  bin and the vault by the time the menu appeared.
//
//  So LAN runs DLSS5.ini, which holds the two techniques DLSS needs and
//  nothing else: the kernel that produces motion vectors and the feed itself.
//  Every other effect is a pass at 1440p that DLSS then has to pay for, so a
//  colour grade on top of the neural pass costs frames twice. Switch to the
//  grading preset from ReShade's own Home tab when you want the look instead.
// ---------------------------------------------------------------------------
QString currentPresetPath(const QString &bin)
{
    QFile f(QDir(bin).filePath(QStringLiteral("ReShade.ini")));
    if (!f.open(QIODevice::ReadOnly))
        return QString();
    return iniGet(f.readAll(), QStringLiteral("GENERAL"), QStringLiteral("PresetPath"));
}

bool isLanPreset(const QString &presetPath)
{
    return presetPath.endsWith(kLanPresetName, Qt::CaseInsensitive);
}

// Written once, then left alone: whatever is tuned in it in game is the
// user's. Only the two techniques, in the order the feeder wants them.
bool ensureLanPreset(const QString &bin, const QString &lanVault, QString &error)
{
    const QString lanFile = QDir(bin).filePath(kLanPresetName);
    if (QFileInfo::exists(lanFile))
        return true;
    const QString parked = QDir(lanVault).filePath(kLanPresetName);
    if (QFileInfo::exists(parked) && QFile::copy(parked, lanFile))
        return true;
    const QByteArray body =
        "Techniques=" + kDlssTechniques.join(QLatin1Char(',')).toUtf8() + "\r\n"
        "TechniqueSorting=" + kDlssTechniques.join(QLatin1Char(',')).toUtf8() + "\r\n"
        "PreprocessorDefinitions=DLSS5_MV_PROVIDER=3\r\n"
        "\r\n"
        "[DLSS5_Feed.fx]\r\n"
        "PreprocessorDefinitions=DLSS5_MV_PROVIDER=3\r\n";
    QDir().mkpath(QFileInfo(lanFile).absolutePath());
    QFile out(lanFile);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate) || out.write(body) != body.size()) {
        error = QObject::tr("Could not write %1").arg(QDir::toNativeSeparators(lanFile));
        return false;
    }
    return true;
}

} // namespace

QolService::ReShadeMode activeReShadeMode(const AppSettings &s)
{
    QFile f(activeMarkerPath(s));
    if (f.open(QIODevice::ReadOnly)
        && QString::fromUtf8(f.readAll()).trimmed().startsWith(QLatin1String("lan"), Qt::CaseInsensitive))
        return ReShadeMode::Lan;
    return ReShadeMode::Online;
}

bool addonReShadeActive(const AppSettings &s)
{
    // Not what a marker file claims - what is actually in bin. The add-on
    // build is the only one of the two that carries no "limited add-on
    // functionality" refusal, and the two differ in size, which is all this
    // needs to tell them apart without reading 4 MB.
    QFile res(QStringLiteral(":/reshade/dxgi_addon.dll"));
    QFile have(QDir(s.plutoniumInstance).filePath(QStringLiteral("bin/dxgi.dll")));
    if (!res.open(QIODevice::ReadOnly) || !have.exists())
        return false;
    return have.size() == res.size();
}

bool dlssPayloadReady(const AppSettings &s)
{
    const QString lan = reShadeLanVault(s);
    for (const QString &f : kDlssRequired) {
        const QFileInfo fi(QDir(lan).filePath(f));
        if (!fi.exists() || fi.size() == 0)
            return false;
    }
    // The helper must have exactly the consumer this route configures.
    const QDir host(QDir(lan).filePath(kHostDir));
    return host.entryList({QStringLiteral("*.addon64")}, QDir::Files)
               == QStringList{QStringLiteral("renodx-dlss5.addon64")};
}

QString dlssPayloadSummary(const AppSettings &s)
{
    if (!dlssPayloadReady(s))
        return QObject::tr("Not installed yet.");
    qint64 bytes = 0;
    QDirIterator it(reShadeLanVault(s), QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        bytes += it.fileInfo().size();
    }
    const QString version = installedDlssVersion(s);
    return version.isEmpty() ? QObject::tr("imported by an older version, %1 MB").arg(bytes / 1000000)
                             : QObject::tr("version %1, %2 MB").arg(version).arg(bytes / 1000000);
}

QString installedDlssVersion(const AppSettings &s)
{
    QFile f(QDir(reShadeLanVault(s)).filePath(QStringLiteral("payload.json")));
    if (!f.open(QIODevice::ReadOnly)) return {};
    return QJsonDocument::fromJson(f.readAll()).object().value(QStringLiteral("version")).toString();
}

bool latestDlssRelease(DlssRelease &release, QString &error)
{
    const QByteArray raw = Downloader::downloadBytes(QStringLiteral(QOL_DLSS_RELEASE_URL), error, 30000);
    if (raw.isEmpty()) return false;
    const QJsonObject o = QJsonDocument::fromJson(raw).object();
    release.version = o.value(QStringLiteral("version")).toString();
    release.url = o.value(QStringLiteral("url")).toString();
    release.sha256 = o.value(QStringLiteral("sha256")).toString().toLower();
    release.size = static_cast<qint64>(o.value(QStringLiteral("size")).toDouble());
    static const QRegularExpression hash(QStringLiteral("^[0-9a-f]{64}$"));
    const QString prefix = QStringLiteral(QOL_DLSS_ASSET_PREFIX);
    if (!release.isValid() || !hash.match(release.sha256).hasMatch()
        || !release.url.startsWith(prefix) || release.url.contains(QLatin1Char('?'))) {
        error = QObject::tr("The published DLSS 5 release manifest is invalid.");
        return false;
    }
    return true;
}

namespace {

// Files the payload names but does not carry, and the only places they may
// come from. LumeniteFX's licence (AGNYA) forbids re-hosting a copy, and
// NVIDIA publishes its DLSS runtime in its own repository, so the payload
// carries each one's publisher URL and hash instead of the file.
const QStringList kRemoteOrigins = {
    QStringLiteral("https://raw.githubusercontent.com/umar-afzaal/LumeniteFX/"),
    QStringLiteral("https://raw.githubusercontent.com/NVIDIA/DLSS/"),
};

bool allowedRemote(const QString &url)
{
    if (url.contains(QLatin1Char('?')) || url.contains(QLatin1String("..")))
        return false;
    for (const QString &origin : kRemoteOrigins)
        if (url.startsWith(origin))
            return true;
    return false;
}

bool safeRelativePath(const QString &name)
{
    const QString clean = QDir::cleanPath(name);
    return !name.isEmpty() && clean == name && !clean.startsWith(QLatin1String("../"))
           && clean != QLatin1String("..") && !QDir::isAbsolutePath(name)
           && !name.contains(QLatin1Char('\\')) && !name.contains(QLatin1Char(':'));
}

bool fileMatches(const QString &path, const QJsonObject &item)
{
    const QFileInfo info(path);
    const QString digest = item.value(QStringLiteral("sha256")).toString().toLower();
    const qint64 size = static_cast<qint64>(item.value(QStringLiteral("size")).toDouble());
    return info.isFile() && !info.isSymLink() && size > 0 && info.size() == size
           && digest.size() == 64 && sha256Of(path) == digest;
}

bool verifyDlssTree(const QString &root, const QString &version, QString &error)
{
    QFile f(QDir(root).filePath(QStringLiteral("payload.json")));
    if (!f.open(QIODevice::ReadOnly)) {
        error = QObject::tr("The DLSS 5 archive has no file manifest.");
        return false;
    }
    const QJsonObject manifest = QJsonDocument::fromJson(f.readAll()).object();
    const QJsonObject files = manifest.value(QStringLiteral("files")).toObject();
    const QJsonObject remote = manifest.value(QStringLiteral("remote")).toObject();
    if (manifest.value(QStringLiteral("version")).toString() != version || files.isEmpty()) {
        error = QObject::tr("The DLSS 5 archive version or file list does not match its release.");
        return false;
    }
    for (const QString &name : kDlssRequired)
        if (!files.contains(name) && !remote.contains(name)) {
            error = QObject::tr("The DLSS 5 archive is missing %1.").arg(name);
            return false;
        }
    for (auto it = files.begin(); it != files.end(); ++it) {
        if (!safeRelativePath(it.key())) {
            error = QObject::tr("The DLSS 5 archive contains an unsafe path.");
            return false;
        }
        if (!fileMatches(QDir(root).filePath(it.key()), it.value().toObject())) {
            error = QObject::tr("The DLSS 5 archive failed verification at %1.").arg(it.key());
            return false;
        }
    }
    for (auto it = remote.begin(); it != remote.end(); ++it) {
        const QString url = it.value().toObject().value(QStringLiteral("url")).toString();
        if (!safeRelativePath(it.key()) || files.contains(it.key()) || !allowedRemote(url)) {
            error = QObject::tr("The DLSS 5 archive names a download it is not allowed to: %1.").arg(it.key());
            return false;
        }
    }
    QDirIterator all(root, QDir::Files, QDirIterator::Subdirectories);
    while (all.hasNext()) {
        const QString name = QDir(root).relativeFilePath(all.next()).replace(QLatin1Char('\\'), QLatin1Char('/'));
        if (name != QLatin1String("payload.json") && !files.contains(name)) {
            error = QObject::tr("The DLSS 5 archive contains an unexpected file: %1.").arg(name);
            return false;
        }
    }
    return true;
}

// LumeniteFX and NVIDIA's runtime, straight from their publishers, into the
// unpacked payload. Each file is checked against the hash the payload recorded
// for it, so a changed upstream file stops the install rather than shipping
// something nobody tested.
bool fetchRemoteFiles(const QString &root, const Progress &p, QString &error)
{
    QFile f(QDir(root).filePath(QStringLiteral("payload.json")));
    if (!f.open(QIODevice::ReadOnly)) {
        error = QObject::tr("The DLSS 5 archive has no file manifest.");
        return false;
    }
    const QJsonObject remote = QJsonDocument::fromJson(f.readAll()).object()
                                   .value(QStringLiteral("remote")).toObject();
    for (auto it = remote.begin(); it != remote.end(); ++it) {
        const QJsonObject item = it.value().toObject();
        const QString dest = QDir(root).filePath(it.key());
        const QString url = item.value(QStringLiteral("url")).toString();
        const QString name = QFileInfo(dest).fileName();
        if (p) p(QObject::tr("Downloading %1 from %2...").arg(name, QUrl(url).path().section(QLatin1Char('/'), 1, 2)), 80);
        bool ok = false;
        for (int attempt = 0; attempt < 2 && !ok; ++attempt) {
            QString why;
            ok = Downloader::downloadToFile(url, dest, why, nullptr, 10 * 60 * 1000) && fileMatches(dest, item);
            if (!ok) {
                QFile::remove(dest);
                error = why.isEmpty()
                            ? QObject::tr("%1 from %2 is not the version this payload was tested with.")
                                  .arg(name, url)
                            : QObject::tr("Could not download %1: %2").arg(name, why);
            }
        }
        if (!ok)
            return false;
    }
    return true;
}

// Windows has shipped tar.exe (bsdtar) since 1803, and it reads zip.
bool unzipWithTar(const QString &zip, const QString &dest, QString &error)
{
#ifdef Q_OS_WIN
    const QString tar = QDir(qEnvironmentVariable("SystemRoot", QStringLiteral("C:\\Windows")))
                            .filePath(QStringLiteral("System32/tar.exe"));
    if (!QFileInfo::exists(tar)) {
        error = QObject::tr("7-Zip is not installed and Windows has no tar.exe to unpack with.");
        return false;
    }
    QDir().mkpath(dest);
    QProcess proc;
    proc.setProcessChannelMode(QProcess::MergedChannels);
    proc.start(tar, {QStringLiteral("-xf"), QDir::toNativeSeparators(zip),
                     QStringLiteral("-C"), QDir::toNativeSeparators(dest)});
    if (!proc.waitForStarted(8000) || !proc.waitForFinished(10 * 60 * 1000)
        || proc.exitStatus() != QProcess::NormalExit || proc.exitCode() != 0) {
        error = QObject::tr("Could not unpack the DLSS 5 payload: %1")
                    .arg(QString::fromLocal8Bit(proc.readAll()).right(240));
        return false;
    }
    return true;
#else
    Q_UNUSED(zip); Q_UNUSED(dest);
    error = QObject::tr("DLSS 5 is Windows only.");
    return false;
#endif
}

// DLSS 5 Swapper, pointed at Plutonium's bin, installs the kernel at the top
// of Shaders\. Ours lives in LumeniteFX\, and two files of one name are two
// techniques of one name: the watchdog's duplicate sweep then quarantines
// ours as the deeper copy, and puts it back from the vault at the next game
// start, every session. Only that one file is taken; the rest of what the
// Swapper added is left for the Swapper to manage.
const QStringList kSwapperLeftovers = {
    QStringLiteral("reshade-shaders/Shaders/lumenite_Kernel.fx"),
};

// Add-ons someone else put in bin (DLSS 5 Swapper's own feeder, its overlay
// page). An online session must not carry them, and blocking the launch over
// a file the user cannot find is no answer, so they move to the state folder,
// kept rather than deleted.
void parkStrayAddons(const AppSettings &s, const QString &bin)
{
    const QString parked = QDir(stateDir(s)).filePath(QStringLiteral("parked-addons"));
    const QStringList addons = QDir(bin).entryList({QStringLiteral("*.addon"), QStringLiteral("*.addon32"),
                                                    QStringLiteral("*.addon64")}, QDir::Files);
    for (const QString &name : addons) {
        QDir().mkpath(parked);
        const QString to = QDir(parked).filePath(name);
        QFile::remove(to);
        if (!QFile::rename(QDir(bin).filePath(name), to))
            QFile::remove(QDir(bin).filePath(name));
    }
}

} // namespace

bool installDlssPayload(const AppSettings &s, const DlssRelease &release, const Progress &p, QString &error)
{
    if (!release.isValid()) { error = QObject::tr("The DLSS 5 release is invalid."); return false; }
    if (const QString g = runningGame(); !g.isEmpty()) {
        error = QObject::tr("Close Plutonium first (%1 is running).").arg(g);
        return false;
    }
    const QString bin = QDir(s.plutoniumInstance).filePath(QStringLiteral("bin"));
    if (!QDir(bin).exists()) {
        error = QObject::tr("Plutonium's bin folder was not found under %1.")
                    .arg(QDir::toNativeSeparators(s.plutoniumInstance));
        return false;
    }
    GameLauncher::stopReShadeWatchdog();

    // The archive must match the published manifest's size and sha256 before
    // anything is unpacked. One retry covers a transfer that arrived damaged.
    const QString work = tempDir(QStringLiteral("dlss5"));
    const auto cleanup = [&work]() { QDir(work).removeRecursively(); };
    const QString zip = QDir(work).filePath(QStringLiteral("pack.zip"));
    bool downloaded = false;
    for (int attempt = 1; attempt <= 2 && !downloaded; ++attempt) {
        if (p) p(attempt == 1 ? QObject::tr("Downloading DLSS 5 %1...").arg(release.version)
                              : QObject::tr("The download was damaged; trying again..."), 5);
        if (!Downloader::downloadToFile(release.url, zip, error, [&](qint64 got, qint64 total) {
                if (p) p(QObject::tr("Downloading DLSS 5 %1...").arg(release.version),
                         total > 0 ? int(5 + got * 65 / total) : 10);
            }, 30 * 60 * 1000)) {
            cleanup();
            return false;
        }
        if (p) p(QObject::tr("Checking the download..."), 70);
        downloaded = QFileInfo(zip).size() == release.size && sha256Of(zip) == release.sha256;
        if (!downloaded) {
            QFile::remove(zip);
            error = QObject::tr("The DLSS 5 download did not match its published checksum. "
                                "Check your connection and try again.");
        }
    }
    if (!downloaded) { cleanup(); return false; }
    // Unpacked with Windows' own tar.exe, so a PC without 7-Zip still
    // installs in one click. 7-Zip is only a fallback for a Windows old
    // enough to have no tar.
    const QString unpacked = QDir(work).filePath(QStringLiteral("unpacked"));
    if (!unzipWithTar(zip, unpacked, error)) {
        QDir(unpacked).removeRecursively();
        if (!ArchiveTool::hasSevenZip() || !ArchiveTool::extractToDirectory(zip, unpacked, &error)) {
            cleanup();
            return false;
        }
    }
    QFile::remove(zip);
    const QString root = packRoot(unpacked, QString());
    if (p) p(QObject::tr("Checking every DLSS 5 file..."), 76);
    if (!verifyDlssTree(root, release.version, error)) { cleanup(); return false; }
    if (!fetchRemoteFiles(root, p, error)) { cleanup(); return false; }

    const QString state = stateDir(s);
    QDir().mkpath(state);
    const QString stage = QDir(state).filePath(QStringLiteral("reshade-lan-stage"));
    const QString previous = QDir(state).filePath(QStringLiteral("reshade-lan-previous"));
    const QString current = reShadeLanVault(s);
    if (QDir(stage).exists()) QDir(stage).removeRecursively();
    // A previous update that died between its two renames. If the live vault
    // is gone the set-aside copy is the only payload there is, so it goes
    // back; otherwise it is stale and goes.
    if (QDir(previous).exists()) {
        if (!QDir(current).exists())
            QDir().rename(previous, current);
        else
            QDir(previous).removeRecursively();
    }
    QStringList written;
    if (!copyTree(root, stage, {}, written, error)
        || !writeResource(QStringLiteral(":/reshade/dxgi_addon.dll"),
                          QDir(stage).filePath(QStringLiteral("dxgi.dll")), error)) {
        QDir(stage).removeRecursively();
        cleanup();
        return false;
    }
    cleanup();

    if (p) p(QObject::tr("Installing the verified payload..."), 94);
    // Bin goes to its online state before the vault behind it changes, so no
    // half-old, half-new payload can ever be in bin at once. This also sweeps
    // out a stray install another tool left there.
    const bool hadReShade = reShadeInstalled(s);
    for (const QString &rel : kSwapperLeftovers)
        QFile::remove(QDir(bin).filePath(rel));
    if (hadReShade ? !applyReShadeMode(s, ReShadeMode::Online, false, error)
                   : !ensureReShadeAbsent(s, error)) {
        QDir(stage).removeRecursively();
        return false;
    }
    // The DLSS preset holds whatever was tuned in game; it is the user's and
    // survives an update. Taken only now: the switch above is what parks the
    // copy that was in bin into the old vault.
    const QString keptPreset = QDir(current).filePath(kLanPresetName);
    if (QFileInfo::exists(keptPreset))
        QFile::copy(keptPreset, QDir(stage).filePath(kLanPresetName));
    const bool hadCurrent = QDir(current).exists();
    if (hadCurrent && !QDir().rename(current, previous)) {
        error = QObject::tr("Could not set aside the current DLSS 5 payload.");
        QDir(stage).removeRecursively();
        return false;
    }
    if (!QDir().rename(stage, current)) {
        if (hadCurrent) QDir().rename(previous, current);
        error = QObject::tr("Could not activate the new DLSS 5 payload. The old one was kept.");
        return false;
    }
    if (hadCurrent) QDir(previous).removeRecursively();
    if (!dlssPayloadReady(s)) {
        error = QObject::tr("The DLSS 5 payload is incomplete after installing.");
        return false;
    }
    if (p) p(QObject::tr("DLSS 5 is ready for LAN play."), 100);
    return true;
}

bool removeDlssPayload(const AppSettings &s, QString &error)
{
    if (const QString g = runningGame(); !g.isEmpty()) {
        error = QObject::tr("Close Plutonium first (%1 is running).").arg(g);
        return false;
    }
    GameLauncher::stopReShadeWatchdog();
    const QString bin = QDir(s.plutoniumInstance).filePath(QStringLiteral("bin"));
    for (const QString &rel : kSwapperLeftovers)
        QFile::remove(QDir(bin).filePath(rel));
    if (reShadeInstalled(s)) {
        if (!applyReShadeMode(s, ReShadeMode::Online, false, error)) return false;
    } else if (!ensureReShadeAbsent(s, error)) {
        return false;
    }
    const QString vault = reShadeLanVault(s);
    if (QDir(vault).exists() && !QDir(vault).removeRecursively()) {
        error = QObject::tr("Could not remove the DLSS 5 payload from %1.").arg(vault);
        return false;
    }
    return onlineReShadeSafe(s, error);
}

bool onlineReShadeSafe(const AppSettings &s, QString &error)
{
    const QString bin = QDir(s.plutoniumInstance).filePath(QStringLiteral("bin"));
    if (addonReShadeActive(s)) {
        error = QObject::tr("The LAN ReShade runtime is still in Plutonium's bin folder.");
        return false;
    }
    for (const QString &rel : kLanOnlyFiles)
        if (QFileInfo::exists(QDir(bin).filePath(rel))) {
            error = QObject::tr("DLSS 5 is still present in Plutonium's bin folder: %1.").arg(rel);
            return false;
        }
    // Any add-on at all, whoever put it there. The stock build refuses them,
    // but an online session should not be carrying one either way.
    const QStringList addons = QDir(bin).entryList({QStringLiteral("*.addon"), QStringLiteral("*.addon32"),
                                                    QStringLiteral("*.addon64")}, QDir::Files);
    if (!addons.isEmpty()) {
        error = QObject::tr("A ReShade add-on is still in Plutonium's bin folder: %1.").arg(addons.first());
        return false;
    }
    if (QDir(QDir(bin).filePath(kHostDir)).exists() || QFileInfo::exists(QDir(bin).filePath(kLanPresetName))) {
        error = QObject::tr("The DLSS 5 helper or preset is still in Plutonium's bin folder.");
        return false;
    }
    return true;
}

bool applyReShadeMode(const AppSettings &s, ReShadeMode mode, bool dlss, QString &error)
{
    if (const QString g = runningGame(); !g.isEmpty()) {
        error = QObject::tr("Close Plutonium first (%1 is running).").arg(g);
        return false;
    }
    const QString bin = QDir(s.plutoniumInstance).filePath(QStringLiteral("bin"));
    if (!QDir(bin).exists()) {
        error = QObject::tr("Plutonium's bin folder was not found under %1.")
                    .arg(QDir::toNativeSeparators(s.plutoniumInstance));
        return false;
    }
    // The presets, the shader pack and the vault the watchdog restores from.
    // Both modes share them; only the runtime and the add-ons differ.
    if (!reShadeInstalled(s) || !QDir(reShadeVault(s)).exists())
        if (!installReShade(s, error))
            return false;

    const bool lan = mode == ReShadeMode::Lan;
    const bool withDlss = lan && dlss && dlssPayloadReady(s);
    if (lan && dlss && !withDlss) {
        error = QObject::tr("Install DLSS 5 from the Quality of Life page before starting LAN play.");
        return false;
    }

    const QString runtime = lan ? QStringLiteral(":/reshade/dxgi_addon.dll")
                                : QStringLiteral(":/reshade/dxgi.dll");
    if (!writeResource(runtime, QDir(bin).filePath(QStringLiteral("dxgi.dll")), error))
        return false;
    // The watchdog restores whatever Plutonium deletes from bin, and in LAN it
    // restores from the LAN vault. If that vault held the stock runtime, one
    // mid-session restore would quietly drop the session back to the build
    // that refuses add-ons - with the add-ons still sitting beside it.
    if (lan) {
        QDir().mkpath(reShadeLanVault(s));
        if (!writeResource(runtime, QDir(reShadeLanVault(s)).filePath(QStringLiteral("dxgi.dll")), error))
            return false;
    }

    if (withDlss) {
        for (const QString &rel : kSwapperLeftovers)
            QFile::remove(QDir(bin).filePath(rel));
        const QString lanVault = reShadeLanVault(s);
        QDirIterator it(lanVault, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            it.next();
            const QString rel = QDir(lanVault).relativeFilePath(it.filePath());
            if (rel == QLatin1String("payload.json") || rel == QLatin1String("dxgi.dll"))
                continue;            // the manifest, and the runtime we just wrote
            // The payload carries ReShade's two headers for a PC that has no
            // shader pack. Where a pack is installed its own copies stay: its
            // other effects were written against them.
            if (rel.endsWith(QLatin1String(".fxh")) && !rel.contains(QLatin1String("LumeniteFX"))
                && QFileInfo::exists(QDir(bin).filePath(rel)))
                continue;
            if (!copyIfDifferent(it.filePath(), QDir(bin).filePath(rel), error))
                return false;
        }
    } else {
        for (const QString &rel : kLanOnlyFiles) {
            const QString path = QDir(bin).filePath(rel);
            if (QFileInfo::exists(path) && !QFile::remove(path)) {
                error = QObject::tr("Could not remove %1 before launch.").arg(path);
                return false;
            }
        }
        const QString host = QDir(bin).filePath(kHostDir);
        if (QDir(host).exists() && !QDir(host).removeRecursively()) {
            error = QObject::tr("Could not remove the DLSS 5 helper before launch.");
            return false;
        }
        if (!lan)
            parkStrayAddons(s, bin);
        parkLanPresets(bin, reShadeLanVault(s));
        // Put the shader pack's own kernel back over the feeder's build.
        const QString kernel = QStringLiteral("reshade-shaders/Shaders/LumeniteFX/lumenite_Kernel.fx");
        const QString fromVault = QDir(reShadeVault(s)).filePath(kernel);
        if (QFileInfo::exists(fromVault) && !copyIfDifferent(fromVault, QDir(bin).filePath(kernel), error))
            return false;
    }

    // ReShade.ini: the keys that decide whether an add-on is looked for at all.
    // DisabledAddons carries Generic Depth in the shipped config, and without
    // Generic Depth the feeder gets no depth buffer and the neural pass is
    // blind. Remembered on the way in, restored on the way out.
    const QString ini = QDir(bin).filePath(QStringLiteral("ReShade.ini"));
    const QString memo = QDir(stateDir(s)).filePath(QStringLiteral("reshade-ini-before-lan.txt"));
    if (QFileInfo::exists(ini)) {
        QFile f(ini);
        QString disabledBefore;
        if (f.open(QIODevice::ReadOnly)) {
            disabledBefore = iniGet(f.readAll(), QStringLiteral("ADDON"), QStringLiteral("DisabledAddons"));
            f.close();
        }
        // Which preset the session plays on. LAN runs DLSS5.ini, which holds
        // the neural pass and nothing else; online goes back to whatever was
        // loaded before. The user's own preset is never opened either way.
        const QString presetNow = currentPresetPath(bin);
        const QString presetMemo = QDir(stateDir(s)).filePath(QStringLiteral("reshade-preset-before-lan.txt"));
        QString presetTarget = presetNow;
        if (withDlss) {
            if (!isLanPreset(presetNow) && !presetNow.isEmpty()) {
                QFile m(presetMemo);
                if (m.open(QIODevice::WriteOnly | QIODevice::Truncate))
                    m.write(presetNow.toUtf8());
            }
            presetTarget = QStringLiteral(".\\") + kLanPresetName;
            for (const QString &old : QDir(bin).entryList({QLatin1Char('*') + kOldLanPresetTag}, QDir::Files)) {
                QFile::remove(QDir(bin).filePath(old));
                QFile::remove(QDir(reShadeLanVault(s)).filePath(old));
            }
            if (!ensureLanPreset(bin, reShadeLanVault(s), error))
                return false;
            // Keep a copy where the watchdog restores from, so a Plutonium
            // wipe mid-session cannot leave the session with no preset.
            QString ignored;
            copyIfDifferent(QDir(bin).filePath(kLanPresetName),
                            QDir(reShadeLanVault(s)).filePath(kLanPresetName), ignored);
        } else if (isLanPreset(presetNow)) {
            QFile m(presetMemo);
            QString back;
            if (m.open(QIODevice::ReadOnly))
                back = QString::fromUtf8(m.readAll()).trimmed();
            presetTarget = back.isEmpty() ? QStringLiteral(".\\Cinematic Colour Grading.ini") : back;
        }

        QList<std::function<QByteArray(const QByteArray &)>> edits;
        edits << [presetTarget](const QByteArray &t) {
            return presetTarget.isEmpty()
                       ? t
                       : iniSet(t, QStringLiteral("GENERAL"), QStringLiteral("PresetPath"), presetTarget);
        };
        if (withDlss) {
            if (!QFileInfo::exists(memo)) {
                QFile m(memo);
                if (m.open(QIODevice::WriteOnly | QIODevice::Truncate))
                    m.write(disabledBefore.toUtf8());
            }
            // DLSS5.ini enables two techniques, so two effects are all that
            // need compiling. ReShade does not keep this setting on its own -
            // it rewrites its config on exit, and a 0 in there means all 473
            // effects in the pack compile on every launch: 90 seconds of
            // black screen and a page of errors from shaders nothing uses.
            edits << [](const QByteArray &t) { return iniSet(t, QStringLiteral("GENERAL"), QStringLiteral("SkipLoadingDisabledEffects"), QStringLiteral("1")); }
                  << [](const QByteArray &t) { return iniSet(t, QStringLiteral("ADDON"), QStringLiteral("AddonPath"), QStringLiteral(".\\")); }
                  << [](const QByteArray &t) { return iniSet(t, QStringLiteral("ADDON"), QStringLiteral("DisabledAddons"), QString()); }
                  << [](const QByteArray &t) {
                         return iniSet(t, QStringLiteral("GENERAL"), QStringLiteral("PreprocessorDefinitions"),
                                       mergeTokens(iniGet(t, QStringLiteral("GENERAL"), QStringLiteral("PreprocessorDefinitions")),
                                                   {QStringLiteral("DLSS5_MV_PROVIDER=3")}, {}));
                     };
        } else {
            QString restore;
            QFile m(memo);
            if (m.open(QIODevice::ReadOnly)) {
                restore = QString::fromUtf8(m.readAll()).trimmed();
                m.close();
            }
            // AddonPath goes back to empty too. The stock build would only
            // search and refuse, but an online config that still reads like a
            // LAN one is the kind of difference that gets blamed later.
            edits << [restore](const QByteArray &t) { return iniSet(t, QStringLiteral("ADDON"), QStringLiteral("DisabledAddons"), restore); }
                  << [](const QByteArray &t) { return iniSet(t, QStringLiteral("ADDON"), QStringLiteral("AddonPath"), QString()); }
                  << [](const QByteArray &t) {
                         return iniSet(t, QStringLiteral("GENERAL"), QStringLiteral("PreprocessorDefinitions"),
                                       mergeTokens(iniGet(t, QStringLiteral("GENERAL"), QStringLiteral("PreprocessorDefinitions")),
                                                   {}, {QStringLiteral("DLSS5_MV_PROVIDER=3")}));
                     };
        }
        if (!rewriteIni(ini, edits, error))
            return false;
    }

    // The feeder's own config, and the reason DLSS off is a setting here
    // rather than a deletion.
    //
    // DLSS 5 Swapper can have this same folder registered as one of its
    // games - on this PC it has bin\host64 - and it puts its payload back
    // within seconds of anything removing it. Two applications owning one
    // folder is not a fight worth having, so the switch that decides whether
    // DLSS runs is the feeder's own `enabled` key. Files may come back; the
    // pass does not.
    //
    // host_window is the expensive one. With the DLSS 5 panel cast into the
    // game the helper renders its whole ReShade overlay into a 900x1352
    // texture every frame and the game composites it: measured here that is
    // about 28 ms of CPU stall per frame on top of the neural pass. The panel
    // is for changing settings, not for playing behind, so it starts hidden
    // and F10 brings it up.
    if (withDlss)
        setFeederKeys(bin, {{QStringLiteral("enabled"), QStringLiteral("1")},
                            {QStringLiteral("host_window"), QStringLiteral("0")},
                            {QStringLiteral("cast_key"), QStringLiteral("121")}});

    // Three states, not two. The watchdog restores whatever Plutonium clears
    // out of bin, and the LAN vault holds both the add-on runtime (wanted by
    // every LAN session) and the DLSS payload (wanted only when DLSS is on).
    // With a single "lan" marker it put the feeder and the 64-bit helper
    // straight back into a session that had deliberately left them out.
    QFile marker(activeMarkerPath(s));
    if (marker.open(QIODevice::WriteOnly | QIODevice::Truncate))
        marker.write(withDlss ? "lan-dlss" : (lan ? "lan" : "online"));

    return lan || onlineReShadeSafe(s, error);
}

bool leaveLanState(const AppSettings &s, QString &error)
{
    if (activeReShadeMode(s) != ReShadeMode::Lan)
        return true;
    return reShadeInstalled(s) ? applyReShadeMode(s, ReShadeMode::Online, false, error)
                               : ensureReShadeAbsent(s, error);
}

bool ensureReShadeAbsent(const AppSettings &s, QString &error)
{
    // Refuse before touching anything. This used to sweep the add-ons and the
    // 64-bit helper out of bin and only then discover it could not delete
    // dxgi.dll because the game held it open - leaving a session with the
    // add-on runtime loaded and nothing for it to load.
    if (const QString g = runningGame(); !g.isEmpty()) {
        error = QObject::tr("Close Plutonium first (%1 is running).").arg(g);
        return false;
    }
    const QString bin = QDir(s.plutoniumInstance).filePath(QStringLiteral("bin"));
    for (const QString &rel : kLanOnlyFiles)
        QFile::remove(QDir(bin).filePath(rel));
    QDir(QDir(bin).filePath(kHostDir)).removeRecursively();
    parkStrayAddons(s, bin);
    parkLanPresets(bin, reShadeLanVault(s));
    QFile::remove(activeMarkerPath(s));
    if (!reShadeInstalled(s) && !QFileInfo::exists(QDir(bin).filePath(QStringLiteral("dlss5-feed.addon32"))))
        return true;
    return removeReShade(s, error);
}

} // namespace QolService
