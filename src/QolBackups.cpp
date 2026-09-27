#include "QolBackups.h"
#include "AppSettings.h"
#include "QolService.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QMap>
#include <QRegularExpression>
#include <QSet>
#include <QStorageInfo>
#include <QTextStream>

namespace {

// One part of a backup: a whole folder, or a named handful of loose files
// (plus anything matching `glob`) from one folder. Each part lands in its own
// named subfolder so a restore knows exactly where it goes, and a person
// reading the tree can tell too.
struct Part {
    QString sub;             // subfolder inside backups\<kind>
    QString path;            // where the files live
    bool folder = false;     // true: everything under path, recursively
    QStringList items;       // folder == false: these names
    QString glob;            // folder == false: and these
    QStringList skip;        // folder == true: top-level entries left out
    QStringList notOurs;     // install manifests whose files are ours, not the player's
};

struct Set {
    QString label, title, desc;
    QList<Part> parts;
};

// The glyphs any controller pack can replace. The icon backup takes only
// these, not the whole images folder: the textures backup already covers that
// folder whole, and two copies of the same gigabyte would help nobody.
const QStringList kIconNames = {
    "xenon_controller_top.iwi",
    "xenonbutton_a.iwi", "xenonbutton_b.iwi", "xenonbutton_x.iwi", "xenonbutton_y.iwi",
    "xenonbutton_back.iwi", "xenonbutton_start.iwi", "xenonbutton_lb.iwi", "xenonbutton_rb.iwi",
    "xenonbutton_lt.iwi", "xenonbutton_rt.iwi", "xenonbutton_ls.iwi", "xenonbutton_rs.iwi",
    "xenonbutton_dpad_all.iwi", "xenonbutton_dpad_up.iwi", "xenonbutton_dpad_down.iwi",
    "xenonbutton_dpad_left.iwi", "xenonbutton_dpad_right.iwi", "xenonbutton_dpad_ud.iwi",
    "xenonbutton_dpad_rl.iwi", "hud_dpad_blood.iwi"};

const QStringList kReShadeNames = {"ReShade.ini", "Cinematic Colour Grading.ini", "BO2.ini",
                                   "BO1.ini", "MW3.ini", "WAW.ini", "dxgi.dll"};

// Settings, never stats. The game writes its stats into the same folders, and
// this app does not own anyone's progress, so a settings backup names what it
// saves rather than taking the folder. That is also what lets repairAaSamples
// reach into the backup copy of plutonium_zm.cfg.
const QStringList kModSettings = {"plutonium_zm.cfg", "bindings_zm.bdg", "hardware_zm.chp",
                                  "user_zm.cgp", "user_common.cgp"};
const QStringList kGameSettings = {"plutonium_zm.cfg", "plutonium_mp.cfg", "bindings_zm.bdg",
                                   "bindings_mp.bdg", "hardware_zm.chp", "hardware_mp.chp",
                                   "user_zm.cgp", "user_mp.cgp", "user_common.cgp"};

QString t6(const AppSettings &s) { return QolService::t6Storage(s); }
QString under(const QString &root, const QString &rel) { return QDir::cleanPath(QDir(root).filePath(rel)); }

QMap<QString, Set> sets(const AppSettings &s)
{
    const QString storage = t6(s);
    const QString images = under(storage, "images");
    const QString bin = under(s.plutoniumInstance, "bin");
    QMap<QString, Set> m;
    // The texture and sound backups leave out the files our own packs put
    // there. Those can be downloaded again at any time; the backup is for what
    // was the player's, and a restore should hand back exactly that.
    m["images"] = {QObject::tr("My textures"), QObject::tr("your textures"),
                   QObject::tr("Everything in storage\\t6\\images that this app did not put there. "
                               "The HD texture pack writes here."),
                   {{"images", images, true, {}, {}, {}, {"images", "controller"}}}};
    m["zone"] = {QObject::tr("My sounds"), QObject::tr("your sounds"),
                 QObject::tr("Everything in storage\\t6\\zone that this app did not put there. "
                             "The custom sound pack writes here."),
                 {{"zone", under(storage, "zone"), true, {}, {}, {}, {"zone"}}}};
    m["controller"] = {QObject::tr("My controller icons"), QObject::tr("your controller icons"),
                       QObject::tr("The button-prompt images a controller icon pack replaces."),
                       {{"controller", images, false, kIconNames, {}, {}, {"controller"}}}};
    // Glob *.ini catches a preset saved under the player's own name, which the
    // fixed list cannot know in advance.
    m["reshade"] = {QObject::tr("My ReShade setup"), QObject::tr("your ReShade setup"),
                    QObject::tr("ReShade's dxgi.dll, every preset .ini and the reshade-shaders folder "
                                "in Plutonium's bin."),
                    {{"bin", bin, false, kReShadeNames, "*.ini"},
                     {"reshade-shaders", under(bin, "reshade-shaders"), true}}};
    m["mod"] = {QObject::tr("The mod + my settings"), QObject::tr("the mod and its settings"),
                QObject::tr("The Quality of Life mod folder and your saved menu settings for it."),
                {{"files", under(storage, "mods/zm_qol"), true},
                 {"settings", under(storage, "players/mods/zm_qol"), false, kModSettings}}};
    // Both places Plutonium loads loose scripts from: scripts\ (GSC mods
    // dropped in by hand) and raw\ (scripts, menus and maps, where Zombies
    // Declassified and other script packs install). The game runs them on
    // every map whatever mod is picked, which is exactly why they need to be
    // easy to park and put back.
    m["scripts"] = {QObject::tr("My scripts"), QObject::tr("your scripts"),
                    QObject::tr("Everything in storage\\t6\\scripts and storage\\t6\\raw: loose GSC "
                                "scripts, menus and maps."),
                    {{"scripts", under(storage, "scripts"), true},
                     {"raw", under(storage, "raw"), true}}};
    m["settings"] = {QObject::tr("My game settings"), QObject::tr("your game settings"),
                     QObject::tr("Plutonium's configs, key bindings and graphics settings for Zombies "
                                 "and Multiplayer, and your Quality of Life menu settings. Never your stats."),
                     {{"players", under(storage, "players"), false, kGameSettings},
                      {"mod-settings", under(storage, "players/mods/zm_qol"), false, kModSettings}}};
    // Every other mod and custom map. Can run to gigabytes, so it is only
    // ever backed up when asked, never before an install.
    m["mods"] = {QObject::tr("My other mods and maps"), QObject::tr("your other mods and maps"),
                 QObject::tr("Every mod in storage\\t6\\mods except Quality of Life, and every custom "
                             "map in storage\\t6\\usermaps. Can be large, so it is never automatic."),
                 {{"mods", under(storage, "mods"), true, {}, {}, {"zm_qol"}},
                  {"usermaps", under(storage, "usermaps"), true}}};
    return m;
}

QString manifestFile(const AppSettings &s, const QString &kind)
{
    return QDir(QolService::stateDir(s)).filePath(QStringLiteral("installed-%1.txt").arg(kind));
}

QStringList readLines(const QString &path)
{
    QStringList out;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return out;
    QTextStream in(&f);
    while (!in.atEnd()) {
        const QString line = in.readLine().trimmed();
        if (!line.isEmpty())
            out << QDir::fromNativeSeparators(line);
    }
    return out;
}

QSet<QString> oursIn(const AppSettings &s, const Part &part)
{
    QSet<QString> out;
    for (const QString &k : part.notOurs)
        for (const QString &rel : readLines(manifestFile(s, k)))
            out.insert(rel.toLower());
    return out;
}

struct FileRef { QString path; QString rel; qint64 size = 0; };

QList<FileRef> partFiles(const AppSettings &s, const Part &part)
{
    QList<FileRef> out;
    const QSet<QString> ours = oursIn(s, part);
    if (part.folder) {
        const QDir dir(part.path);
        if (!dir.exists())
            return out;
        QDirIterator it(part.path, QDir::Files | QDir::Hidden, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            it.next();
            const QString rel = dir.relativeFilePath(it.filePath());
            if (!part.skip.isEmpty()
                && part.skip.contains(rel.section(QLatin1Char('/'), 0, 0), Qt::CaseInsensitive))
                continue;
            if (ours.contains(rel.toLower()))
                continue;
            out << FileRef{it.filePath(), rel, it.fileInfo().size()};
        }
        return out;
    }
    QSet<QString> seen;
    auto take = [&](const QFileInfo &fi) {
        const QString key = fi.fileName().toLower();
        if (!fi.isFile() || seen.contains(key) || ours.contains(key))
            return;
        seen.insert(key);
        out << FileRef{fi.filePath(), fi.fileName(), fi.size()};
    };
    for (const QString &n : part.items)
        take(QFileInfo(QDir(part.path).filePath(n)));
    if (!part.glob.isEmpty())
        for (const QFileInfo &fi : QDir(part.path).entryInfoList({part.glob}, QDir::Files))
            take(fi);
    return out;
}

void appendLog(const AppSettings &s, const QString &text)
{
    const QString dir = QolService::stateDir(s);
    QDir().mkpath(dir);
    QFile f(QDir(dir).filePath(QStringLiteral("installer.log")));
    if (f.open(QIODevice::Append | QIODevice::Text))
        QTextStream(&f) << QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"))
                        << "  info   " << text << '\n';
}

QString fmtSize(qint64 b)
{
    if (b >= 1000LL * 1000 * 1000) return QStringLiteral("%1 GB").arg(b / 1e9, 0, 'f', 1);
    if (b >= 1000 * 1000)          return QStringLiteral("%1 MB").arg(b / 1e6, 0, 'f', 0);
    if (b >= 1000)                 return QStringLiteral("%1 KB").arg(b / 1e3, 0, 'f', 0);
    return QStringLiteral("%1 bytes").arg(b);
}

QDateTime takenAt(const QString &folder)
{
    // Birth time of the backup folder, which is when the backup was made.
    // Copied files keep their own dates, so they cannot say when.
    const QFileInfo fi(folder);
    return fi.birthTime().isValid() ? fi.birthTime() : fi.lastModified();
}

bool copyFile(const QString &from, const QString &to, QString &error)
{
    QDir().mkpath(QFileInfo(to).absolutePath());
    if (QFileInfo::exists(to)) {
        QFile::setPermissions(to, QFile::permissions(to) | QFileDevice::WriteOwner);
        if (!QFile::remove(to)) {
            error = QObject::tr("%1 is in use. Close Plutonium and try again.").arg(QDir::toNativeSeparators(to));
            return false;
        }
    }
    if (!QFile::copy(from, to)) {
        error = QObject::tr("Could not copy %1 to %2").arg(QDir::toNativeSeparators(from), QDir::toNativeSeparators(to));
        return false;
    }
    return true;
}

// Drops restored paths from an install manifest. A file that came back from a
// backup is the player's again, and the manifest is the list a Remove deletes,
// so leaving it there would delete their file on the next Remove. A manifest
// left with nothing in it means nothing of that pack remains.
void forgetFromManifest(const AppSettings &s, const QString &kind, const QStringList &rels)
{
    const QString path = manifestFile(s, kind);
    if (rels.isEmpty() || !QFileInfo::exists(path))
        return;
    QSet<QString> back;
    for (const QString &r : rels)
        back.insert(r.toLower());
    const QStringList before = readLines(path);
    QStringList keep;
    for (const QString &line : before)
        if (!back.contains(line.toLower()))
            keep << line;
    if (keep.size() == before.size())
        return;
    if (keep.isEmpty()) {
        QFile::remove(path);
        if (kind == QLatin1String("controller"))
            QFile::remove(QDir(QolService::stateDir(s)).filePath(QStringLiteral("controller-pack.txt")));
        return;
    }
    QFile f(path);
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        QTextStream out(&f);
        for (const QString &k : keep)
            out << k << '\n';
    }
}

} // namespace

namespace QolBackups {

QStringList kinds()
{
    // Page order: the ones an install writes over first.
    return {"images", "zone", "controller", "reshade", "mod", "scripts", "settings", "mods"};
}

QStringList automaticKinds()
{
    // Not "mod": that is our own mod, which can be downloaded again, and a
    // first-install copy of it kept forever would be 400 MB of nothing. The
    // player's menu settings for it are in "settings".
    return {"images", "zone", "controller", "reshade", "scripts", "settings"};
}

QString root(const AppSettings &s) { return QDir(t6(s)).filePath(QStringLiteral("backups")); }

bool exists(const AppSettings &s, const QString &kind)
{
    return QDir(QDir(root(s)).filePath(kind)).exists();
}

Info info(const AppSettings &s, const QString &kind, bool measure)
{
    const auto all = sets(s);
    Info out;
    out.kind = kind;
    if (!all.contains(kind))
        return out;
    const Set &set = all[kind];
    out.label = set.label;
    out.title = set.title;
    out.desc = set.desc;
    out.folder = QDir(root(s)).filePath(kind);
    out.exists = QDir(out.folder).exists();
    if (out.exists)
        out.when = takenAt(out.folder);
    if (!measure)
        return out;
    for (const Part &part : set.parts)
        for (const FileRef &f : partFiles(s, part)) {
            ++out.liveFiles;
            out.liveBytes += f.size;
        }
    if (out.exists) {
        QDirIterator it(out.folder, QDir::Files | QDir::Hidden, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            it.next();
            ++out.files;
            out.bytes += it.fileInfo().size();
        }
    }
    return out;
}

Result backup(const AppSettings &s, const QString &kind, bool replace, const Progress &p, QString &message)
{
    const auto all = sets(s);
    if (!all.contains(kind)) {
        message = QObject::tr("Unknown backup kind \"%1\".").arg(kind);
        return Result::Failed;
    }
    const Set &set = all[kind];
    const QString dest = QDir(root(s)).filePath(kind);
    // Checked before anything is measured: this runs before every install,
    // and walking a gigabyte of textures only to keep the old copy is waste.
    if (QDir(dest).exists() && !replace) {
        message = QObject::tr("A backup of %1 already exists from %2 - kept it. That is the older copy, "
                              "so it is the one worth having.")
                      .arg(set.title, takenAt(dest).toString(QStringLiteral("d MMM yyyy HH:mm")));
        return Result::KeptOlder;
    }

    QList<QPair<Part, QList<FileRef>>> plan;
    int total = 0;
    qint64 bytes = 0;
    for (const Part &part : set.parts) {
        const QList<FileRef> files = partFiles(s, part);
        total += files.size();
        for (const FileRef &f : files) bytes += f.size;
        plan << qMakePair(part, files);
    }
    if (total == 0) {
        message = QObject::tr("Nothing to back up for %1 - there are no files of yours there yet.").arg(set.title);
        return Result::NothingToSave;
    }
    // Room check up front. A backup that runs the disk dry half-way through
    // helps nobody and can take the install down with it.
    QDir().mkpath(root(s));
    const QStorageInfo disk(root(s));
    if (disk.isValid() && disk.bytesAvailable() > 0 && disk.bytesAvailable() < bytes + 64LL * 1024 * 1024) {
        message = QObject::tr("Not enough free space to back up %1: it needs %2 and %3 is free.")
                      .arg(set.title, fmtSize(bytes), fmtSize(disk.bytesAvailable()));
        return Result::Failed;
    }

    // Built beside the old copy and swapped in only when whole, so a copy
    // that fails half-way never costs the backup that was already there.
    const QString staging = dest + QStringLiteral(".new");
    QDir(staging).removeRecursively();
    int done = 0;
    for (const auto &step : plan) {
        const QString to = QDir(staging).filePath(step.first.sub);
        for (const FileRef &f : step.second) {
            QString err;
            if (!copyFile(f.path, QDir(to).filePath(f.rel), err)) {
                QDir(staging).removeRecursively();
                message = QObject::tr("Backup of %1 failed - nothing was changed. %2").arg(set.title, err);
                return Result::Failed;
            }
            ++done;
            if (p && (done % 25 == 0 || done == total))
                p(QObject::tr("Backing up %1 (%2 of %3)").arg(set.title).arg(done).arg(total),
                  int(done * 100LL / total));
        }
    }
    const QString old = dest + QStringLiteral(".old");
    QDir(old).removeRecursively();
    if (QDir(dest).exists() && !QDir().rename(dest, old)) {
        QDir(staging).removeRecursively();
        message = QObject::tr("Could not replace the old backup in %1. Close anything that has it open.")
                      .arg(QDir::toNativeSeparators(dest));
        return Result::Failed;
    }
    if (!QDir().rename(staging, dest)) {
        if (QDir(old).exists())
            QDir().rename(old, dest);
        QDir(staging).removeRecursively();
        message = QObject::tr("Could not finish the backup in %1.").arg(QDir::toNativeSeparators(dest));
        return Result::Failed;
    }
    QDir(old).removeRecursively();
    appendLog(s, QStringLiteral("backup: %1 -> %2 (%3 files)").arg(kind, dest).arg(total));
    message = QObject::tr("Backed up %1 - %2, %3, to %4.")
                  .arg(set.title, total == 1 ? QObject::tr("1 file") : QObject::tr("%1 files").arg(total),
                       fmtSize(bytes), QDir::toNativeSeparators(dest));
    if (kind == QLatin1String("mod") || kind == QLatin1String("settings"))
        repairAaSamples(s);
    return Result::Saved;
}

bool backupBeforeInstall(const AppSettings &s, const QStringList &required, const QStringList &extra,
                         const Progress &p, QString &error)
{
    for (const QString &k : required) {
        QString msg;
        if (backup(s, k, false, p, msg) == Result::Failed) {
            error = QObject::tr("%1\n\nThe install was stopped so none of your files are overwritten "
                                "without a copy. Free some space, or untick \"Back up my files before "
                                "every install\" under Backups to install anyway.").arg(msg);
            return false;
        }
    }
    for (const QString &k : extra) {
        if (required.contains(k))
            continue;
        QString msg;
        if (backup(s, k, false, p, msg) == Result::Failed)
            appendLog(s, QStringLiteral("backup before install skipped: %1 - %2").arg(k, msg));
    }
    return true;
}

bool restore(const AppSettings &s, const QString &kind, const Progress &p, QString &error)
{
    if (const QString g = QolService::runningGame(); !g.isEmpty()) {
        error = QObject::tr("Close Plutonium first (%1 is running). The game keeps these files open.").arg(g);
        return false;
    }
    const auto all = sets(s);
    if (!all.contains(kind)) {
        error = QObject::tr("Unknown backup kind \"%1\".").arg(kind);
        return false;
    }
    const Set &set = all[kind];
    const QString dir = QDir(root(s)).filePath(kind);
    if (!QDir(dir).exists()) {
        error = QObject::tr("There is no backup of %1 to put back.").arg(set.title);
        return false;
    }
    // Take the sting out of an old settings backup before it goes live.
    if (kind == QLatin1String("mod") || kind == QLatin1String("settings"))
        repairAaSamples(s);

    // Where each file in the backup goes. Folder parts keep their tree; loose
    // parts go back flat, exactly where they were taken from.
    struct Move { QString from, to, rel; const Part *part; };
    QList<Move> moves;
    for (const Part &part : set.parts) {
        const QString from = QDir(dir).filePath(part.sub);
        QDirIterator it(from, QDir::Files | QDir::Hidden, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            it.next();
            const QString rel = part.folder ? QDir(from).relativeFilePath(it.filePath()) : it.fileName();
            moves << Move{it.filePath(), QDir(part.path).filePath(rel), rel, &part};
        }
    }
    // Versions 2.0 to 2.2 of this app kept the ReShade backup flat, straight
    // in backups\reshade rather than in its bin subfolder. Those go to bin.
    if (kind == QLatin1String("reshade"))
        for (const QFileInfo &fi : QDir(dir).entryInfoList(QDir::Files))
            moves << Move{fi.filePath(), QDir(set.parts.first().path).filePath(fi.fileName()),
                          fi.fileName(), &set.parts.first()};
    if (moves.isEmpty()) {
        error = QObject::tr("That backup is empty - there is nothing to put back.");
        return false;
    }

    int done = 0;
    QStringList imagesBack, zoneBack, binBack;
    for (const Move &m : moves) {
        if (!copyFile(m.from, m.to, error)) {
            error = QObject::tr("Putting back %1 stopped part-way: %2").arg(set.title, error);
            return false;
        }
        const QString path = m.part->path;
        if (path.endsWith(QLatin1String("/images"))) imagesBack << m.rel;
        else if (path.endsWith(QLatin1String("/zone"))) zoneBack << m.rel;
        else if (path.endsWith(QLatin1String("/bin"))) binBack << m.rel;
        ++done;
        if (p && (done % 25 == 0 || done == moves.size()))
            p(QObject::tr("Putting back %1 (%2 of %3)").arg(set.title).arg(done).arg(moves.size()),
              int(done * 100LL / moves.size()));
    }
    forgetFromManifest(s, QStringLiteral("images"), imagesBack);
    forgetFromManifest(s, QStringLiteral("controller"), imagesBack);
    forgetFromManifest(s, QStringLiteral("zone"), zoneBack);
    forgetFromManifest(s, QStringLiteral("reshade"), binBack);
    appendLog(s, QStringLiteral("restore: %1 (%2 files)").arg(kind).arg(done));
    return true;
}

bool remove(const AppSettings &s, const QString &kind, QString &error)
{
    const QString dir = QDir(root(s)).filePath(kind);
    if (!QDir(dir).exists())
        return true;
    if (!QDir(dir).removeRecursively()) {
        error = QObject::tr("Could not delete %1. Close anything that has it open.").arg(QDir::toNativeSeparators(dir));
        return false;
    }
    appendLog(s, QStringLiteral("backup deleted: %1").arg(kind));
    return true;
}

int repairAaSamples(const AppSettings &s)
{
    static const QRegularExpression re(QStringLiteral("(?m)^([ \\t]*seta[ \\t]+r_aaSamples[ \\t]+)\"?(-?\\d+)\"?"));
    const QString storage = t6(s);
    const QString bk = root(s);
    int fixed = 0;
    for (const QString &path : {under(storage, "players/mods/zm_qol/plutonium_zm.cfg"),
                                under(bk, "mod/settings/plutonium_zm.cfg"),
                                under(storage, "players/plutonium_zm.cfg"),
                                under(bk, "settings/players/plutonium_zm.cfg"),
                                under(bk, "settings/mod-settings/plutonium_zm.cfg")}) {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly))
            continue;
        const QByteArray raw = f.readAll();
        f.close();
        const QString text = QString::fromUtf8(raw);
        const QRegularExpressionMatch m = re.match(text);
        if (!m.hasMatch())
            continue;
        const int v = m.captured(2).toInt();
        if (v == 1 || v == 2 || v == 4 || v == 8)
            continue;
        // Only that one line changes; every other byte, line endings included,
        // is written back as it was read.
        QString out = text;
        out.replace(m.capturedStart(0), m.capturedLength(0), m.captured(1) + QStringLiteral("\"4\""));
        const QByteArray bytes = out.toUtf8();
        if (f.open(QIODevice::WriteOnly | QIODevice::Truncate) && f.write(bytes) == bytes.size()) {
            ++fixed;
            appendLog(s, QStringLiteral("repaired r_aaSamples %1 -> 4 in %2").arg(v).arg(path));
        }
    }
    return fixed;
}

} // namespace QolBackups
