#include "UpdateService.h"
#include "VersionCompare.h"
#include "AppSettings.h"
#include "ArchiveTool.h"
#include "Downloader.h"
#include "ProgressDialog.h"
#include "SelfUpdate.h"
#include "Version.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QAbstractItemView>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QProcess>
#include <QRegularExpression>
#include <QUrl>
#include <QPushButton>
#include <QSettings>
#include <QThread>
#include <QVBoxLayout>
#include <QVersionNumber>
#include <QtConcurrent>

namespace {

QString normVer(QString v)
{
    v = v.trimmed();
    if (v.startsWith(QLatin1Char('v')) || v.startsWith(QLatin1Char('V')))
        v = v.mid(1);
    return v;
}

int cmpVer(const QString &a, const QString &b)
{
    const QVersionNumber va = QVersionNumber::fromString(normVer(a));
    const QVersionNumber vb = QVersionNumber::fromString(normVer(b));
    if (!va.isNull() && !vb.isNull())
        return QVersionNumber::compare(va, vb);
    return QString::compare(normVer(a), normVer(b), Qt::CaseInsensitive);
}

QString sha1Of(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    QCryptographicHash h(QCryptographicHash::Sha1);
    while (!f.atEnd())
        h.addData(f.read(1024 * 1024));
    return QString::fromLatin1(h.result().toHex());
}

QString sha256Of(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    QCryptographicHash h(QCryptographicHash::Sha256);
    while (!f.atEnd())
        h.addData(f.read(1024 * 1024));
    return QString::fromLatin1(h.result().toHex());
}

QString displayName(const QString &id)
{
    if (id == QLatin1String("launcher"))
        return QCoreApplication::translate("UpdateService", QOL_APP_NAME);
    if (id == QLatin1String("pu"))
        return QCoreApplication::translate("UpdateService", "Portable kit (pu.dat)");
    if (id == QLatin1String("t7-cll"))
        return QCoreApplication::translate("UpdateService", "T7-CLL client");
    if (id == QLatin1String("s1-cll"))
        return QCoreApplication::translate("UpdateService", "S1-CLL client");
    if (id == QLatin1String("boiii-community"))
        return QCoreApplication::translate("UpdateService", "BOIII Community");
    return id;
}

bool applyItem(const UpdateService::Item &it, AppSettings &settings, QString &error,
               const std::function<void(const QString &, int)> &progress)
{
    if (it.url.isEmpty()) {
        error = QCoreApplication::translate("UpdateService", "Missing download URL.");
        return false;
    }
    const QString tmp = QDir::temp().filePath(
        QStringLiteral("cll_upd_%1_%2").arg(it.id, it.name));
    progress(QCoreApplication::translate("UpdateService", "Downloading %1...").arg(it.name), 8);
    if (!Downloader::downloadToFile(it.url, tmp, error, [&](qint64 g, qint64 t) {
            const int pct = (t > 0) ? int(g * 80 / t) : 15;
            progress(QCoreApplication::translate("UpdateService", "Downloading %1...").arg(it.name), pct);
        }, 30 * 60 * 1000)) {
        QFile::remove(tmp);
        return false;
    }
    if (!it.hash.isEmpty()) {
        const QString got = sha1Of(tmp);
        if (!got.isEmpty() && got.compare(it.hash, Qt::CaseInsensitive) != 0) {
            QFile::remove(tmp);
            error = QCoreApplication::translate("UpdateService", "SHA-1 mismatch for %1.").arg(it.name);
            return false;
        }
    } else if (!it.sha256.isEmpty()) {
        // A live lookup cannot reuse the feed's sha1, but GitHub publishes its
        // own digest for the asset, so the download is still verified.
        const QString got = sha256Of(tmp);
        if (!got.isEmpty() && got.compare(it.sha256, Qt::CaseInsensitive) != 0) {
            QFile::remove(tmp);
            error = QCoreApplication::translate("UpdateService", "SHA-256 mismatch for %1.").arg(it.name);
            return false;
        }
    }
    // Stamp what we actually downloaded. The feed's hash is meaningless once a
    // live lookup has moved us to a newer release.
    const QString stampHash = it.hash.isEmpty() ? sha1Of(tmp) : it.hash;

    if (it.id == QLatin1String("launcher")) {
        // No stamp here. The old code recorded the new version the moment the
        // download finished, so the .ini claimed a build that was never
        // installed. Only a start that actually runs the new version counts:
        // SelfUpdate::reconcile() does that stamping.
        progress(QCoreApplication::translate("UpdateService", "Closing other copies of the app..."), 90);
        if (!SelfUpdate::begin(tmp, it.version, error)) {
            QFile::remove(tmp);
            return false;
        }
        return true;
    }

    if (it.id == QLatin1String("pu")) {
        progress(QCoreApplication::translate("UpdateService", "Extracting portable kit..."), 85);
        const QString puDir = AppSettings::localPuDir();
        QDir().mkpath(puDir);
        if (!ArchiveTool::extractObfuscatedBundle(tmp, puDir, &error)) {
            QFile::remove(tmp);
            if (error.isEmpty())
                error = QCoreApplication::translate("UpdateService", "Failed to extract pu.dat.");
            return false;
        }
        QFile::remove(tmp);
        settings.plutoniumInstance = puDir;
        settings.saveToIni();
        UpdateService::stamp(it.id, it.version, stampHash);
        return true;
    }

    if (it.id == QLatin1String("t7-cll")) {
        const QString dest = QDir::cleanPath(settings.bo3);
        if (dest.isEmpty()) {
            QFile::remove(tmp);
            error = QCoreApplication::translate("UpdateService", "Black Ops III folder is not set.");
            return false;
        }
        progress(QCoreApplication::translate("UpdateService", "Extracting T7-CLL..."), 85);
        if (!ArchiveTool::extractToDirectory(tmp, dest, &error)) {
            QFile::remove(tmp);
            return false;
        }
        QFile::remove(tmp);
        UpdateService::stamp(it.id, it.version, stampHash);
        return true;
    }

    if (it.id == QLatin1String("s1-cll")) {
        const QString dest = QDir(settings.aw).filePath(QStringLiteral("s1.exe"));
        if (settings.aw.isEmpty()) {
            QFile::remove(tmp);
            error = QCoreApplication::translate("UpdateService", "Advanced Warfare folder is not set.");
            return false;
        }
        QFile::remove(dest);
        if (!QFile::copy(tmp, dest)) {
            QFile::remove(tmp);
            error = QCoreApplication::translate("UpdateService", "Could not write s1.exe.");
            return false;
        }
        QFile::remove(tmp);
        UpdateService::stamp(it.id, it.version, stampHash);
        return true;
    }

    if (it.id == QLatin1String("boiii-community")) {
        const QString dest = QDir(settings.bo3).filePath(QStringLiteral("boiii.exe"));
        if (settings.bo3.isEmpty()) {
            QFile::remove(tmp);
            error = QCoreApplication::translate("UpdateService", "Black Ops III folder is not set.");
            return false;
        }
        QFile::remove(dest);
        if (!QFile::copy(tmp, dest)) {
            QFile::remove(tmp);
            error = QCoreApplication::translate("UpdateService", "Could not write boiii.exe.");
            return false;
        }
        QFile::remove(tmp);
        settings.bo3Client = QStringLiteral("competitive");
        settings.bo3ClientChosen = true;
        settings.saveToIni();
        UpdateService::stamp(it.id, it.version, stampHash);
        return true;
    }

    QFile::remove(tmp);
    error = QCoreApplication::translate("UpdateService", "Unknown component.");
    return false;
}

} // namespace

namespace UpdateService {

Item Catalog::byId(const QString &id) const
{
    for (const Item &it : items) {
        if (it.id == id)
            return it;
    }
    return {};
}

void stamp(const QString &id, const QString &version, const QString &hash)
{
    QSettings ini(AppSettings::iniPath(), QSettings::IniFormat);
    ini.beginGroup(QStringLiteral("Updates"));
    ini.setValue(id + QStringLiteral("/version"), version);
    ini.setValue(id + QStringLiteral("/hash"), hash);
    ini.endGroup();
    ini.sync();
}

void stampFile(const QString &id, const QString &version, const QString &filePath)
{
    stamp(id, version, sha1Of(filePath));
}

QString stampedVersion(const QString &id)
{
    QSettings ini(AppSettings::iniPath(), QSettings::IniFormat);
    ini.beginGroup(QStringLiteral("Updates"));
    return ini.value(id + QStringLiteral("/version")).toString();
}

QString stampedHash(const QString &id)
{
    QSettings ini(AppSettings::iniPath(), QSettings::IniFormat);
    ini.beginGroup(QStringLiteral("Updates"));
    return ini.value(id + QStringLiteral("/hash")).toString();
}

Catalog fetch(QString &error)
{
    Catalog cat;
    const QByteArray raw = Downloader::downloadBytes(QString::fromUtf8(kFeedUrl), error, 20000);
    if (raw.isEmpty())
        return cat;
    const auto doc = QJsonDocument::fromJson(raw);
    if (!doc.isObject()) {
        error = QCoreApplication::translate("UpdateService", "Invalid update feed.");
        return cat;
    }
    for (const QJsonValue &v : doc.object().value(QStringLiteral("items")).toArray()) {
        const QJsonObject o = v.toObject();
        Item it;
        it.id = o.value(QStringLiteral("id")).toString();
        it.name = o.value(QStringLiteral("name")).toString();
        it.repo = o.value(QStringLiteral("repo")).toString();
        it.version = o.value(QStringLiteral("version")).toString();
        it.url = o.value(QStringLiteral("url")).toString();
        it.hash = o.value(QStringLiteral("hash")).toString();
        it.size = static_cast<qint64>(o.value(QStringLiteral("size")).toDouble());
        if (!it.id.isEmpty())
            cat.items << it;
    }

    QString upErr;
    const QByteArray upRaw = Downloader::downloadBytes(
        QStringLiteral("https://gitlab.com/boiii-community/BOIII-Community/-/raw/main/updater.json?ref_type=heads"),
        upErr, 20000);
    const auto upDoc = QJsonDocument::fromJson(upRaw);
    if (upDoc.isArray()) {
        Item community;
        community.id = QStringLiteral("boiii-community");
        community.name = QStringLiteral("boiii.exe");
        community.repo = QStringLiteral("boiii-community/BOIII-Community");
        for (const QJsonValue &row : upDoc.array()) {
            const QJsonArray a = row.toArray();
            if (a.size() < 4)
                continue;
            if (a.at(0).toString().compare(QLatin1String("boiii.exe"), Qt::CaseInsensitive) != 0)
                continue;
            community.size = static_cast<qint64>(a.at(1).toDouble());
            community.hash = a.at(2).toString();
            community.url = a.at(3).toString();
            break;
        }
        if (!community.url.isEmpty()) {
            for (int i = 0; i < cat.items.size(); ++i) {
                if (cat.items[i].id == community.id) {
                    cat.items.removeAt(i);
                    break;
                }
            }
            cat.items << community;
        }
    }
    return cat;
}

// Is this component present on this PC at all? Nothing is worth offering an
// update for otherwise, and it keeps the live lookups down to what matters.
static bool componentInstalled(const QString &id, const AppSettings &settings)
{
    if (id == QLatin1String("launcher"))
        return true;
    if (id == QLatin1String("pu"))
        return settings.usingLocalPortableKit() || !stampedHash(id).isEmpty();
    if (id == QLatin1String("t7-cll"))
        return QFileInfo::exists(QDir(settings.bo3).filePath(QStringLiteral("boiii.exe")))
               || !stampedHash(id).isEmpty();
    if (id == QLatin1String("s1-cll"))
        return QFileInfo::exists(QDir(settings.aw).filePath(QStringLiteral("s1.exe")))
               || !stampedHash(id).isEmpty();
    if (id == QLatin1String("boiii-community"))
        return settings.bo3Client == QLatin1String("competitive")
               || !stampedHash(id).isEmpty();
    return false;
}

void resolveLatest(Catalog &cat, const AppSettings &settings)
{
    for (Item &it : cat.items) {
        // boiii-community already comes from its own live updater.json in
        // fetch(), and it has no GitHub release to ask about.
        if (it.repo.isEmpty() || it.name.isEmpty()
            || it.id == QLatin1String("boiii-community"))
            continue;
        if (!componentInstalled(it.id, settings))
            continue;

        // GitHub answers /releases/latest/download/<asset> with a redirect to
        // /releases/download/<tag>/<asset>. One request, no api.github.com, so
        // no rate limit and no token - and the redirect only exists if that
        // release really does carry an asset by that name.
        QString err;
        const QString target = Downloader::redirectTargetOf(
            QStringLiteral("https://github.com/%1/releases/latest/download/%2")
                .arg(it.repo, it.name),
            err, 10000);
        if (target.isEmpty())
            continue; // offline, blocked, repo gone: keep the feed's values

        static const QRegularExpression re(
            QStringLiteral("/releases/download/([^/]+)/"));
        const QRegularExpressionMatch m = re.match(target);
        if (!m.hasMatch())
            continue;
        const QString tag = QUrl::fromPercentEncoding(m.captured(1).toUtf8());
        if (tag.isEmpty())
            continue;

        it.feedVersion = it.version;
        // The feed pinned a sha1 for the old asset; it cannot vouch for a newer
        // one. Keep it only while the tag still matches what the feed described.
        if (tag.compare(it.version, Qt::CaseInsensitive) != 0)
            it.hash.clear();
        it.version = tag;
        it.url = target;
        it.live = true;
    }
}

// Should this component be offered? When we asked the repo directly, the
// release tag is the authority; the feed's sha1 belongs to an older asset and
// cannot speak for a newer one. Falling back to the feed's version as "what we
// would have installed" keeps users who never had a version stamped - anyone
// who installed the client outside this app - from being stuck forever.
static bool clientPending(const UpdateService::Item &it)
{
    const QString localVer = stampedVersion(it.id);
    const QString localHash = stampedHash(it.id);
    if (it.live) {
        const QString base = !localVer.isEmpty() ? localVer : it.feedVersion;
        if (!base.isEmpty())
            return VersionCompare::isUpdate(base, it.version);
    }
    if (it.hash.isEmpty())
        return false; // nothing trustworthy to compare against: stay quiet
    return localHash.isEmpty() || localHash.compare(it.hash, Qt::CaseInsensitive) != 0;
}

QList<Pending> detect(const Catalog &cat, const AppSettings &settings)
{
    QList<Pending> out;
    for (const Item &it : cat.items) {
        Pending p;
        p.remote = it;
        p.title = displayName(it.id);

        const QString localVer = (it.id == QLatin1String("launcher"))
                                     ? QStringLiteral(CLL_VERSION)
                                     : stampedVersion(it.id);
        if (!it.version.isEmpty() && !localVer.isEmpty()
            && cmpVer(it.version, localVer) < 0)
            continue;

        if (it.id == QLatin1String("launcher")) {
            if (cmpVer(it.version, QStringLiteral(CLL_VERSION)) > 0) {
                p.detail = QCoreApplication::translate("UpdateService", "%1 → %2")
                               .arg(QStringLiteral(CLL_VERSION), it.version);
                out << p;
            }
            continue;
        }

        if (it.id == QLatin1String("pu")) {
            if (!componentInstalled(it.id, settings))
                continue;
            if (clientPending(it)) {
                p.detail = it.version;
                out << p;
            }
            continue;
        }

        if (it.id == QLatin1String("t7-cll")) {
            if (!componentInstalled(it.id, settings))
                continue;
            if (clientPending(it)) {
                p.detail = it.version;
                out << p;
            }
            continue;
        }

        if (it.id == QLatin1String("s1-cll")) {
            if (!componentInstalled(it.id, settings))
                continue;
            if (clientPending(it)) {
                p.detail = it.version;
                out << p;
            }
            continue;
        }

        if (it.id == QLatin1String("boiii-community")) {
            if (settings.bo3Client != QLatin1String("competitive")
                && stampedHash(it.id).isEmpty())
                continue;
            const QString exe = QDir(settings.bo3).filePath(QStringLiteral("boiii.exe"));
            QString local = stampedHash(it.id);
            if (local.isEmpty() && QFileInfo::exists(exe))
                local = sha1Of(exe);
            if (local.isEmpty() || local.compare(it.hash, Qt::CaseInsensitive) != 0) {
                p.detail = QCoreApplication::translate("UpdateService", "new version");
                out << p;
            }
        }
    }
    return out;
}

bool prompt(QWidget *parent, AppSettings &settings, bool silentIfNone)
{
    QString error;
    Catalog cat = fetch(error);
    // Ask each component's own repo before deciding anything, so a feed nobody
    // has regenerated cannot pin the LAN clients to one version forever.
    resolveLatest(cat, settings);
    if (cat.items.isEmpty()) {
        if (!silentIfNone) {
            QMessageBox::warning(parent,
                                 QCoreApplication::translate("UpdateService", "Updates"),
                                 error.isEmpty()
                                     ? QCoreApplication::translate("UpdateService", "Could not read the update list.")
                                     : error);
        }
        return false;
    }

    // Refresh stamps when the file already matches.
    for (const Item &it : cat.items) {
        if (it.id == QLatin1String("launcher"))
            continue;
        const QString local = stampedHash(it.id);
        if (!it.hash.isEmpty() && !local.isEmpty()
            && local.compare(it.hash, Qt::CaseInsensitive) == 0
            && stampedVersion(it.id) != it.version)
            stamp(it.id, it.version, it.hash);
    }

    QList<Pending> pending = detect(cat, settings);
    // A self-update that cannot replace the exe would otherwise be offered,
    // fail, restart and be offered again for as long as the app is open. After
    // two goes the automatic check leaves it alone; a manual check from
    // Settings still tries, because by then the user has been told why.
    if (silentIfNone) {
        for (int i = pending.size() - 1; i >= 0; --i) {
            if (pending.at(i).remote.id == QLatin1String("launcher")
                && SelfUpdate::isExhausted(pending.at(i).remote.version))
                pending.removeAt(i);
        }
    }
    if (pending.isEmpty()) {
        if (!silentIfNone) {
            QMessageBox::information(parent,
                                     QCoreApplication::translate("UpdateService", "Updates"),
                                     QCoreApplication::translate("UpdateService", "Everything is up to date."));
        }
        return false;
    }

    QDialog dlg(parent);
    dlg.setWindowTitle(QCoreApplication::translate("UpdateService", "Updates available"));
    auto *lay = new QVBoxLayout(&dlg);
    auto *intro = new QLabel(
        QCoreApplication::translate("UpdateService",
                                    "New versions were found. You can install them now."),
        &dlg);
    intro->setWordWrap(true);
    lay->addWidget(intro);
    auto *list = new QListWidget(&dlg);
    for (const Pending &p : pending) {
        const QString line = p.detail.isEmpty()
                                 ? p.title
                                 : QStringLiteral("%1  (%2)").arg(p.title, p.detail);
        list->addItem(line);
    }
    list->setSelectionMode(QAbstractItemView::NoSelection);
    lay->addWidget(list);

    auto *mute = new QPushButton(
        QCoreApplication::translate("UpdateService", "Don't notify me about updates"), &dlg);
    mute->setFlat(true);
    lay->addWidget(mute);

    auto *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    box->button(QDialogButtonBox::Ok)->setText(QCoreApplication::translate("UpdateService", "Update now"));
    box->button(QDialogButtonBox::Cancel)->setText(QCoreApplication::translate("UpdateService", "Later"));
    lay->addWidget(box);
    QObject::connect(box, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(box, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    QObject::connect(mute, &QPushButton::clicked, &dlg, [&]() {
        settings.checkUpdatesOnStart = false;
        settings.saveToIni();
        dlg.reject();
    });
    dlg.resize(420, 280);
    if (dlg.exec() != QDialog::Accepted)
        return false;

    auto *progress = new ProgressDialog(
        QCoreApplication::translate("UpdateService", "Updating"), parent);
    progress->setAttribute(Qt::WA_DeleteOnClose);
    progress->setIndeterminate(true);
    progress->show();

    bool needQuit = false;
    QString fail;
    for (const Pending &p : pending) {
        progress->setStatus(displayName(p.remote.id));
        QString err;
        const bool ok = applyItem(p.remote, settings, err, [&](const QString &text, int pct) {
            progress->setStatus(text);
            if (pct < 0)
                progress->setIndeterminate(true);
            else
                progress->setProgress(pct, 100);
        });
        if (!ok) {
            fail = err;
            break;
        }
        if (p.remote.id == QLatin1String("launcher"))
            needQuit = true;
    }
    progress->close();
    if (!fail.isEmpty()) {
        QMessageBox::warning(parent,
                             QCoreApplication::translate("UpdateService", "Updates"),
                             fail);
        return false;
    }
    if (needQuit)
        return true;
    QMessageBox::information(parent,
                             QCoreApplication::translate("UpdateService", "Updates"),
                             QCoreApplication::translate("UpdateService", "Updates installed."));
    return false;
}

} // namespace UpdateService
