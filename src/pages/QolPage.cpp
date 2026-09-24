#include "QolPage.h"
#include "VersionCompare.h"
#include "AppSettings.h"
#include "GameLauncher.h"
#include "ProgressDialog.h"
#include "QolService.h"
#include "Version.h"
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
    m_dlss = addRow(root, tr("LAN ONLY"), tr("DLSS 5 Neural Rendering"),
                    tr("One click downloads and checks the DLSS 5 payload for Black Ops II. The manager enables "
                       "it for LAN play and removes its add-on, helper and neural preset before online play. "
                       "Needs an RTX card; the download is about 230 MB."));
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
            if (!confirm(tr("HD texture pack"), tr("Remove the HD texture pack?")))
                return;
            QString err;
            if (!QolService::removePack(m_settings, QolService::Pack::Textures, err))
                QMessageBox::warning(this, tr("HD texture pack"), err);
            refresh();
            return;
        }
        runJob(tr("Installing the HD texture pack"),
               [this](QString &err, const std::function<void(const QString &, int)> &p) {
                   return QolService::installPack(m_settings, QolService::Pack::Textures, p, err);
               });
    });
    connect(m_sounds.primary, &QPushButton::clicked, this, [this]() {
        if (QolService::packInstalled(m_settings, QolService::Pack::Sounds)) {
            if (!confirm(tr("Custom sounds"), tr("Remove the custom sound pack?")))
                return;
            QString err;
            if (!QolService::removePack(m_settings, QolService::Pack::Sounds, err))
                QMessageBox::warning(this, tr("Custom sounds"), err);
            refresh();
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
        if (!confirm(tr("Controller icons"), tr("Go back to the game's own button icons?")))
            return;
        QString err;
        if (!QolService::removeController(m_settings, err))
            QMessageBox::warning(this, tr("Controller icons"), err);
        refresh();
    });
    connect(m_reshade.primary, &QPushButton::clicked, this, [this]() {
        QString err;
        if (QolService::reShadeInstalled(m_settings)) {
            if (!confirm(tr("ReShade"), tr("Remove ReShade from Plutonium's bin folder?")))
                return;
            // Remove means gone: the add-ons, the 64-bit host and the preset
            // entries that name them go with it, not just dxgi.dll.
            if (!QolService::ensureReShadeAbsent(m_settings, err))
                QMessageBox::warning(this, tr("ReShade"), err);
        } else if (!QolService::installReShade(m_settings, err)) {
            QMessageBox::warning(this, tr("ReShade"), err);
        }
        refresh();
    });
    connect(m_reshade.secondary, &QPushButton::clicked, this, [this]() {
        QString err;
        if (!GameLauncher::startReShadeWatchdog(m_settings.plutoniumInstance, err))
            QMessageBox::warning(this, tr("ReShade"), err);
    });
    connect(m_dlss.primary, &QPushButton::clicked, this, [this]() {
        if (QolService::dlssPayloadReady(m_settings)) {
            runJob(tr("Removing DLSS 5"), [this](QString &err, const QolService::Progress &) {
                return QolService::removeDlssPayload(m_settings, err);
            }, [this](bool ok) {
                if (ok) {
                    m_settings.launchDlss5 = false;
                    m_settings.saveToIni();
                }
            });
            return;
        }
        runJob(tr("Installing DLSS 5"), [this](QString &err, const QolService::Progress &p) {
            QolService::DlssRelease release;
            if (!QolService::latestDlssRelease(release, err)) return false;
            return QolService::installDlssPayload(m_settings, release, p, err);
        }, [this](bool ok) {
            if (ok) {
                m_settings.launchReShade = true;
                m_settings.launchDlss5 = true;
                m_settings.saveToIni();
            }
        });
    });
    connect(m_dlss.secondary, &QPushButton::clicked, this, [this]() {
        runJob(tr("Updating DLSS 5"), [this](QString &err, const QolService::Progress &p) {
            QolService::DlssRelease release;
            if (!QolService::latestDlssRelease(release, err)) return false;
            return QolService::installDlssPayload(m_settings, release, p, err);
        });
    });

    retranslate();
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
    m_reshade.status->setText(!rs ? tr("Not installed.")
                              : addon ? tr("Installed, add-on build (LAN).")
                                      : tr("Installed, stock build (online)."));
    m_reshade.primary->setText(rs ? tr("Remove") : tr("Install"));
    m_reshade.primary->setProperty("cssClass", rs ? "danger" : "primary");
    m_reshade.primary->setEnabled(pluto);
    m_reshade.secondary->setText(tr("Start watchdog"));
    m_reshade.secondary->setVisible(rs);

    const bool dlss = QolService::dlssPayloadReady(m_settings);
    m_dlss.status->setText(dlss ? tr("Installed: %1 Used on LAN launches of Black Ops II.")
                                      .arg(QolService::dlssPayloadSummary(m_settings))
                                : tr("Not installed. Press Install to download the verified payload."));
    m_dlss.primary->setText(dlss ? tr("Remove") : tr("Install"));
    m_dlss.primary->setProperty("cssClass", dlss ? "danger" : "primary");
    m_dlss.primary->setEnabled(pluto);
    const QString installedVersion = QolService::installedDlssVersion(m_settings);
    const bool update = dlss && m_dlssRelease.isValid()
                        && (installedVersion.isEmpty()
                            || VersionCompare::isUpdate(installedVersion, m_dlssRelease.version));
    m_dlss.secondary->setText(tr("Update to %1").arg(m_dlssRelease.version));
    m_dlss.secondary->setVisible(update);
    m_dlss.secondary->setEnabled(pluto);

    // Re-polish the buttons whose cssClass flipped.
    for (QPushButton *b : {m_textures.primary, m_sounds.primary, m_reshade.primary, m_dlss.primary}) {
        b->style()->unpolish(b);
        b->style()->polish(b);
    }

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

void QolPage::runJob(const QString &title,
                     const std::function<bool(QString &, const std::function<void(const QString &, int)> &)> &job,
                     const std::function<void(bool)> &finished)
{
    auto *progress = new ProgressDialog(title, this);
    progress->setAttribute(Qt::WA_DeleteOnClose);
    progress->setIndeterminate(true);
    progress->setStatus(tr("Starting..."));
    progress->show();
    const QPointer<ProgressDialog> guard(progress);
    const QPointer<QolPage> self(this);
    auto future = QtConcurrent::run([self, guard, job, title, finished]() {
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
        QMetaObject::invokeMethod(qApp, [self, guard, ok, error, title, finished]() {
            if (guard)
                guard->close();
            if (!self)
                return;
            if (finished) finished(ok);
            if (!ok)
                QMessageBox::warning(self, title, error.isEmpty() ? tr("The install failed.") : error);
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
