#include "QolPage.h"
#include "VersionCompare.h"
#include "AppSettings.h"
#include "GameLauncher.h"
#include "ProgressDialog.h"
#include "QolBackups.h"
#include "QolService.h"
#include "Version.h"
#include "../Checkables.h"
#include "../Theme.h"

#include <QDesktopServices>
#include <QDir>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QStyle>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <QtConcurrent>

namespace {

QLabel *section(QWidget *parent, const QString &text)
{
    auto *l = new QLabel(text, parent);
    l->setObjectName("SectionTitle");
    return l;
}

QString fmtFiles(int n)
{
    return n == 1 ? QObject::tr("1 file") : QObject::tr("%1 files").arg(n);
}

QString fmtSize(qint64 b)
{
    if (b >= 1000LL * 1000 * 1000) return QObject::tr("%1 GB").arg(b / 1e9, 0, 'f', 1);
    if (b >= 1000 * 1000)          return QObject::tr("%1 MB").arg(b / 1e6, 0, 'f', 0);
    if (b >= 1000)                 return QObject::tr("%1 KB").arg(b / 1e3, 0, 'f', 0);
    return QObject::tr("%1 bytes").arg(b);
}

} // namespace

QolPage::QolPage(AppSettings &settings, QWidget *parent)
    : QWidget(parent)
    , m_settings(settings)
{
    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto *inner = new QWidget;
    auto *root = new QVBoxLayout(inner);
    root->setContentsMargins(28, 22, 28, 24);
    root->setSpacing(12);

    m_intro = new QLabel(inner);
    m_intro->setObjectName("MutedHint");
    m_intro->setWordWrap(true);
    root->addWidget(m_intro);

    m_plutoHint = new QLabel(inner);
    m_plutoHint->setObjectName("MutedHint");
    m_plutoHint->setWordWrap(true);
    root->addWidget(m_plutoHint);

    root->addWidget(section(inner, tr("THE MODS")));
    for (const QolService::SeriesMod &m : QolService::series()) {
        Row r = addRow(root, m.gameCode.toUpper(), tr("Quality of Life for %1").arg(m.gameTitle), QString());
        r.card->setProperty("gameCode", m.gameCode);
        m_modRows << r;
        m_latest << QString();
    }

    root->addWidget(section(inner, tr("BLACK OPS II EXTRAS")));
    m_textures = addRow(root, tr("PACK"), tr("HD texture pack"),
                        tr("Higher resolution textures for the Zombies maps. Goes into storage\\t6\\images."));
    m_sounds = addRow(root, tr("PACK"), tr("Custom sounds"),
                      tr("Replacement sound banks. Goes into storage\\t6\\zone."));
    m_controller = addRow(root, tr("PACK"), tr("Controller icons"),
                          tr("Button prompts for PlayStation 5, Nintendo Switch or Xbox instead of the stock Xbox 360 set."));

    root->addWidget(section(inner, tr("RESHADE")));
    m_reshade = addRow(root, tr("VISUALS"), tr("ReShade"),
                       tr("Cinematic colour grading for every Plutonium game. Plutonium clears its bin folder on "
                          "every start, so play with the ReShade option on the game page (or start the watchdog "
                          "here) and the files are put back the moment the game opens."));
    m_reshade.tertiary = new QPushButton(tr("View log"), m_reshade.card);
    m_reshade.tertiary->setCursor(Qt::PointingHandCursor);
    m_reshade.tertiary->setMinimumHeight(36);
    static_cast<QHBoxLayout *>(m_reshade.card->layout())->insertWidget(1, m_reshade.tertiary, 0, Qt::AlignVCenter);
    connect(m_reshade.tertiary, &QPushButton::clicked, this, [this]() {
        const QString log = QDir(GameLauncher::toolsDir()).filePath("reshade-watchdog.log");
        QFile file(log);
        if (!file.open(QIODevice::ReadOnly)) {
            QMessageBox::information(this, tr("ReShade watchdog"), tr("Start the watchdog to create its log."));
            return;
        }
        QMessageBox box(this);
        box.setWindowTitle(tr("ReShade watchdog log"));
        box.setText(tr("The watchdog runs in the background. Its latest activity is shown below."));
        box.setDetailedText(QString::fromUtf8(file.readAll().right(24000)));
        box.setStandardButtons(QMessageBox::Ok);
        box.exec();
    });
    m_dlss = addRow(root, tr("LAN ONLY"), tr("DLSS 5 Neural Rendering"),
                    tr("NVIDIA DLSS 5 in Black Ops II. Install downloads it and checks every file; the app keeps "
                       "it up to date. It runs only in LAN games: every online launch takes it out of Plutonium "
                       "first and will not start while any of it is still there. Needs an NVIDIA RTX card. "
                       "About 180 MB."));

    // Backups ---------------------------------------------------------------
    root->addWidget(section(inner, tr("BACKUPS")));
    auto *backupHint = new QLabel(tr("Copies of your own files, kept in storage\\t6\\backups with one plain "
                                     "folder per kind, so you can always put them back - here, or by hand. "
                                     "An install never deletes a backup, and each kind keeps its oldest "
                                     "copy (the one from before anything was installed) until you replace "
                                     "it."), inner);
    backupHint->setObjectName("MutedHint");
    backupHint->setWordWrap(true);
    root->addWidget(backupHint);
    {
        auto *opts = new QHBoxLayout();
        m_backupAuto = new Ui::CheckBox(tr("Back up my files before every install"), inner);
        m_backupAuto->setChecked(m_settings.backupBeforeInstall);
        m_backupAuto->setToolTip(tr("Before the mod, a pack or controller icons are installed, your "
                                    "textures, sounds, icons, ReShade setup, scripts and settings are "
                                    "copied into the backups folder first. Only the first time for each "
                                    "kind: after that the older copy is kept. ReShade is always backed "
                                    "up before it is installed."));
        opts->addWidget(m_backupAuto, 1);
        m_backupOpen = new QPushButton(tr("Open backups folder"), inner);
        m_backupOpen->setCursor(Qt::PointingHandCursor);
        m_backupOpen->setMinimumHeight(32);
        opts->addWidget(m_backupOpen, 0, Qt::AlignRight);
        root->addLayout(opts);
    }
    m_backupAll = addRow(root, tr("EVERYTHING"), tr("All my files"),
                         tr("Every kind below except the mod itself and your other mods and maps. "
                            "This is what is taken automatically before an install."));
    for (const QString &kind : QolBackups::kinds()) {
        const QolBackups::Info i = QolBackups::info(m_settings, kind, false);
        Row r = addRow(root, tr("BACKUP"), i.label, i.desc);
        r.tertiary = new QPushButton(r.card);
        r.tertiary->setCursor(Qt::PointingHandCursor);
        r.tertiary->setMinimumHeight(36);
        r.tertiary->setText(tr("More"));
        static_cast<QHBoxLayout *>(r.card->layout())->insertWidget(1, r.tertiary, 0, Qt::AlignVCenter);
        m_backupRows << r;
    }
    root->addStretch();

    scroll->setWidget(inner);
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->addWidget(scroll);

    // Actions -------------------------------------------------------------
    const auto series = QolService::series();
    for (int i = 0; i < m_modRows.size(); ++i) {
        const QolService::SeriesMod m = series[i];
        connect(m_modRows[i].primary, &QPushButton::clicked, this, [this, m]() {
            if (!m.released) {
                openUrl(QStringLiteral("https://github.com/") + m.repo);
                return;
            }
            runJob(tr("Installing Quality of Life for %1").arg(m.gameTitle),
                   [this, m](QString &err, const std::function<void(const QString &, int)> &p) {
                       return QolService::installMod(m_settings, m, p, err);
                   });
        });
        connect(m_modRows[i].secondary, &QPushButton::clicked, this, [this, m]() {
            openUrl(QStringLiteral("https://github.com/") + m.repo);
        });
    }

    connect(m_textures.primary, &QPushButton::clicked, this, [this]() {
        if (QolService::packInstalled(m_settings, QolService::Pack::Textures)) {
            removeWithRestore(tr("HD texture pack"), tr("Remove the HD texture pack?"), QStringLiteral("images"),
                              [this](QString &err) {
                                  return QolService::removePack(m_settings, QolService::Pack::Textures, err);
                              });
            return;
        }
        runJob(tr("Installing the HD texture pack"),
               [this](QString &err, const std::function<void(const QString &, int)> &p) {
                   return QolService::installPack(m_settings, QolService::Pack::Textures, p, err);
               });
    });
    connect(m_sounds.primary, &QPushButton::clicked, this, [this]() {
        if (QolService::packInstalled(m_settings, QolService::Pack::Sounds)) {
            removeWithRestore(tr("Custom sounds"), tr("Remove the custom sound pack?"), QStringLiteral("zone"),
                              [this](QString &err) {
                                  return QolService::removePack(m_settings, QolService::Pack::Sounds, err);
                              });
            return;
        }
        runJob(tr("Installing the custom sounds"),
               [this](QString &err, const std::function<void(const QString &, int)> &p) {
                   return QolService::installPack(m_settings, QolService::Pack::Sounds, p, err);
               });
    });
    connect(m_controller.primary, &QPushButton::clicked, this, [this]() {
        QMessageBox box(this);
        box.setWindowTitle(tr("Controller icons"));
        box.setText(tr("Which controller do you play with?"));
        auto *ps5 = box.addButton(tr("PlayStation 5"), QMessageBox::AcceptRole);
        auto *sw = box.addButton(tr("Nintendo Switch"), QMessageBox::AcceptRole);
        auto *xb = box.addButton(tr("Xbox"), QMessageBox::AcceptRole);
        box.addButton(QMessageBox::Cancel);
        box.exec();
        QString pack;
        if (box.clickedButton() == ps5) pack = QStringLiteral("ps5");
        else if (box.clickedButton() == sw) pack = QStringLiteral("switch");
        else if (box.clickedButton() == xb) pack = QStringLiteral("xbox");
        if (pack.isEmpty())
            return;
        runJob(tr("Installing controller icons"),
               [this, pack](QString &err, const std::function<void(const QString &, int)> &p) {
                   return QolService::installController(m_settings, pack, p, err);
               });
    });
    connect(m_controller.secondary, &QPushButton::clicked, this, [this]() {
        removeWithRestore(tr("Controller icons"), tr("Go back to the game's own button icons?"),
                          QStringLiteral("controller"), [this](QString &err) {
                              return QolService::removeController(m_settings, err);
                          });
    });
    connect(m_reshade.primary, &QPushButton::clicked, this, [this]() {
        QString err;
        if (QolService::reShadeInstalled(m_settings)) {
            // Remove means gone: the add-ons, the 64-bit host and the preset
            // entries that name them go with it, not just dxgi.dll.
            removeWithRestore(tr("ReShade"), tr("Remove ReShade from Plutonium's bin folder?"),
                              QStringLiteral("reshade"), [this](QString &e) {
                                  return QolService::ensureReShadeAbsent(m_settings, e);
                              });
            return;
        } else if (!QolService::installReShade(m_settings, err)) {
            QMessageBox::warning(this, tr("ReShade"), err);
        }
        refresh();
    });
    connect(m_reshade.secondary, &QPushButton::clicked, this, [this]() {
        // A toggle with visible state. Starting the watchdog used to give no
        // feedback at all - the process is a hidden PowerShell script - so the
        // button read as broken whether it worked or not.
        if (GameLauncher::runningReShadeWatchdog() > 0) {
            GameLauncher::stopReShadeWatchdog();
            refresh();
            return;
        }
        QString err;
        if (!GameLauncher::startReShadeWatchdog(m_settings.plutoniumInstance, err)) {
            QMessageBox::warning(this, tr("ReShade"), err);
            return;
        }
        refresh();
        // A watchdog that dies in its first seconds means the script failed
        // before it could say why; say it here instead of leaving silence.
        const QPointer<QolPage> guard(this);
        QTimer::singleShot(2000, this, [this, guard]() {
            if (!guard)
                return;
            if (GameLauncher::runningReShadeWatchdog() <= 0)
                QMessageBox::warning(this, tr("ReShade"),
                                     tr("The ReShade watchdog closed immediately. Its log is "
                                        "reshade-watchdog.log in the app's tools folder."));
            else
                refresh();
        });
    });
    // One button, no folder to pick. Install and Update are the same job: read
    // the published manifest, download, verify, swap in. Remove takes it all
    // back out of bin and out of storage.
    const auto installLatest = [this](const QString &title) {
        runJob(title, [this](QString &err, const QolService::Progress &p) {
            if (p) p(tr("Checking the published DLSS 5 release..."), 2);
            QolService::DlssRelease release;
            if (!QolService::latestDlssRelease(release, err)) return false;
            return QolService::installDlssPayload(m_settings, release, p, err);
        }, {}, [this](bool ok) {
            if (!ok) return;
            // Installing is asking for it: tick both boxes so the very next LAN
            // launch of Black Ops II runs DLSS without another trip to the
            // game page. Online is unaffected either way.
            m_settings.launchReShade = true;
            m_settings.launchDlss5 = true;
            m_settings.saveToIni();
            m_dlssCheckStarted = false;   // re-read the release so Update disappears
        });
    };
    connect(m_dlss.primary, &QPushButton::clicked, this, [this, installLatest]() {
        if (QolService::dlssPayloadReady(m_settings)) {
            if (!confirm(tr("DLSS 5"), tr("Remove DLSS 5 from this PC? ReShade stays installed.")))
                return;
            runJob(tr("Removing DLSS 5"), [this](QString &err, const QolService::Progress &) {
                return QolService::removeDlssPayload(m_settings, err);
            }, {}, [this](bool ok) {
                if (ok) {
                    m_settings.launchDlss5 = false;
                    m_settings.saveToIni();
                }
            });
            return;
        }
        installLatest(tr("Installing DLSS 5"));
    });
    connect(m_dlss.secondary, &QPushButton::clicked, this, [installLatest, this]() {
        installLatest(tr("Updating DLSS 5"));
    });

    // Backups ---------------------------------------------------------------
    connect(m_backupAuto, &QCheckBox::toggled, this, [this](bool on) {
        m_settings.backupBeforeInstall = on;
        m_settings.saveToIni();
    });
    connect(m_backupOpen, &QPushButton::clicked, this, [this]() {
        const QString dir = QolBackups::root(m_settings);
        QDir().mkpath(dir);
        QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
    });
    connect(m_backupAll.primary, &QPushButton::clicked, this, [this]() {
        // Kinds that already have a backup keep it: this fills the gaps, it
        // does not overwrite the copies taken before anything was installed.
        auto summary = std::make_shared<QStringList>();
        runJob(tr("Backing up your files"),
               [this, summary](QString &err, const std::function<void(const QString &, int)> &p) {
                   for (const QString &k : QolBackups::automaticKinds()) {
                       QString msg;
                       if (QolBackups::backup(m_settings, k, false, p, msg) == QolBackups::Result::Failed) {
                           err = msg;
                           return false;
                       }
                       *summary << msg;
                   }
                   return true;
               },
               [this, summary]() {
                   QMessageBox::information(this, tr("Backups"), summary->join(QStringLiteral("\n\n")));
               });
    });
    connect(m_backupAll.secondary, &QPushButton::clicked, this, [this]() {
        QStringList have;
        for (const QString &k : QolBackups::automaticKinds())
            if (QolBackups::exists(m_settings, k))
                have << k;
        if (have.isEmpty())
            return;
        QStringList titles;
        for (const QString &k : have)
            titles << QolBackups::info(m_settings, k).title;
        if (!confirm(tr("Backups"), tr("Put back %1?\n\nYour backed-up files are copied over what is there "
                                       "now. Files added since are left alone.").arg(titles.join(QStringLiteral(", ")))))
            return;
        runJob(tr("Putting your files back"),
               [this, have](QString &err, const std::function<void(const QString &, int)> &p) {
                   for (const QString &k : have)
                       if (!QolBackups::restore(m_settings, k, p, err))
                           return false;
                   return true;
               });
    });
    const QStringList kinds = QolBackups::kinds();
    for (int i = 0; i < m_backupRows.size(); ++i) {
        const QString kind = kinds[i];
        connect(m_backupRows[i].primary, &QPushButton::clicked, this, [this, kind]() {
            const QolBackups::Info info = QolBackups::info(m_settings, kind);
            if (info.exists) {
                if (!confirm(info.label, tr("Put %1 back?\n\nThe backup from %2 is copied over what is there "
                                            "now. Files added since are left alone.")
                                             .arg(info.title, info.when.toString(QStringLiteral("d MMM yyyy HH:mm")))))
                    return;
                runJob(tr("Putting back %1").arg(info.title),
                       [this, kind](QString &err, const std::function<void(const QString &, int)> &p) {
                           return QolBackups::restore(m_settings, kind, p, err);
                       });
                return;
            }
            auto message = std::make_shared<QString>();
            runJob(tr("Backing up %1").arg(info.title),
                   [this, kind, message](QString &err, const std::function<void(const QString &, int)> &p) {
                       if (QolBackups::backup(m_settings, kind, false, p, *message) == QolBackups::Result::Failed) {
                           err = *message;
                           return false;
                       }
                       return true;
                   },
                   [this, info, message]() { QMessageBox::information(this, info.label, *message); });
        });
        connect(m_backupRows[i].secondary, &QPushButton::clicked, this, [this, kind]() {
            const QolBackups::Info info = QolBackups::info(m_settings, kind);
            if (!info.exists)
                return;
            QDesktopServices::openUrl(QUrl::fromLocalFile(info.folder));
        });
        connect(m_backupRows[i].tertiary, &QPushButton::clicked, this, [this, kind]() { backupMenu(kind); });
    }

    retranslate();
    refresh();
}

void QolPage::backupMenu(const QString &kind)
{
    const QolBackups::Info info = QolBackups::info(m_settings, kind);
    QMessageBox box(this);
    box.setWindowTitle(info.label);
    box.setText(info.exists
                    ? tr("Backup of %1 from %2: %3 files, %4.\n\nWhat would you like to do?")
                          .arg(info.title, info.when.toString(QStringLiteral("d MMM yyyy HH:mm")))
                          .arg(info.files).arg(fmtSize(info.bytes))
                    : tr("There is no backup of %1 yet.").arg(info.title));
    QAbstractButton *replace = box.addButton(info.exists ? tr("Back up again, replacing it") : tr("Back up now"),
                                             QMessageBox::AcceptRole);
    QAbstractButton *del = info.exists ? box.addButton(tr("Delete this backup"), QMessageBox::DestructiveRole) : nullptr;
    box.addButton(QMessageBox::Cancel);
    box.exec();
    if (box.clickedButton() == replace) {
        if (info.exists && !confirm(info.label, tr("Replace the backup from %1 with what is there now?\n\n"
                                                   "The older copy is the one from before anything was "
                                                   "installed. Once replaced it is gone.")
                                                    .arg(info.when.toString(QStringLiteral("d MMM yyyy HH:mm")))))
            return;
        auto message = std::make_shared<QString>();
        runJob(tr("Backing up %1").arg(info.title),
               [this, kind, message](QString &err, const std::function<void(const QString &, int)> &p) {
                   if (QolBackups::backup(m_settings, kind, true, p, *message) == QolBackups::Result::Failed) {
                       err = *message;
                       return false;
                   }
                   return true;
               },
               [this, info, message]() { QMessageBox::information(this, info.label, *message); });
    } else if (del && box.clickedButton() == del) {
        if (!confirm(info.label, tr("Delete the backup of %1? The files on your PC are not touched.").arg(info.title)))
            return;
        QString err;
        if (!QolBackups::remove(m_settings, kind, err))
            QMessageBox::warning(this, info.label, err);
        refreshBackups();
    }
}

void QolPage::removeWithRestore(const QString &title, const QString &question, const QString &backupKind,
                                const std::function<bool(QString &)> &remove)
{
    QString err;
    if (!QolBackups::exists(m_settings, backupKind)) {
        if (!confirm(title, question))
            return;
        if (!remove(err))
            QMessageBox::warning(this, title, err);
        refresh();
        return;
    }
    const QolBackups::Info info = QolBackups::info(m_settings, backupKind);
    QMessageBox box(this);
    box.setWindowTitle(title);
    box.setText(question);
    box.setInformativeText(tr("A backup of %1 from %2 was found.")
                               .arg(info.title, info.when.toString(QStringLiteral("d MMM yyyy HH:mm"))));
    QAbstractButton *withRestore = box.addButton(tr("Remove and put my originals back"), QMessageBox::AcceptRole);
    QAbstractButton *plain = box.addButton(tr("Just remove"), QMessageBox::DestructiveRole);
    box.addButton(QMessageBox::Cancel);
    box.setDefaultButton(static_cast<QPushButton *>(withRestore));
    box.exec();
    if (box.clickedButton() != withRestore && box.clickedButton() != plain)
        return;
    // The put-back refuses while a game is open, so ask first: removing and
    // only then being told to close the game left the pack gone and nothing
    // of the player's back in its place.
    if (box.clickedButton() == withRestore) {
        if (const QString g = QolService::runningGame(); !g.isEmpty()) {
            QMessageBox::warning(this, title, tr("Close Plutonium first (%1 is running). Nothing was removed.").arg(g));
            return;
        }
    }
    if (!remove(err)) {
        QMessageBox::warning(this, title, err);
        refresh();
        return;
    }
    if (box.clickedButton() == withRestore) {
        runJob(tr("Putting back %1").arg(info.title),
               [this, backupKind](QString &e, const std::function<void(const QString &, int)> &p) {
                   return QolBackups::restore(m_settings, backupKind, p, e);
               });
        return;
    }
    refresh();
}

bool QolPage::installSeriesMod(const QString &gameCode)
{
    const QolService::SeriesMod m = QolService::seriesFor(gameCode);
    if (!m.released)
        return false;
    runJob(tr("Installing Quality of Life for %1").arg(m.gameTitle),
           [this, m](QString &err, const std::function<void(const QString &, int)> &p) {
               return QolService::installMod(m_settings, m, p, err);
           });
    return true;
}

QolPage::Row QolPage::addRow(QVBoxLayout *into, const QString &kicker, const QString &title, const QString &desc)
{
    Row r;
    r.card = new QFrame(this);
    r.card->setObjectName("Card");
    auto *h = new QHBoxLayout(r.card);
    h->setContentsMargins(18, 14, 18, 14);
    h->setSpacing(14);
    auto *col = new QVBoxLayout();
    col->setSpacing(3);
    auto *k = new QLabel(kicker, r.card);
    k->setObjectName("CardKicker");
    auto *t = new QLabel(title, r.card);
    t->setObjectName("CardTitle");
    col->addWidget(k);
    col->addWidget(t);
    if (!desc.isEmpty()) {
        auto *d = new QLabel(desc, r.card);
        d->setObjectName("CardDesc");
        d->setWordWrap(true);
        col->addWidget(d);
    }
    r.status = new QLabel(r.card);
    r.status->setObjectName("ModCardMeta");
    col->addWidget(r.status);
    h->addLayout(col, 1);
    r.secondary = new QPushButton(r.card);
    r.secondary->setCursor(Qt::PointingHandCursor);
    r.secondary->setMinimumHeight(36);
    r.primary = new QPushButton(r.card);
    r.primary->setProperty("cssClass", "primary");
    r.primary->setCursor(Qt::PointingHandCursor);
    r.primary->setMinimumHeight(36);
    r.primary->setMinimumWidth(110);
    h->addWidget(r.secondary, 0, Qt::AlignVCenter);
    h->addWidget(r.primary, 0, Qt::AlignVCenter);
    into->addWidget(r.card);
    return r;
}

void QolPage::refresh()
{
    const bool pluto = AppSettings::isPlutoniumRoot(m_settings.plutoniumInstance);
    m_plutoHint->setVisible(!pluto);
    m_plutoHint->setText(tr("Plutonium was not found at %1. Set its folder in Settings; installs are disabled until then.")
                             .arg(QDir::toNativeSeparators(m_settings.plutoniumInstance)));

    const auto series = QolService::series();
    for (int i = 0; i < m_modRows.size(); ++i) {
        const QolService::SeriesMod &m = series[i];
        Row &r = m_modRows[i];
        r.secondary->setText(tr("GitHub"));
        if (!m.released) {
            r.status->setText(tr("Not started yet. Black Ops II is the current focus."));
            r.primary->setText(tr("About"));
            r.primary->setEnabled(true);
            continue;
        }
        const QString have = QolService::installedModVersion(m_settings, m);
        const QString latest = m_latest.value(i);
        if (have.isEmpty()) {
            r.status->setText(latest.isEmpty() ? tr("Not installed.") : tr("Not installed. Latest is %1.").arg(latest));
            r.primary->setText(tr("Install"));
        } else if (VersionCompare::isUpdate(have, latest)) {
            // Only when the release is genuinely ahead. A tag that merely spells
            // the same version differently, or an install ahead of the release,
            // used to leave this row asking to update forever.
            r.status->setText(tr("Installed %1. Update %2 is available.").arg(have, latest));
            r.primary->setText(tr("Update"));
        } else {
            r.status->setText(tr("Installed %1.").arg(have));
            r.primary->setText(tr("Reinstall"));
        }
        r.primary->setEnabled(pluto);
    }

    const bool tex = QolService::packInstalled(m_settings, QolService::Pack::Textures);
    m_textures.status->setText(tex ? tr("Installed.") : tr("Not installed."));
    m_textures.primary->setText(tex ? tr("Remove") : tr("Install"));
    m_textures.primary->setProperty("cssClass", tex ? "danger" : "primary");
    m_textures.primary->setEnabled(pluto);
    m_textures.secondary->hide();

    const bool snd = QolService::packInstalled(m_settings, QolService::Pack::Sounds);
    m_sounds.status->setText(snd ? tr("Installed.") : tr("Not installed."));
    m_sounds.primary->setText(snd ? tr("Remove") : tr("Install"));
    m_sounds.primary->setProperty("cssClass", snd ? "danger" : "primary");
    m_sounds.primary->setEnabled(pluto);
    m_sounds.secondary->hide();

    const QString ctl = QolService::installedController(m_settings);
    const QString ctlName = ctl == QLatin1String("ps5") ? tr("PlayStation 5")
                          : ctl == QLatin1String("switch") ? tr("Nintendo Switch")
                          : ctl == QLatin1String("xbox") ? tr("Xbox") : ctl;
    m_controller.status->setText(ctl.isEmpty() ? tr("Game defaults (Xbox 360).") : tr("Installed: %1.").arg(ctlName));
    m_controller.primary->setText(tr("Choose"));
    m_controller.primary->setEnabled(pluto);
    m_controller.secondary->setText(tr("Reset"));
    m_controller.secondary->setVisible(!ctl.isEmpty());

    const bool rs = QolService::reShadeInstalled(m_settings);
    const bool addon = rs && QolService::addonReShadeActive(m_settings);
    const bool watchdog = GameLauncher::runningReShadeWatchdog() > 0;
    m_reshade.status->setText(!rs ? tr("Not installed.")
                              : addon ? (watchdog ? tr("Installed, add-on build (LAN). Watchdog running.")
                                                  : tr("Installed, add-on build (LAN)."))
                                      : (watchdog ? tr("Installed, stock build (online). Watchdog running.")
                                                  : tr("Installed, stock build (online).")));
    m_reshade.primary->setText(rs ? tr("Remove") : tr("Install"));
    m_reshade.primary->setProperty("cssClass", rs ? "danger" : "primary");
    m_reshade.primary->setEnabled(pluto);
    m_reshade.secondary->setText(watchdog ? tr("Stop watchdog") : tr("Start watchdog"));
    m_reshade.secondary->setVisible(rs);
    m_reshade.secondary->setEnabled(pluto);
    m_reshade.tertiary->setVisible(rs || watchdog);

    const bool dlss = QolService::dlssPayloadReady(m_settings);
    const QString installedVersion = QolService::installedDlssVersion(m_settings);
    // An install from before 2.3 has no version and may be missing files a
    // clean PC needs, so it is always offered the managed one.
    const bool update = dlss && m_dlssRelease.isValid()
                        && (installedVersion.isEmpty()
                            || VersionCompare::isUpdate(installedVersion, m_dlssRelease.version));
    if (!dlss)
        m_dlss.status->setText(tr("Not installed."));
    else if (update)
        m_dlss.status->setText(tr("Installed (%1). Version %2 is available.")
                                   .arg(QolService::dlssPayloadSummary(m_settings), m_dlssRelease.version));
    else
        m_dlss.status->setText(tr("Installed (%1). Runs in LAN games of Black Ops II; never online.")
                                   .arg(QolService::dlssPayloadSummary(m_settings)));
    m_dlss.primary->setText(dlss ? tr("Remove") : tr("Install"));
    m_dlss.primary->setProperty("cssClass", dlss ? "danger" : "primary");
    m_dlss.primary->setEnabled(pluto);
    m_dlss.secondary->setText(tr("Update"));
    m_dlss.secondary->setVisible(update);
    m_dlss.secondary->setEnabled(pluto);

    // Re-polish the buttons whose cssClass flipped.
    for (QPushButton *b : {m_textures.primary, m_sounds.primary, m_reshade.primary, m_dlss.primary}) {
        b->style()->unpolish(b);
        b->style()->polish(b);
    }

    refreshBackups();

    // Latest versions arrive later, off the UI thread, and only once per page life.
    static bool asked = false;
    if (!asked) {
        asked = true;
        const QPointer<QolPage> guard(this);
        auto future = QtConcurrent::run([guard, series]() {
            QStringList tags;
            for (const QolService::SeriesMod &m : series) {
                QString err;
                tags << (m.released ? QolService::latestModVersion(m, err) : QString());
            }
            QMetaObject::invokeMethod(qApp, [guard, tags]() {
                if (!guard)
                    return;
                guard->m_latest = tags;
                guard->refresh();
            }, Qt::QueuedConnection);
        });
        Q_UNUSED(future);
    }
    if (!m_dlssCheckStarted) {
        m_dlssCheckStarted = true;
        const QPointer<QolPage> guard(this);
        auto future = QtConcurrent::run([guard]() {
            QolService::DlssRelease release;
            QString err;
            QolService::latestDlssRelease(release, err);
            QMetaObject::invokeMethod(qApp, [guard, release]() {
                if (!guard) return;
                guard->m_dlssRelease = release;
                guard->refresh();
            }, Qt::QueuedConnection);
        });
        Q_UNUSED(future);
    }
}

void QolPage::refreshBackups()
{
    const bool pluto = AppSettings::isPlutoniumRoot(m_settings.plutoniumInstance);
    const QStringList kinds = QolBackups::kinds();
    int have = 0;
    int automatic = 0;
    for (int i = 0; i < m_backupRows.size(); ++i) {
        Row &r = m_backupRows[i];
        const QolBackups::Info info = QolBackups::info(m_settings, kinds[i]);
        const bool isAuto = QolBackups::automaticKinds().contains(kinds[i]);
        if (isAuto) {
            ++automatic;
            if (info.exists) ++have;
        }
        const QString now = kinds[i] == QLatin1String("controller") && info.liveFiles == 0
            ? tr("stock controller icons selected")
            : info.liveFiles == 0
            ? tr("nothing of yours there now")
            : tr("%1, %2 there now").arg(fmtFiles(info.liveFiles), fmtSize(info.liveBytes));
        r.status->setText(info.exists
                              ? tr("Backed up %1: %2, %3. (%4)")
                                    .arg(info.when.toString(QStringLiteral("d MMM yyyy HH:mm")),
                                         fmtFiles(info.files), fmtSize(info.bytes), now)
                              : tr("No backup yet. (%1)").arg(now));
        r.primary->setText(info.exists ? tr("Put back") : tr("Back up"));
        r.primary->setEnabled(pluto && (info.exists || info.liveFiles > 0 || kinds[i] == QLatin1String("controller")));
        r.secondary->setText(tr("Open"));
        r.secondary->setVisible(info.exists);
        r.tertiary->setEnabled(pluto);
    }
    m_backupAll.status->setText(have == 0 ? tr("Nothing backed up yet.")
                                          : tr("%1 of %2 kinds backed up.").arg(have).arg(automatic));
    m_backupAll.primary->setText(tr("Back up all"));
    m_backupAll.primary->setEnabled(pluto && have < automatic);
    m_backupAll.secondary->setText(tr("Put all back"));
    m_backupAll.secondary->setVisible(have > 0);
    m_backupAll.secondary->setEnabled(pluto);
    m_backupAuto->setEnabled(pluto);
    m_backupOpen->setEnabled(pluto);
}

void QolPage::runJob(const QString &title,
                     const std::function<bool(QString &, const std::function<void(const QString &, int)> &)> &job,
                     const std::function<void()> &after,
                     const std::function<void(bool)> &finished)
{
    auto *progress = new ProgressDialog(title, this);
    progress->setAttribute(Qt::WA_DeleteOnClose);
    progress->setIndeterminate(true);
    progress->setStatus(tr("Starting..."));
    progress->show();
    const QPointer<ProgressDialog> guard(progress);
    const QPointer<QolPage> self(this);
    auto future = QtConcurrent::run([self, guard, job, title, after, finished]() {
        auto report = [guard](const QString &text, int pct) {
            QMetaObject::invokeMethod(qApp, [guard, text, pct]() {
                if (!guard) return;
                guard->setStatus(text);
                if (pct < 0) guard->setIndeterminate(true);
                else guard->setProgress(pct, 100);
            }, Qt::QueuedConnection);
        };
        QString error;
        const bool ok = job(error, report);
        QMetaObject::invokeMethod(qApp, [self, guard, ok, error, title, after, finished]() {
            if (guard)
                guard->close();
            if (!self)
                return;
            if (finished) finished(ok);
            if (!ok)
                QMessageBox::warning(self, title, error.isEmpty() ? tr("The install failed.") : error);
            else if (after)
                after();
            self->refresh();
            emit self->installedChanged();
        }, Qt::QueuedConnection);
    });
    Q_UNUSED(future);
}

bool QolPage::confirm(const QString &title, const QString &text)
{
    return QMessageBox::question(this, title, text, QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
           == QMessageBox::Yes;
}

void QolPage::openUrl(const QString &url)
{
    QDesktopServices::openUrl(QUrl(url));
}

void QolPage::retranslate()
{
    m_intro->setText(tr("One quality of life mod per game, each in its own repository. Black Ops II is released; "
                        "the others start once it is finished. Everything installs into the Plutonium folder "
                        "set in Settings, and nothing touches the game's own files."));
    refresh();
}
