#include "MainWindow.h"
#include "LoginDialog.h"
#include "PlutoniumAuth.h"
#include "AnimatedLogo.h"
#include "Dialogs.h"
#include "GameCatalog.h"
#include "GameLauncher.h"
#include "SetupWizard.h"
#include "Theme.h"
#include "I18n.h"
#include "UpdateService.h"
#include "SelfUpdate.h"
#include "Version.h"

#include "pages/PlayPage.h"
#include "pages/HomePage.h"
#include "pages/QolPage.h"
#include "QolService.h"
#include "pages/ModsPage.h"
#include "pages/ServerPage.h"
#include "pages/SettingsPage.h"
#include "pages/AboutPage.h"

#include <QApplication>
#include <QDesktopServices>
#include <QIcon>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QStackedWidget>
#include <QUrl>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QCloseEvent>
#include <QDir>
#include <QResizeEvent>
#include <QtConcurrent/QtConcurrent>

namespace {

class GameGrip : public QWidget
{
public:
    explicit GameGrip(QWidget *parent = nullptr) : QWidget(parent)
    {
        setFixedSize(14, 28);
        setCursor(Qt::SizeAllCursor);
        setObjectName("GameNavGrip");
    }
protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        const QColor c = Theme::color("TEXT_FAINT");
        p.setBrush(c);
        p.setPen(Qt::NoPen);
        for (int row = 0; row < 3; ++row) {
            for (int col = 0; col < 2; ++col) {
                p.drawRoundedRect(QRectF(2 + col * 6, 6 + row * 7, 4, 4), 1.2, 1.2);
            }
        }
    }
};

class DimOverlay : public QWidget
{
public:
    QRect hole;
    explicit DimOverlay(QWidget *parent = nullptr) : QWidget(parent)
    {
        setAttribute(Qt::WA_NoSystemBackground, false);
        setAutoFillBackground(false);
    }
protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        QPainterPath dim;
        dim.addRect(rect());
        QPainterPath cut;
        const int r = Theme::radius() ? 12 : 0;
        cut.addRoundedRect(QRectF(hole).adjusted(-8, -8, 8, 8), r, r);
        dim = dim.subtracted(cut);
        p.fillPath(dim, QColor(8, 8, 14, 168));
        p.setPen(QPen(QColor(Theme::accent()), 1.6));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(QRectF(hole).adjusted(-8, -8, 8, 8), r, r);
    }
};

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    m_settings = AppSettings::loadForStartup();
    I18n::apply(m_settings.language);
    Theme::apply(m_settings.theme);

    if (!m_settings.setupCompleted) {
        SetupWizard wizard(m_settings, nullptr);
        wizard.exec();
        Theme::apply(m_settings.theme);
    }

    setWindowTitle(tr(QOL_APP_NAME));
    setWindowIcon(QIcon(":/icons/icon.ico"));
    resize(1180, 720);
    // Six game rows plus five tools plus brand and footer: below this the
    // bottom game row gets squeezed.
    // 720 used to be the floor because the sidebar could not give way below
    // it; a window a few pixels short of that clipped the last game row. The
    // nav column scrolls now, so a shorter window is a real size, not a bug.
    setMinimumSize(960, 640);

    auto *central = new QWidget(this);
    auto *outer = new QHBoxLayout(central);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);
    outer->addWidget(buildSidebar());

    m_stack = new QStackedWidget(this);
    m_playPage = new PlayPage(m_settings, this);
    m_homePage = new HomePage(m_settings, this);
    m_qolPage = new QolPage(m_settings, this);
    m_modsPage = new ModsPage(m_settings, this);
    m_serverPage = new ServerPage(m_settings, this);
    m_settingsPage = new SettingsPage(m_settings, this);
    m_aboutPage = new AboutPage(this);
    m_stack->addWidget(m_playPage);
    m_stack->addWidget(m_homePage);
    m_stack->addWidget(m_qolPage);
    m_stack->addWidget(m_modsPage);
    m_stack->addWidget(m_serverPage);
    m_stack->addWidget(m_settingsPage);
    m_stack->addWidget(m_aboutPage);

    auto *right = new QWidget(this);
    auto *rl = new QVBoxLayout(right);
    rl->setContentsMargins(0, 0, 0, 0);
    rl->setSpacing(0);
    rl->addWidget(buildHeader());
    rl->addWidget(m_stack, 1);

    outer->addWidget(right, 1);
    setCentralWidget(central);

    m_organizeOverlay = new DimOverlay(this);
    m_organizeOverlay->hide();
    m_organizeOverlay->installEventFilter(this);

    connect(&ThemeHub::instance(), &ThemeHub::themeChanged, this, [this]() {
        for (auto *b : findChildren<QPushButton*>()) {
            const QString path = b->property("iconPath").toString();
            if (!path.isEmpty() && !b->property("rawIcon").toBool())
                b->setIcon(Theme::icon(path));
        }
    });

    connect(m_homePage, &HomePage::installRequested, this, [this](const QString &modId) {
        // Series mods ship plain release zips, not a CLL manifest: the
        // Quality of Life page owns those installs.
        for (const QolService::SeriesMod &sm : QolService::series()) {
            if (sm.released && modId == QStringLiteral("%1-quality-of-life").arg(sm.gameCode)) {
                showTool(ToolQol);
                m_qolPage->installSeriesMod(sm.gameCode);
                return;
            }
        }
        showTool(ToolMods);
        if (!modId.isEmpty())
            m_modsPage->installFromCatalog(modId);
    });
    connect(m_homePage, &HomePage::gameRequested, this, [this](const QString &gameId) {
        m_modsPage->selectGame(gameId);
        m_homePage->selectGame(gameId);
    });
    connect(m_playPage, &PlayPage::launchRequested, this, &MainWindow::onLaunchGame);
    connect(m_playPage, &PlayPage::launchOnlineRequested, this, &MainWindow::onLaunchOnline);
    connect(m_qolPage, &QolPage::installedChanged, this, [this]() {
        m_modsPage->refreshList();
        if (m_homePage)
            m_homePage->refreshInstallState();
    });
    connect(m_playPage, &PlayPage::stopRequested, this, &MainWindow::onStopGame);
    connect(m_modsPage, &ModsPage::selectionChanged, this, [this]() {
        m_playPage->setSelectedMod(m_modsPage->selectedMod());
    });
    connect(m_modsPage, &ModsPage::catalogChanged, this, [this]() {
        if (m_homePage)
            m_homePage->refreshInstallState();
    });
    connect(m_settingsPage, &SettingsPage::plutoniumFolderChanged, this, [this]() {
        m_modsPage->refreshList();
        m_qolPage->refresh();
        m_serverPage->refreshList();
        if (m_homePage)
            m_homePage->refreshInstallState();
    });
    connect(m_settingsPage, &SettingsPage::homeEnabledChanged, this, [this](bool) {
        applyHomeVisibility();
    });
    connect(m_settingsPage, &SettingsPage::checkUpdatesRequested, this, [this]() {
        // A manual check is the user saying "try it anyway", so clear whatever
        // the last failed swap left behind before asking.
        SelfUpdate::forget();
        if (UpdateService::prompt(this, m_settings, false))
            SelfUpdate::quitForSwap();
    });
    connect(m_settingsPage, &SettingsPage::cancelled, this, [this]() {
        if (m_settings.homeEnabled)
            showTool(ToolHome);
        else
            selectGame(m_gameIndex);
    });
    connect(m_serverPage, &ServerPage::launchServerRequested, this, &MainWindow::onLaunchServer);
    connect(m_serverPage, &ServerPage::stopServerRequested, this, &MainWindow::onStopServer);
    connect(&m_processPollTimer, &QTimer::timeout, this, &MainWindow::onPollRunningProcess);
    m_processPollTimer.setInterval(2000);

    applyHomeVisibility();
    if (m_settings.homeEnabled)
        showTool(ToolHome);
    else
        selectGame(m_gameIndex);
    connect(&I18nHub::instance(), &I18nHub::languageChanged, this, &MainWindow::retranslate);

    // Settle the last self-update before asking about the next one: the build
    // that is running now is the only evidence of whether the swap worked.
    const SelfUpdate::Outcome swap = SelfUpdate::reconcile();
    if (swap.failed && swap.attempts >= 2) {
        QTimer::singleShot(400, this, [this, swap]() { reportFailedSelfUpdate(swap); });
    } else if (m_settings.checkUpdatesOnStart) {
        QTimer::singleShot(900, this, [this]() {
            if (UpdateService::prompt(this, m_settings, true))
                SelfUpdate::quitForSwap();
        });
    }
}

// Two failed swaps in a row is not a hiccup: something is holding the program
// file open, and silently trying a third time is how the app used to loop. Say
// what happened and let the user pick what to do about it.
void MainWindow::reportFailedSelfUpdate(const SelfUpdate::Outcome &swap)
{
    QMessageBox box(this);
    box.setIcon(QMessageBox::Warning);
    box.setWindowTitle(tr("Update not installed"));
    box.setText(tr("Version %1 was downloaded but could not be installed. "
                   "This is still version %2.")
                    .arg(swap.version, QStringLiteral(CLL_VERSION)));
    QString why = tr("Windows will not overwrite a program while a copy of it is "
                     "running. Close every other window of this app and try again.");
    if (!swap.reason.isEmpty())
        why += QStringLiteral("\n\n") + tr("The updater said: %1").arg(swap.reason);
    box.setInformativeText(why);
    QPushButton *again = box.addButton(tr("Try again"), QMessageBox::AcceptRole);
    QPushButton *manual = box.addButton(tr("Open the download page"), QMessageBox::ActionRole);
    box.addButton(tr("Skip this version"), QMessageBox::RejectRole);
    box.setDefaultButton(again);
    box.exec();

    if (box.clickedButton() == again) {
        SelfUpdate::forget();
        if (UpdateService::prompt(this, m_settings, false))
            SelfUpdate::quitForSwap();
    } else if (box.clickedButton() == manual) {
        QDesktopServices::openUrl(QUrl(QStringLiteral(QOL_REPO_URL "/releases/latest")));
    }
}

void MainWindow::retranslate()
{
    setWindowTitle(tr(QOL_APP_NAME));
    if (m_sidebarBy)
        m_sidebarBy->setText(QStringLiteral(QOL_BRAND_SUBTITLE));
    if (m_sidebarGames)
        m_sidebarGames->setText(tr("JOGOS"));
    if (m_sidebarTools)
        m_sidebarTools->setText(tr("FERRAMENTAS"));
    if (m_saveBtn)
        m_saveBtn->setText(tr("Salvar"));
    const QStringList tools = {
        tr("Quality of Life"), tr("Mods"), tr("Servidor LAN (beta)"), tr("Configuracoes"), tr("Sobre")
    };
    for (int i = 0; i < m_toolButtons.size() && i < tools.size(); ++i)
        m_toolButtons[i]->setText("  " + tools[i]);
    if (m_qolPage) m_qolPage->retranslate();
    applyHeader(m_toolIndex);
    if (m_homeNavBtn) m_homeNavBtn->setText("  " + tr("Início"));
    if (m_editGamesBtn) m_editGamesBtn->setToolTip(tr("Reorganizar jogos"));
    if (m_playPage) m_playPage->retranslate();
    if (m_homePage) m_homePage->retranslate();
    if (m_modsPage) m_modsPage->retranslate();
    if (m_serverPage) m_serverPage->retranslate();
    if (m_settingsPage) m_settingsPage->retranslate();
    if (m_aboutPage) m_aboutPage->retranslate();
}

QWidget *MainWindow::buildSidebar()
{
    auto *sidebar = new QWidget(this);
    sidebar->setObjectName("Sidebar");
    sidebar->setFixedWidth(252);
    auto *layout = new QVBoxLayout(sidebar);
    layout->setContentsMargins(12, 16, 12, 12);
    layout->setSpacing(3);

    auto *brand = new QHBoxLayout();
    brand->setSpacing(10);
    auto *logo = new AnimatedLogo(sidebar);
    logo->setLogoSize(38);
    auto *titles = new QVBoxLayout();
    titles->setSpacing(0);
    auto *name = new QLabel(QStringLiteral(QOL_BRAND_TITLE), sidebar);
    name->setObjectName("SidebarTitle");
    m_sidebarBy = new QLabel(QStringLiteral(QOL_BRAND_SUBTITLE), sidebar);
    m_sidebarBy->setObjectName("SidebarSubtitle");
    titles->addWidget(name);
    titles->addWidget(m_sidebarBy);
    brand->addWidget(logo);
    brand->addLayout(titles, 1);
    layout->addLayout(brand);
    layout->addSpacing(10);

    // The nav column scrolls. Every row in it has a fixed height, so a window
    // short enough to squeeze the column had nowhere to give and drew the last
    // game row with its bottom edge sliced off.
    auto *navScroll = new QScrollArea(sidebar);
    navScroll->setObjectName("SidebarScroll");
    navScroll->setWidgetResizable(true);
    navScroll->setFrameShape(QFrame::NoFrame);
    navScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    navScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    navScroll->viewport()->setAutoFillBackground(false);
    auto *nav = new QWidget(navScroll);
    nav->setObjectName("SidebarNav");
    auto *navLay = new QVBoxLayout(nav);
    navLay->setContentsMargins(0, 0, 0, 0);
    navLay->setSpacing(3);
    navScroll->setWidget(nav);
    layout->addWidget(navScroll, 1);

    m_homeNavBtn = new QPushButton(Theme::icon(":/icons/main.svg"), "  " + tr("Início"), nav);
    m_homeNavBtn->setProperty("iconPath", ":/icons/main.svg");
    m_homeNavBtn->setObjectName("SidebarButton");
    m_homeNavBtn->setCheckable(true);
    m_homeNavBtn->setIconSize(QSize(32, 32));
    m_sidebar = sidebar;
    m_homeNavBtn->setCursor(Qt::PointingHandCursor);
    navLay->addWidget(m_homeNavBtn);
    connect(m_homeNavBtn, &QPushButton::clicked, this, [this]() { showTool(ToolHome); });
    navLay->addSpacing(6);

    auto *gamesHead = new QWidget(nav);
    gamesHead->setObjectName("GamesNavHead");
    gamesHead->setAttribute(Qt::WA_StyledBackground, false);
    auto *gamesHeadLay = new QHBoxLayout(gamesHead);
    gamesHeadLay->setContentsMargins(2, 2, 0, 2);
    gamesHeadLay->setSpacing(6);
    m_sidebarGames = new QLabel(tr("JOGOS"), gamesHead);
    m_sidebarGames->setObjectName("NavSection");
    m_editGamesBtn = new QPushButton(Theme::icon(":/icons/pen.svg"), QString(), gamesHead);
    m_editGamesBtn->setProperty("iconPath", ":/icons/pen.svg");
    m_editGamesBtn->setObjectName("GameEditButton");
    m_editGamesBtn->setFixedSize(22, 22);
    m_editGamesBtn->setIconSize(QSize(12, 12));
    m_editGamesBtn->setCursor(Qt::PointingHandCursor);
    m_editGamesBtn->setFlat(true);
    m_editGamesBtn->setToolTip(tr("Reorganizar jogos"));
    m_editGamesBtn->setCheckable(true);
    gamesHeadLay->addWidget(m_sidebarGames, 1);
    gamesHeadLay->addWidget(m_editGamesBtn, 0, Qt::AlignRight | Qt::AlignVCenter);
    navLay->addWidget(gamesHead);
    connect(m_editGamesBtn, &QPushButton::clicked, this, [this](bool on) { setOrganizeGames(on); });

    m_gamesBox = new QWidget(nav);
    m_gamesBox->setObjectName("GamesNavBox");
    m_gamesBox->setAttribute(Qt::WA_StyledBackground, false);
    m_gamesLay = new QVBoxLayout(m_gamesBox);
    m_gamesLay->setContentsMargins(0, 0, 0, 0);
    m_gamesLay->setSpacing(3);

    const auto games = GameCatalog::ordered(m_settings.gameOrder);
    for (int i = 0; i < games.size(); ++i) {
        const auto &g = games[i];
        auto *btn = new QPushButton(m_gamesBox);
        btn->setObjectName("GameNav");
        btn->setCheckable(true);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setToolTip(g.title);
        btn->setProperty("gameCode", g.code);
        btn->setProperty("gameId", g.id);
        auto *hl = new QHBoxLayout(btn);
        hl->setContentsMargins(8, 5, 4, 5);
        hl->setSpacing(8);
        auto *ic = new QLabel(btn);
        ic->setFixedSize(32, 32);
        ic->setPixmap(GameCatalog::icon(g.code, QSize(32, 32)));
        ic->setScaledContents(true);
        ic->setAttribute(Qt::WA_TransparentForMouseEvents);
        auto *txtCol = new QVBoxLayout();
        txtCol->setSpacing(0);
        auto *tag = new QLabel(g.shortLabel.toUpper(), btn);
        tag->setObjectName("GameNavTag");
        tag->setAttribute(Qt::WA_TransparentForMouseEvents);
        auto *nameLbl = new QLabel(g.title, btn);
        nameLbl->setObjectName("GameNavName");
        nameLbl->setAttribute(Qt::WA_TransparentForMouseEvents);
        txtCol->addWidget(tag);
        txtCol->addWidget(nameLbl);
        auto *grip = new GameGrip(btn);
        grip->hide();
        grip->installEventFilter(this);
        hl->addWidget(ic);
        hl->addLayout(txtCol, 1);
        hl->addWidget(grip, 0, Qt::AlignVCenter);
        btn->setFixedHeight(46);
        m_gamesLay->addWidget(btn);
        m_gameButtons << btn;
        connect(btn, &QPushButton::clicked, this, [this, btn]() {
            if (m_organizeGames)
                return;
            const int idx = m_gameButtons.indexOf(btn);
            if (idx >= 0)
                selectGame(idx);
        });
    }
    navLay->addWidget(m_gamesBox);
    m_gameIndex = 0;
    for (int i = 0; i < m_gameButtons.size(); ++i) {
        if (m_gameButtons[i]->property("gameCode").toString() == QLatin1String("t6")) {
            m_gameIndex = i;
            break;
        }
    }

    navLay->addSpacing(4);
    m_sidebarTools = new QLabel(tr("FERRAMENTAS"), nav);
    m_sidebarTools->setObjectName("NavSection");
    navLay->addWidget(m_sidebarTools);

    struct Tool { QString text; QString icon; };
    const QList<Tool> tools = {
        {tr("Quality of Life"), ":/icons/qol.svg"},
        {tr("Mods"), ":/icons/mods.svg"},
        {tr("Servidor LAN (beta)"), ":/icons/server.svg"},
        {tr("Configuracoes"), ":/icons/misc.svg"},
        {tr("Sobre"), ":/icons/icon.ico"},
    };
    for (int i = 0; i < tools.size(); ++i) {
        const bool rawIcon = (tools[i].icon == QLatin1String(":/icons/icon.ico"));
        auto *btn = new QPushButton(rawIcon ? QIcon(tools[i].icon) : Theme::icon(tools[i].icon),
                                    "  " + tools[i].text, nav);
        btn->setProperty("iconPath", tools[i].icon);
        btn->setProperty("rawIcon", rawIcon);
        btn->setObjectName("SidebarButton");
        btn->setCheckable(true);
        btn->setIconSize(QSize(16, 16));
        btn->setCursor(Qt::PointingHandCursor);
        navLay->addWidget(btn);
        m_toolButtons << btn;
        connect(btn, &QPushButton::clicked, this, [this, i]() { showTool(i + kFirstTool); });
    }

    navLay->addStretch();

    auto *rule = new QFrame(sidebar);
    rule->setObjectName("NavRule");
    rule->setFrameShape(QFrame::NoFrame);
    rule->setFixedHeight(1);
    layout->addWidget(rule);
    layout->addSpacing(6);

    auto *foot = new QWidget(sidebar);
    foot->setObjectName("SidebarFooter");
    auto *fl = new QHBoxLayout(foot);
    fl->setContentsMargins(2, 0, 2, 0);
    fl->setSpacing(6);
    auto *ver = new QLabel(QStringLiteral("v") + QStringLiteral(CLL_VERSION), foot);
    ver->setObjectName("VersionBadge");
    auto *repo = new QPushButton(QStringLiteral("GitHub"), foot);
    repo->setObjectName("RepoLink");
    repo->setCursor(Qt::PointingHandCursor);
    repo->setToolTip(QStringLiteral(QOL_REPO_URL));
    QObject::connect(repo, &QPushButton::clicked, foot, []() {
        QDesktopServices::openUrl(QUrl(QStringLiteral(QOL_REPO_URL)));
    });
    fl->addWidget(ver);
    fl->addWidget(repo);
    fl->addStretch();
    layout->addWidget(foot);

    return sidebar;
}

QWidget *MainWindow::buildHeader()
{
    m_header = new QWidget(this);
    m_header->setObjectName("HeaderBar");
    auto *hl = new QHBoxLayout(m_header);
    hl->setContentsMargins(28, 16, 20, 16);
    hl->setSpacing(12);

    auto *col = new QVBoxLayout();
    col->setSpacing(2);
    m_headerTitle = new QLabel(m_header);
    m_headerTitle->setObjectName("PageTitle");
    m_headerSub = new QLabel(m_header);
    m_headerSub->setObjectName("PageSubtitle");
    col->addWidget(m_headerTitle);
    col->addWidget(m_headerSub);
    hl->addLayout(col, 1);

    m_saveBtn = new QPushButton(tr("Salvar"), m_header);
    m_saveBtn->setProperty("cssClass", "ghost");
    m_saveBtn->setCursor(Qt::PointingHandCursor);
    hl->addWidget(m_saveBtn);

    connect(m_saveBtn, &QPushButton::clicked, this, &MainWindow::onSaveSettings);
    return m_header;
}

void MainWindow::applyHeader(int toolIndex)
{
    if (!m_header)
        return;
    if (toolIndex == ToolPlay) {
        m_header->hide();
        return;
    }
    struct Info { QString title; QString sub; bool save; };
    Info info;
    switch (toolIndex) {
    case ToolHome:
        info = {tr("Início"), tr("Descubra mods e mapas por jogo."), false};
        break;
    case ToolQol:
        info = {tr("Quality of Life"), tr("The series: install, update and remove each game's mod and its extras."), false};
        break;
    case ToolMods:
        info = {tr("Mods"), tr("Instale pacotes e mapas customizados por jogo."), false};
        break;
    case ToolServer:
        info = {tr("Servidor LAN (beta)"), tr("Hospede uma partida na sua rede local."), false};
        break;
    case ToolSettings:
        info = {tr("Configuracoes"), tr("Apelido, pastas do Plutonium e dos jogos."), false};
        break;
    default:
        info = {tr("Sobre"), tr("Versao, creditos e licencas."), false};
        break;
    }
    m_headerTitle->setText(info.title);
    m_headerSub->setText(info.sub);
    m_saveBtn->setVisible(info.save);
    m_header->show();
}

void MainWindow::selectGame(int gameIndex)
{
    m_gameIndex = gameIndex;
    m_toolIndex = ToolPlay;
    const QString id = (gameIndex >= 0 && gameIndex < m_gameButtons.size())
                           ? m_gameButtons[gameIndex]->property("gameId").toString()
                           : GameCatalog::all().at(0).id;
    m_modsPage->selectGame(id);
    m_playPage->setSelectedMod(m_modsPage->selectedMod());
    m_playPage->setGame(id);
    m_playPage->setRunning(m_runningPid > 0 && m_runningGameId == id);
    m_stack->setCurrentWidget(m_playPage);
    applyHeader(ToolPlay);
    syncNav();
}

void MainWindow::applyHomeVisibility()
{
    const bool on = m_settings.homeEnabled;
    if (m_homeNavBtn)
        m_homeNavBtn->setVisible(on);
    if (m_homePage)
        m_homePage->setEnabled(on);
    if (!on && m_toolIndex == ToolHome)
        selectGame(m_gameIndex);
}

void MainWindow::showTool(int toolIndex)
{
    if (toolIndex == ToolHome && !m_settings.homeEnabled) {
        selectGame(m_gameIndex);
        return;
    }
    m_toolIndex = toolIndex;
    const QString gameId = (m_gameIndex >= 0 && m_gameIndex < m_gameButtons.size())
                               ? m_gameButtons[m_gameIndex]->property("gameId").toString()
                               : QString();
    if (toolIndex == ToolQol)
        m_qolPage->refresh();
    else if (toolIndex == ToolMods)
        m_modsPage->selectGame(gameId);
    else if (toolIndex == ToolServer)
        m_serverPage->selectGame(gameId);
    else if (toolIndex == ToolSettings)
        m_settingsPage->beginEdit();
    m_stack->setCurrentIndex(toolIndex);
    applyHeader(toolIndex);
    syncNav();
}

void MainWindow::syncNav()
{
    if (m_homeNavBtn)
        m_homeNavBtn->setChecked(m_toolIndex == ToolHome);
    for (int i = 0; i < m_gameButtons.size(); ++i)
        m_gameButtons[i]->setChecked(m_toolIndex == ToolPlay && i == m_gameIndex);
    for (int i = 0; i < m_toolButtons.size(); ++i)
        m_toolButtons[i]->setChecked(m_toolIndex == i + kFirstTool);
}

// The ReShade tick box is a statement about the next session, not a switch on
// a watchdog: ticked means ReShade is there and is the right build for where
// you are playing, unticked means it is gone. Both are made true here, before
// the game starts, because bin cannot be written once it is running.
void MainWindow::prepareReShade(QolService::ReShadeMode mode, bool wantDlss)
{
    QString err;
    if (!m_settings.launchReShade) {
        if (!QolService::ensureReShadeAbsent(m_settings, err) && !err.isEmpty())
            Dialogs::error(this, err);
        return;
    }
    if (!QolService::applyReShadeMode(m_settings, mode, wantDlss, err) && !err.isEmpty())
        Dialogs::error(this, err);
}

void MainWindow::startReShadeWatchdogIfWanted()
{
    if (!m_settings.launchReShade)
        return;
    QString err;
    if (!GameLauncher::startReShadeWatchdog(m_settings.plutoniumInstance, err))
        Dialogs::error(this, err);
}

void MainWindow::onLaunchGame(const QString &gameId, const QString &mode)
{
    if (m_runningPid > 0)
        return;
    if (gameId == "World at War") m_settings.modeId = (mode == QLatin1String("mp")) ? "t4mp" : "t4sp";
    else if (gameId == "Black ops") m_settings.modeId = (mode == QLatin1String("mp")) ? "t5mp" : "t5sp";
    else if (gameId == "Black ops II") m_settings.modeId = (mode == QLatin1String("mp")) ? "t6mp" : "t6zm";
    else if (gameId == "Modern Warfare 3") m_settings.modeId = "iw5mp";
    else if (gameId == "Advanced Warfare") {
        if (mode == QLatin1String("sp")) m_settings.modeId = "s1sp";
        else if (mode == QLatin1String("sv")) m_settings.modeId = "s1sv";
        else if (mode == QLatin1String("zm")) m_settings.modeId = "s1zm";
        else m_settings.modeId = "s1mp";
    } else if (gameId == "Black ops III") {
        if (mode == QLatin1String("sp")) m_settings.modeId = "t7sp";
        else if (mode == QLatin1String("zm")) m_settings.modeId = "t7zm";
        else m_settings.modeId = "t7mp";
    }
    m_settings.gameId = gameId;
    m_settings.saveToIni();

    // LAN is not signed in to anything, so it gets the add-on build of ReShade
    // and, on Black Ops II, DLSS 5 with it. This has to happen before the game
    // starts: once the bootstrapper has dxgi.dll open it cannot be replaced.
    prepareReShade(QolService::ReShadeMode::Lan, gameId == QLatin1String("Black ops II"));

    const GameLauncher::Result result = GameLauncher::launch(m_settings, m_modsPage->selectedMod());
    if (result.hasError) {
        if (!result.errorText.isEmpty())
            Dialogs::error(this, result.errorText);
        else
            Dialogs::info(this, result.errorMsg, m_settings);
        return;
    }
    startReShadeWatchdogIfWanted();
    m_runningPid = result.pid;
    m_runningGameId = gameId;
    m_playPage->setRunning(true);
    m_processPollTimer.start();
}

void MainWindow::onLaunchOnline(const QString &gameId, const QString &mode)
{
    if (m_runningPid > 0)
        return;
    if (gameId == "World at War") m_settings.modeId = (mode == QLatin1String("mp")) ? "t4mp" : "t4sp";
    else if (gameId == "Black ops") m_settings.modeId = (mode == QLatin1String("mp")) ? "t5mp" : "t5sp";
    else if (gameId == "Black ops II") m_settings.modeId = (mode == QLatin1String("mp")) ? "t6mp" : "t6zm";
    else if (gameId == "Modern Warfare 3") m_settings.modeId = "iw5mp";
    else m_settings.modeId.clear();
    m_settings.gameId = gameId;
    m_settings.saveToIni();

    // Online is signed in to Plutonium, so it gets the stock build and none of
    // the add-ons: the feeder, its 64-bit host and the DLSS entries in the
    // preset all come back out of bin before the session starts.
    prepareReShade(QolService::ReShadeMode::Online, false);

    GameLauncher::Result result = GameLauncher::launchOnline(m_settings);
    if (result.needsLogin) {
        // No account signed in yet, or the saved one expired. Ask once, then
        // launch straight away so one click on Play online still lands in game.
        LoginDialog dlg(PlutoniumAuth::tokenRoot(m_settings.plutoniumInstance), this);
        if (dlg.exec() != QDialog::Accepted)
            return;
        if (m_settings.username.isEmpty() && !dlg.username().isEmpty()) {
            m_settings.username = dlg.username();
            m_settings.saveToIni();
        }
        result = GameLauncher::launchOnline(m_settings);
    }
    if (result.hasError) {
        if (!result.errorText.isEmpty())
            Dialogs::error(this, result.errorText);
        else
            Dialogs::info(this, result.errorMsg, m_settings);
        return;
    }
    startReShadeWatchdogIfWanted();
    // We started the bootstrapper ourselves, so the pid is ours to track: Stop
    // and the running state work exactly as they do for a LAN launch.
    m_runningPid = result.pid;
    m_runningGameId = gameId;
    m_playPage->setRunning(true);
    m_processPollTimer.start();
}

void MainWindow::onStopGame(const QString &)
{
    if (m_runningPid > 0) {
        // terminatePid waits for a clean quit before forcing; keep the UI alive.
        const qint64 pid = m_runningPid;
        auto f = QtConcurrent::run([pid]() { GameLauncher::terminatePid(pid); });
        Q_UNUSED(f);
    }
    if (m_runningGameId == QLatin1String("Black ops III"))
        GameLauncher::terminateFolderProcesses(m_settings.bo3);
    m_runningPid = 0;
    m_runningGameId.clear();
    m_playPage->setRunning(false);
    m_processPollTimer.stop();
    // The watchdog exists to serve a running game. Stopping the game closes
    // its window too - nobody has to go and find it afterwards.
    GameLauncher::stopReShadeWatchdog();
}

void MainWindow::onPollRunningProcess()
{
    if (m_runningGameId == QLatin1String("Black ops III")) {
        const qint64 live = GameLauncher::findProcessInFolder(m_settings.bo3);
        if (live > 0) {
            m_runningPid = live;
        } else if (m_runningPid > 0) {
            m_runningPid = 0;
            m_runningGameId.clear();
            m_playPage->setRunning(false);
            GameLauncher::stopReShadeWatchdog();
        }
    } else if (m_runningPid > 0 && !GameLauncher::isPidRunning(m_runningPid)) {
        m_runningPid = 0;
        m_runningGameId.clear();
        m_playPage->setRunning(false);
        // The game closed on its own: close the watchdog window with it.
        GameLauncher::stopReShadeWatchdog();
    }
    if (m_serverPid > 0 && !GameLauncher::isPidRunning(m_serverPid)) {
        m_serverPid = 0;
        m_serverPage->setRunning(false);
    }
    if (m_runningPid <= 0 && m_serverPid <= 0)
        m_processPollTimer.stop();
}

void MainWindow::onLaunchServer(const QString &configSelection, const QString &port)
{
    if (m_serverPid > 0)
        return;
    m_settings.saveToIni();
    const GameLauncher::Result result = GameLauncher::launchServer(
        m_settings, m_modsPage->selectedMod(), configSelection, port);
    if (result.needsBo2GameSettings) {
        if (Dialogs::confirmDownloadGameSettings(this))
            m_serverPage->downloadBo2GameSettings();
        return;
    }
    if (result.hasError) {
        Dialogs::info(this, result.errorMsg, m_settings);
        return;
    }
    m_serverPid = result.pid;
    m_serverPage->setRunning(true);
    m_processPollTimer.start();
}

void MainWindow::onStopServer()
{
    if (m_serverPid > 0)
        GameLauncher::terminatePid(m_serverPid);
    m_serverPid = 0;
    m_serverPage->setRunning(false);
    if (m_runningPid <= 0)
        m_processPollTimer.stop();
}

void MainWindow::onSaveSettings() { m_settings.saveToIni(); }

void MainWindow::persistGameOrder()
{
    QStringList codes;
    for (auto *b : m_gameButtons)
        codes << b->property("gameCode").toString();
    m_settings.gameOrder = codes;
    m_settings.saveToIni();
}

void MainWindow::updateOrganizeOverlay()
{
    if (!m_organizeOverlay || !m_gamesBox)
        return;
    m_organizeOverlay->setGeometry(rect());
    auto *dim = static_cast<DimOverlay *>(m_organizeOverlay);
    QWidget *head = m_editGamesBtn ? m_editGamesBtn->parentWidget() : nullptr;
    QRect hole = m_gamesBox->rect();
    hole.moveTopLeft(m_gamesBox->mapTo(this, QPoint(0, 0)));
    if (head) {
        QRect headR = head->rect();
        headR.moveTopLeft(head->mapTo(this, QPoint(0, 0)));
        hole = hole.united(headR);
    }
    dim->hole = hole.adjusted(-4, -2, 4, 4);
    QRegion mask(dim->rect());
    mask -= QRegion(dim->hole.adjusted(-8, -8, 8, 8));
    dim->setMask(mask);
    dim->update();
    if (m_organizeHint && m_organizeHint->isVisible()) {
        const QPoint pos(hole.right() + 22, hole.top() + 8);
        m_organizeHint->move(pos);
        m_organizeHint->raise();
    }
}

void MainWindow::setOrganizeGames(bool on)
{
    m_organizeGames = on;
    if (m_editGamesBtn)
        m_editGamesBtn->setChecked(on);
    for (auto *b : m_gameButtons) {
        if (auto *grip = b->findChild<QWidget *>(QStringLiteral("GameNavGrip")))
            grip->setVisible(on);
    }
    if (!m_organizeOverlay)
        return;
    if (!on) {
        if (m_organizeHint)
            m_organizeHint->hide();
        m_organizeOverlay->hide();
        persistGameOrder();
        return;
    }
    m_organizeOverlay->show();
    m_organizeOverlay->raise();
    updateOrganizeOverlay();

    if (!m_settings.gameOrderHintSeen) {
        if (!m_organizeHint) {
            m_organizeHint = new QFrame(this);
            m_organizeHint->setObjectName("OrganizeHint");
            auto *lay = new QVBoxLayout(m_organizeHint);
            lay->setContentsMargins(16, 14, 16, 14);
            lay->setSpacing(6);
            auto *title = new QLabel(tr("Reorganize seus jogos"), m_organizeHint);
            title->setObjectName("OrganizeHintTitle");
            auto *body = new QLabel(tr("Arraste pelos quadradinhos à direita de cada aba para mudar a ordem. A lista fica salva neste PC."), m_organizeHint);
            body->setObjectName("OrganizeHintBody");
            body->setWordWrap(true);
            body->setFixedWidth(230);
            lay->addWidget(title);
            lay->addWidget(body);
            auto *ok = new QPushButton(tr("OK"), m_organizeHint);
            ok->setProperty("cssClass", "primary");
            ok->setCursor(Qt::PointingHandCursor);
            ok->setMinimumHeight(32);
            lay->addWidget(ok, 0, Qt::AlignRight);
            connect(ok, &QPushButton::clicked, this, [this]() {
                m_settings.gameOrderHintSeen = true;
                m_settings.saveToIni();
                if (m_organizeHint)
                    m_organizeHint->hide();
            });
            m_organizeHint->setStyleSheet(
                QStringLiteral("QFrame#OrganizeHint { background:%1; border:1px solid %2; border-radius:%3px; }"
                               "QLabel#OrganizeHintTitle { color:%4; font-weight:600; font-size:13px; }"
                               "QLabel#OrganizeHintBody { color:%5; font-size:12px; }")
                    .arg(Theme::token("SURFACE"), Theme::token("LINE"),
                         Theme::radius() ? QStringLiteral("10") : QStringLiteral("0"),
                         Theme::token("TEXT_STRONG"), Theme::token("TEXT_DIM")));
        }
        m_organizeHint->adjustSize();
        m_organizeHint->show();
        m_organizeHint->raise();
        updateOrganizeOverlay();
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    // The watchdog window is ours; do not leave it behind when the app closes.
    GameLauncher::stopReShadeWatchdog();
    QMainWindow::closeEvent(event);
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    if (m_organizeGames)
        updateOrganizeOverlay();
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == m_organizeOverlay && event->type() == QEvent::MouseButtonPress) {
        setOrganizeGames(false);
        return true;
    }
    if (obj->objectName() == QLatin1String("GameNavGrip") && m_organizeGames) {
        auto *btn = qobject_cast<QPushButton *>(obj->parent());
        if (!btn)
            return QMainWindow::eventFilter(obj, event);
        if (event->type() == QEvent::MouseButtonPress) {
            m_dragBtn = btn;
            m_dragOffset = static_cast<QMouseEvent *>(event)->globalPosition().toPoint().y();
            btn->raise();
            return true;
        }
        if (event->type() == QEvent::MouseMove && m_dragBtn) {
            const int y = static_cast<QMouseEvent *>(event)->globalPosition().toPoint().y();
            QPushButton *over = nullptr;
            int overIdx = -1;
            for (int i = 0; i < m_gameButtons.size(); ++i) {
                const QRect r(m_gameButtons[i]->mapToGlobal(QPoint(0, 0)), m_gameButtons[i]->size());
                if (r.contains(QPoint(r.center().x(), y))) {
                    over = m_gameButtons[i];
                    overIdx = i;
                    break;
                }
            }
            const int from = m_gameButtons.indexOf(m_dragBtn);
            if (over && overIdx >= 0 && from >= 0 && overIdx != from) {
                m_gameButtons.move(from, overIdx);
                m_gamesLay->removeWidget(m_dragBtn);
                m_gamesLay->insertWidget(overIdx, m_dragBtn);
                if (m_gameIndex == from)
                    m_gameIndex = overIdx;
                else if (from < m_gameIndex && overIdx >= m_gameIndex)
                    --m_gameIndex;
                else if (from > m_gameIndex && overIdx <= m_gameIndex)
                    ++m_gameIndex;
                syncNav();
            }
            Q_UNUSED(y);
            return true;
        }
        if (event->type() == QEvent::MouseButtonRelease) {
            m_dragBtn = nullptr;
            persistGameOrder();
            return true;
        }
    }
    return QMainWindow::eventFilter(obj, event);
}

