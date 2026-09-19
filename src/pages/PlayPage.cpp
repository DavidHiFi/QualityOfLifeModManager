#include "PlayPage.h"
#include "AppSettings.h"
#include "ArchiveTool.h"
#include "Downloader.h"
#include "GameCatalog.h"
#include "ProgressDialog.h"
#include "UpdateService.h"
#include "../Checkables.h"
#include "GameLauncher.h"
#include "Version.h"
#include <QCheckBox>

#include <QPainter>
#include <QLinearGradient>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QIcon>
#include "../Theme.h"
#include <QRadioButton>
#include <QButtonGroup>
#include <QStyle>
#include <QResizeEvent>
#include <QDateTime>
#include <QDialog>
#include <QTabWidget>
#include <QLineEdit>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMessageBox>
#include <QFuture>
#include <QPointer>
#include <QtConcurrent>

PlayPage::PlayPage(AppSettings &settings, QWidget *parent)
    : QWidget(parent)
    , m_settings(settings)
{
    setObjectName("PlayPage");

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(48, 40, 48, 40);
    root->addStretch(2);

    m_code = new QLabel(this);
    m_code->setObjectName("HeroCode");
    m_code->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_logo = new QLabel(this);
    m_logo->setObjectName("HeroLogo");
    m_logo->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_title = new QLabel(this);
    m_title->setObjectName("HeroTitle");
    m_title->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_sub = new QLabel(this);
    m_sub->setObjectName("HeroSub");
    m_sub->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_path = new QLabel(this);
    m_path->setObjectName("HeroPath");
    m_path->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_path->setWordWrap(true);

    root->addWidget(m_code);
    root->addWidget(m_logo);
    root->addWidget(m_title);
    root->addWidget(m_sub);
    root->addSpacing(8);
    root->addWidget(m_path);
    root->addSpacing(18);

    // Launch options: LAN (bootstrapper, the mod can be pre-loaded) or Online
    // (Plutonium's own launcher does the login), plus ReShade beside either.
    auto *opts = new QHBoxLayout();
    opts->setSpacing(0);
    m_lanBtn = new QPushButton(tr("LAN"), this);
    m_onlineBtn = new QPushButton(tr("Online"), this);
    for (QPushButton *b : {m_lanBtn, m_onlineBtn}) {
        b->setObjectName("SegmentButton");
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        b->setMinimumHeight(32);
        b->setMinimumWidth(84);
    }
    m_lanBtn->setProperty("segment", "left");
    m_onlineBtn->setProperty("segment", "right");
    auto *seg = new QButtonGroup(this);
    seg->addButton(m_lanBtn);
    seg->addButton(m_onlineBtn);
    (m_settings.launchOnline ? m_onlineBtn : m_lanBtn)->setChecked(true);
    opts->addWidget(m_lanBtn);
    opts->addWidget(m_onlineBtn);
    opts->addSpacing(16);
    m_reshade = new Ui::CheckBox(tr("ReShade"), this);
    m_reshade->setProperty("onArt", true);
    m_reshade->setChecked(m_settings.launchReShade);
    m_reshade->setToolTip(tr("Start the ReShade watchdog beside the game. Plutonium clears ReShade out of its bin folder on every start; the watchdog puts it back."));
    opts->addWidget(m_reshade);
    opts->addSpacing(12);
    m_modeHint = new QLabel(this);
    m_modeHint->setObjectName("HeroPath");
    m_modeHint->setWordWrap(true);
    opts->addWidget(m_modeHint, 1);
    root->addLayout(opts);
    root->addSpacing(10);
    connect(m_lanBtn, &QPushButton::toggled, this, [this](bool on) {
        if (!on) return;
        m_settings.launchOnline = false;
        m_settings.saveToIni();
        updateLaunchOptions();
    });
    connect(m_onlineBtn, &QPushButton::toggled, this, [this](bool on) {
        if (!on) return;
        m_settings.launchOnline = true;
        m_settings.saveToIni();
        updateLaunchOptions();
    });
    connect(m_reshade, &QCheckBox::toggled, this, [this](bool on) {
        m_settings.launchReShade = on;
        m_settings.saveToIni();
        updateLaunchOptions();
    });

    auto *row = new QHBoxLayout();
    m_spzm = new Ui::RadioButton(tr("Solo / Zombies"), this);
    m_mp = new Ui::RadioButton(tr("Multiplayer"), this);
    m_sp = new Ui::RadioButton(tr("Campanha"), this);
    m_sv = new Ui::RadioButton(tr("Survival"), this);
    for (auto *r : {m_spzm, m_mp, m_sp, m_sv})
        r->setProperty("onArt", true);
    m_spzm->setChecked(true);
    auto *modes = new QButtonGroup(this);
    modes->addButton(m_spzm);
    modes->addButton(m_mp);
    modes->addButton(m_sp);
    modes->addButton(m_sv);
    m_play = new QPushButton(tr("Iniciar"), this);
    m_play->setObjectName("PlayButton");
    m_play->setProperty("cssClass", "primary");
    m_play->setMinimumWidth(180);
    m_play->setFixedHeight(48);
    m_play->setCursor(Qt::PointingHandCursor);
    row->addWidget(m_spzm);
    row->addWidget(m_mp);
    row->addWidget(m_sp);
    row->addWidget(m_sv);
    row->addStretch();
    m_clientBtn = new QPushButton(this);
    m_clientBtn->setObjectName("ClientGearButton");
    m_clientBtn->setProperty("cssClass", "primary");
    m_clientBtn->setCursor(Qt::PointingHandCursor);
    m_clientBtn->setFixedSize(48, 48);
    m_clientBtn->setIcon(Theme::icon(QStringLiteral(":/icons/misc.svg")));
    m_clientBtn->setProperty("iconPath", ":/icons/misc.svg");
    m_clientBtn->setIconSize(QSize(20, 20));
    m_clientBtn->hide();
    connect(m_clientBtn, &QPushButton::clicked, this, &PlayPage::openClientPicker);
    row->addWidget(m_clientBtn, 0, Qt::AlignVCenter);
    row->addWidget(m_play, 0, Qt::AlignVCenter);
    root->addLayout(row);
    root->addStretch(1);

    connect(m_play, &QPushButton::clicked, this, [this]() {
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        if (m_running) {
            m_blockLaunchUntil = now + 2500;
            emit stopRequested(m_gameId);
            return;
        }
        if (now < m_blockLaunchUntil)
            return;
        const QString code = GameCatalog::byId(m_gameId).code;
        if (code == QLatin1String("s1")) {
            const QString exe = QDir(m_settings.gameFolder(m_gameId)).filePath(QStringLiteral("s1.exe"));
            if (!QFileInfo::exists(exe) || !m_settings.awClientReady) {
                const auto ans = QMessageBox::question(
                    this, tr("Advanced Warfare"),
                    tr("The launcher will download the recommended client to start the game."),
                    QMessageBox::Ok | QMessageBox::Cancel, QMessageBox::Ok);
                if (ans != QMessageBox::Ok)
                    return;
                installS1Client(false, true);
                return;
            }
        }
        if (code == QLatin1String("t7")
                && !m_settings.bo3ClientChosen) {
                const auto ans = QMessageBox::question(
                    this, tr("Black Ops III"),
                    tr("The launcher will download the recommended client to start the game."),
                    QMessageBox::Ok | QMessageBox::Cancel, QMessageBox::Ok);
                if (ans != QMessageBox::Ok)
                    return;
                installBo3Client(QStringLiteral("cll"), false, true);
                return;
        }
        const bool plutoGame = code == QLatin1String("t4") || code == QLatin1String("t5")
                            || code == QLatin1String("t6") || code == QLatin1String("iw5");
        if (plutoGame && m_settings.launchOnline)
            emit launchOnlineRequested(m_gameId, selectedMode());
        else
            emit launchRequested(m_gameId, selectedMode());
    });

    auto refreshOnToggle = [this](bool on) {
        if (on) {
            refreshArt();
            update();
        }
    };
    connect(m_mp, &QRadioButton::toggled, this, refreshOnToggle);
    connect(m_spzm, &QRadioButton::toggled, this, refreshOnToggle);
    connect(m_sp, &QRadioButton::toggled, this, refreshOnToggle);
    connect(m_sv, &QRadioButton::toggled, this, refreshOnToggle);

    setGame("Black ops II");
}

bool PlayPage::isMultiplayer() const
{
    return selectedMode() == QLatin1String("mp");
}

QString PlayPage::selectedMode() const
{
    if (m_sv && m_sv->isVisible() && m_sv->isChecked())
        return QStringLiteral("sv");
    if (m_sp && m_sp->isVisible() && m_sp->isChecked())
        return QStringLiteral("sp");
    if (m_mp && m_mp->isChecked())
        return QStringLiteral("mp");
    return QStringLiteral("zm");
}

void PlayPage::setGame(const QString &gameId)
{
    m_gameId = gameId;
    const auto g = GameCatalog::byId(gameId);
    m_code->setText(g.shortLabel);
    m_title->setText(g.title.toUpper());
    m_sub->setText(g.subtitle);
    const QString folder = m_settings.gameFolder(gameId);
    m_path->setText(folder.isEmpty()
                        ? tr("Pasta do jogo nao configurada — abra Configuracoes.")
                        : folder);
    const bool s1 = (g.code == QLatin1String("s1"));
    const bool t7 = (g.code == QLatin1String("t7"));
    m_spzm->setVisible(g.hasZmSp && !t7);
    m_spzm->setText(s1 ? tr("Zombies") : tr("Solo / Zombies"));
    if (m_mp)
        m_mp->setVisible(!t7);
    if (m_sp)
        m_sp->setVisible(s1);
    if (m_sv)
        m_sv->setVisible(s1);
    if (!g.hasZmSp)
        m_mp->setChecked(true);
    updateClientButton();
    updateLaunchOptions();
    refreshArt();
    update();
}

void PlayPage::updateLaunchOptions()
{
    if (!m_lanBtn)
        return;
    const QString code = GameCatalog::byId(m_gameId).code;
    const bool plutoGame = code == QLatin1String("t4") || code == QLatin1String("t5")
                        || code == QLatin1String("t6") || code == QLatin1String("iw5");
    m_lanBtn->setVisible(plutoGame);
    m_onlineBtn->setVisible(plutoGame);
    m_reshade->setVisible(plutoGame);
    if (!plutoGame) {
        m_modeHint->clear();
        return;
    }
    const bool online = m_settings.launchOnline;
    const bool reshadeReady = GameLauncher::reShadeInstalled(m_settings.plutoniumInstance);
    QString hint;
    if (online)
        hint = tr("Plutonium's launcher signs in and starts the game. Pick a mod from its Mods menu in game.");
    else
        hint = tr("No login needed. The mod selected on the Mods page loads automatically.");
    if (m_settings.launchReShade && !reshadeReady)
        hint += QStringLiteral("  ") + tr("ReShade is not installed yet: see the Quality of Life page.");
    m_modeHint->setText(hint);
    m_play->setText(m_running ? tr("Encerrar") : (online ? tr("Play online") : tr("Iniciar")));
}

void PlayPage::setRunning(bool running)
{
    m_running = running;
    if (running) {
        m_play->setText(tr("Encerrar"));
        m_play->setProperty("cssClass", "danger");
        m_play->setObjectName("StopButton");
    } else {
        m_play->setText(m_settings.launchOnline ? tr("Play online") : tr("Iniciar"));
        m_play->setProperty("cssClass", "primary");
        m_play->setObjectName("PlayButton");
    }
    m_play->style()->unpolish(m_play);
    m_play->style()->polish(m_play);
    m_play->setFixedHeight(48);
    if (m_clientBtn)
        m_clientBtn->setFixedSize(48, 48);
}

void PlayPage::refreshArt()
{
    const auto g = GameCatalog::byCode(GameCatalog::byId(m_gameId).code);
    const QSize bgSize = size().isEmpty() ? QSize(1600, 900) : size();
    const QString variant = (GameCatalog::byId(m_gameId).code == QLatin1String("t7"))
                                ? QStringLiteral("mp")
                                : selectedMode();
    m_bg = GameCatalog::background(g.code, bgSize, variant);

    const int logoW = qMax(240, qMin(width() > 0 ? width() - 120 : 480, 520));
    const QPixmap logoPm = GameCatalog::logo(g.code, QSize(logoW, 104));
    if (!logoPm.isNull()) {
        m_logo->setPixmap(logoPm);
        m_logo->show();
        m_title->hide();
    } else {
        m_logo->hide();
        m_title->show();
        m_title->setText(g.title.toUpper());
    }
}

void PlayPage::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    refreshArt();
}

void PlayPage::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    if (!m_bg.isNull()) {
        const QPixmap scaled = m_bg.scaled(size(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
        const int x = (width() - scaled.width()) / 2;
        const int y = (height() - scaled.height()) / 2;
        p.drawPixmap(x, y, scaled);
    } else {
        p.fillRect(rect(), QColor("#101114"));
    }
    QLinearGradient g(0, 0, width() * 0.7, height());
    g.setColorAt(0.0, QColor(10, 11, 14, 40));
    g.setColorAt(0.45, QColor(10, 11, 14, 160));
    g.setColorAt(1.0, QColor(10, 11, 14, 230));
    p.fillRect(rect(), g);

    QLinearGradient bottom(0, height() - 180, 0, height());
    bottom.setColorAt(0, QColor(10, 11, 14, 0));
    bottom.setColorAt(1, QColor(10, 11, 14, 210));
    p.fillRect(QRect(0, height() - 180, width(), 180), bottom);
}

void PlayPage::updateClientButton()
{
    if (!m_clientBtn)
        return;
    const bool t7 = GameCatalog::byId(m_gameId).code == QLatin1String("t7");
    m_clientBtn->setVisible(t7);
    if (!t7)
        return;
    m_clientBtn->setText(QString());
    m_clientBtn->setToolTip(m_settings.bo3Client == QLatin1String("competitive")
                                ? tr("BO3 client: Competitive - (Boiii-Community)")
                                : tr("BO3 client: T7-CLL"));
}

void PlayPage::openClientPicker()
{
    const QString gameDir = m_settings.gameFolder(m_gameId);
    if (gameDir.isEmpty() || !QDir(gameDir).exists()) {
        QMessageBox::warning(this, tr("Black Ops III"),
                             tr("Set the Black Ops III folder in Settings first."));
        return;
    }

    QDialog dlg(this);
    dlg.setWindowTitle(tr("BO3 client"));
    dlg.setMinimumWidth(560);
    auto *root = new QVBoxLayout(&dlg);
    root->setContentsMargins(16, 16, 16, 14);
    root->setSpacing(10);

    auto *tabs = new QTabWidget(&dlg);

    auto *clientsPage = new QWidget(&dlg);
    auto *clientsLay = new QVBoxLayout(clientsPage);
    clientsLay->setContentsMargins(8, 12, 8, 8);
    clientsLay->setSpacing(10);

    auto *hint = new QLabel(tr("The files are downloaded into the game folder.\nCurrent: %1")
                                .arg(gameDir), clientsPage);
    hint->setObjectName("CardDesc");
    hint->setWordWrap(true);
    clientsLay->addWidget(hint);

    auto addOption = [&](const QString &id, const QString &name, const QString &desc) {
        auto *card = new QFrame(clientsPage);
        card->setObjectName("Card");
        card->setCursor(Qt::PointingHandCursor);
        auto *v = new QVBoxLayout(card);
        v->setContentsMargins(16, 14, 16, 14);
        auto *n = new QLabel(name, card);
        n->setObjectName("CardTitle");
        auto *d = new QLabel(desc, card);
        d->setObjectName("CardDesc");
        d->setWordWrap(true);
        v->addWidget(n);
        v->addWidget(d);
        if (m_settings.bo3Client == id) {
            auto *cur = new QLabel(tr("Selected"), card);
            cur->setObjectName("ModCardInstalled");
            v->addWidget(cur);
        }
        card->setProperty("clientId", id);
        clientsLay->addWidget(card);
        return card;
    };

    auto *cll = addOption(QStringLiteral("cll"),
                          tr("T7-CLL"),
                          tr("Nosso launcher padrão, modificado para recursos lan e sem marca dagua"));
    auto *comp = addOption(QStringLiteral("competitive"),
                           tr("Competitive - (Boiii-Community)"),
                           tr("Community client for speedrun and records. Only boiii.exe is downloaded; the rest is fetched by the game."));
    clientsLay->addStretch();

    auto *argsPage = new QWidget(&dlg);
    auto *argsLay = new QVBoxLayout(argsPage);
    argsLay->setContentsMargins(8, 12, 8, 8);
    argsLay->setSpacing(10);
    auto *argsHint = new QLabel(tr("These arguments are passed when starting the selected client."), argsPage);
    argsHint->setObjectName("CardDesc");
    argsHint->setWordWrap(true);
    argsLay->addWidget(argsHint);

    auto *cllTitle = new QLabel(tr("T7-CLL"), argsPage);
    cllTitle->setObjectName("CardTitle");
    auto *cllEdit = new QLineEdit(m_settings.bo3CllArgs, argsPage);
    cllEdit->setPlaceholderText(QStringLiteral("-launch -noconsole -nowatermark -nointro"));
    argsLay->addWidget(cllTitle);
    argsLay->addWidget(cllEdit);

    auto *compTitle = new QLabel(tr("Competitive - (Boiii-Community)"), argsPage);
    compTitle->setObjectName("CardTitle");
    auto *compEdit = new QLineEdit(m_settings.bo3CompArgs, argsPage);
    compEdit->setPlaceholderText(QStringLiteral("-nointro"));
    argsLay->addWidget(compTitle);
    argsLay->addWidget(compEdit);

    auto *resetBtn = new QPushButton(tr("Reset"), argsPage);
    connect(resetBtn, &QPushButton::clicked, &dlg, [cllEdit, compEdit]() {
        cllEdit->setText(QStringLiteral("-launch -noconsole -nowatermark -nointro"));
        compEdit->setText(QStringLiteral("-nointro"));
    });
    argsLay->addWidget(resetBtn, 0, Qt::AlignLeft);
    argsLay->addStretch();

    tabs->addTab(clientsPage, tr("Clients"));
    tabs->addTab(argsPage, tr("Arguments"));
    root->addWidget(tabs, 1);

    auto pick = [&](const QString &id) {
        dlg.setProperty("picked", id);
        dlg.accept();
    };
    cll->setFocusPolicy(Qt::StrongFocus);
    comp->setFocusPolicy(Qt::StrongFocus);

    class ClickFilter : public QObject {
    public:
        std::function<void(QString)> onPick;
        bool eventFilter(QObject *obj, QEvent *ev) override
        {
            if (ev->type() == QEvent::MouseButtonRelease) {
                const QString id = obj->property("clientId").toString();
                if (!id.isEmpty() && onPick)
                    onPick(id);
                return true;
            }
            return QObject::eventFilter(obj, ev);
        }
    };
    auto *filter = new ClickFilter();
    filter->onPick = pick;
    cll->installEventFilter(filter);
    comp->installEventFilter(filter);
    filter->setParent(&dlg);

    auto *cancel = new QPushButton(tr("Cancelar"), &dlg);
    auto *save = new QPushButton(tr("Salvar"), &dlg);
    save->setProperty("cssClass", "primary");
    connect(cancel, &QPushButton::clicked, &dlg, &QDialog::reject);
    connect(save, &QPushButton::clicked, &dlg, [&]() {
        dlg.setProperty("saved", true);
        dlg.reject();
    });
    auto *row = new QHBoxLayout();
    row->addStretch();
    row->addWidget(cancel);
    row->addWidget(save);
    root->addLayout(row);

    auto saveArgs = [&]() {
        m_settings.bo3CllArgs = cllEdit->text().trimmed();
        m_settings.bo3CompArgs = compEdit->text().trimmed();
        m_settings.saveToIni();
    };
    if (dlg.exec() != QDialog::Accepted) {
        if (dlg.property("saved").toBool())
            saveArgs();
        return;
    }
    saveArgs();
    const QString picked = dlg.property("picked").toString();
    if (picked.isEmpty())
        return;
    installBo3Client(picked, true, false);
}

void PlayPage::installBo3Client(const QString &kind, bool notifyWhenDone, bool launchWhenReady)
{
    const QString gameDir = QDir::cleanPath(m_settings.gameFolder(m_gameId));
    if (gameDir.isEmpty())
        return;

    auto *progress = new ProgressDialog(tr("Installing BO3 client"), this);
    progress->setAttribute(Qt::WA_DeleteOnClose);
    progress->setIndeterminate(true);
    progress->setStatus(tr("Starting download..."));
    progress->show();

    const QPointer<ProgressDialog> guard(progress);
    const QString job = kind;
    const QFuture<void> future = QtConcurrent::run([this, gameDir, job, guard, notifyWhenDone, launchWhenReady]() {
        auto report = [&](const QString &text, int pct) {
            QMetaObject::invokeMethod(this, [guard, text, pct]() {
                if (!guard)
                    return;
                guard->setStatus(text);
                if (pct < 0)
                    guard->setIndeterminate(true);
                else
                    guard->setProgress(pct, 100);
            }, Qt::QueuedConnection);
        };

        QString error;
        bool ok = false;
        if (job == QLatin1String("cll")) {
            report(QObject::tr("Downloading T7-CLL..."), 8);
            const QString zip = QDir::temp().filePath(
                QStringLiteral("LanLauncher_t7_cll_%1.zip").arg(QDateTime::currentMSecsSinceEpoch()));
            ok = Downloader::downloadToFile(
                QStringLiteral("https://github.com/MestreTM/t7-cll/releases/latest/download/t7_cll.zip"),
                zip, error,
                [&](qint64 got, qint64 total) {
                    const int pct = (total > 0) ? int(got * 70 / total) : 10;
                    report(QObject::tr("Downloading T7-CLL..."), pct);
                },
                10 * 60 * 1000);
            if (ok) {
                report(QObject::tr("Extracting into the game folder..."), 80);
                const QString zipMd5Path = zip;
                ok = ArchiveTool::extractToDirectory(zip, gameDir, &error);
                if (ok)
                    UpdateService::stampFile(QStringLiteral("t7-cll"), QString(), zipMd5Path);
            }
            QFile::remove(zip);
        } else {
            report(QObject::tr("Reading competitive updater..."), 5);
            const QByteArray raw = Downloader::downloadBytes(
                QStringLiteral("https://gitlab.com/boiii-community/BOIII-Community/-/raw/main/updater.json?ref_type=heads"),
                error, 30000);
            QString exeUrl;
            const auto doc = QJsonDocument::fromJson(raw);
            for (const QJsonValue &row : doc.array()) {
                const QJsonArray a = row.toArray();
                if (a.size() < 4)
                    continue;
                if (a.at(0).toString().compare(QLatin1String("boiii.exe"), Qt::CaseInsensitive) == 0) {
                    exeUrl = a.at(3).toString();
                    break;
                }
            }
            if (exeUrl.isEmpty()) {
                ok = false;
                if (error.isEmpty())
                    error = QObject::tr("Could not find boiii.exe in the competitive updater.");
            } else {
                report(QObject::tr("Downloading boiii.exe..."), 20);
                const QString dest = QDir(gameDir).filePath(QStringLiteral("boiii.exe"));
                ok = Downloader::downloadToFile(exeUrl, dest, error, [&](qint64 got, qint64 total) {
                    const int pct = (total > 0) ? int(20 + got * 70 / total) : 30;
                    report(QObject::tr("Downloading boiii.exe..."), pct);
                }, 10 * 60 * 1000);
                if (ok)
                    UpdateService::stampFile(QStringLiteral("boiii-community"), QString(), dest);
            }
        }

        QMetaObject::invokeMethod(this, [this, guard, ok, error, job, notifyWhenDone, launchWhenReady]() {
            if (ok) {
                m_settings.bo3Client = job;
                m_settings.bo3ClientChosen = true;
                m_settings.saveToIni();
                updateClientButton();
            }
            if (ok && !notifyWhenDone) {
                if (guard)
                    guard->close();
                if (launchWhenReady
                    && QDateTime::currentMSecsSinceEpoch() >= m_blockLaunchUntil)
                    emit launchRequested(m_gameId, selectedMode());
                return;
            }
            if (guard)
                guard->close();
            if (ok && notifyWhenDone)
                QMessageBox::information(this, tr("Black Ops III"),
                                         tr("Download complete. The client is ready in the game folder."));
            else if (!ok)
                QMessageBox::warning(this, tr("Black Ops III"),
                                     error.isEmpty() ? tr("Failed to install the client.") : error);
        }, Qt::QueuedConnection);
    });
    Q_UNUSED(future);
}

void PlayPage::installS1Client(bool notifyWhenDone, bool launchWhenReady)
{
    const QString gameDir = QDir::cleanPath(m_settings.gameFolder(m_gameId));
    if (gameDir.isEmpty())
        return;

    auto *progress = new ProgressDialog(tr("Installing AW client"), this);
    progress->setAttribute(Qt::WA_DeleteOnClose);
    progress->setIndeterminate(true);
    progress->setStatus(tr("Starting download..."));
    progress->show();

    const QPointer<ProgressDialog> guard(progress);
    const QFuture<void> future = QtConcurrent::run([this, gameDir, guard, notifyWhenDone, launchWhenReady]() {
        auto report = [&](const QString &text, int pct) {
            QMetaObject::invokeMethod(this, [guard, text, pct]() {
                if (!guard)
                    return;
                guard->setStatus(text);
                if (pct < 0)
                    guard->setIndeterminate(true);
                else
                    guard->setProgress(pct, 100);
            }, Qt::QueuedConnection);
        };

        QString error;
        report(QObject::tr("Downloading S1-CLL..."), 8);
        const QString dest = QDir(gameDir).filePath(QStringLiteral("s1.exe"));
        const bool ok = Downloader::downloadToFile(
            QStringLiteral("https://github.com/MestreTM/s1-cll/releases/latest/download/s1.exe"),
            dest, error,
            [&](qint64 got, qint64 total) {
                const int pct = (total > 0) ? int(got * 90 / total) : 15;
                report(QObject::tr("Downloading S1-CLL..."), pct);
            },
            10 * 60 * 1000);
        if (ok)
            UpdateService::stampFile(QStringLiteral("s1-cll"), QString(), dest);

        QMetaObject::invokeMethod(this, [this, guard, ok, error, notifyWhenDone, launchWhenReady]() {
            if (ok) {
                m_settings.awClientReady = true;
                m_settings.saveToIni();
            }
            if (ok && !notifyWhenDone) {
                if (guard)
                    guard->close();
                if (launchWhenReady
                    && QDateTime::currentMSecsSinceEpoch() >= m_blockLaunchUntil)
                    emit launchRequested(m_gameId, selectedMode());
                return;
            }
            if (guard)
                guard->close();
            if (ok && notifyWhenDone)
                QMessageBox::information(this, tr("Advanced Warfare"),
                                         tr("Download complete. The client is ready in the game folder."));
            else if (!ok)
                QMessageBox::warning(this, tr("Advanced Warfare"),
                                     error.isEmpty() ? tr("Failed to install the client.") : error);
        }, Qt::QueuedConnection);
    });
    Q_UNUSED(future);
}

void PlayPage::retranslate()
{
    if (m_lanBtn) m_lanBtn->setText(tr("LAN"));
    if (m_onlineBtn) m_onlineBtn->setText(tr("Online"));
    if (m_reshade) m_reshade->setText(tr("ReShade"));
    if (m_mp)
        m_mp->setText(tr("Multiplayer"));
    if (m_sp)
        m_sp->setText(tr("Campanha"));
    if (m_sv)
        m_sv->setText(tr("Survival"));
    const QString code = GameCatalog::byId(m_gameId).code;
    if (m_spzm)
        m_spzm->setText(code == QLatin1String("s1") ? tr("Zombies") : tr("Solo / Zombies"));
    setRunning(m_running);
    if (!m_gameId.isEmpty())
        setGame(m_gameId);
}
