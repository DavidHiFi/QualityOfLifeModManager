#include "AboutPage.h"
#include "Version.h"

#include <QDesktopServices>
#include <QIcon>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSizePolicy>
#include <QUrl>
#include <QVBoxLayout>

AboutPage::AboutPage(QWidget *parent)
    : QWidget(parent)
{
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(28, 24, 28, 24);
    outer->setAlignment(Qt::AlignTop | Qt::AlignHCenter);

    auto *col = new QWidget(this);
    col->setObjectName("AboutColumn");
    col->setMaximumWidth(620);
    col->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);

    auto *root = new QVBoxLayout(col);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(14);
    root->setAlignment(Qt::AlignTop | Qt::AlignHCenter);

    auto *logo = new QLabel(col);
    QIcon appIcon(QStringLiteral(":/icons/icon.ico"));
    if (appIcon.isNull())
        appIcon = QIcon(QStringLiteral(":/icons/app.svg"));
    logo->setPixmap(appIcon.pixmap(QSize(96, 96)));
    logo->setFixedSize(96, 96);
    logo->setAlignment(Qt::AlignCenter);
    root->addWidget(logo, 0, Qt::AlignHCenter);

    auto *title = new QLabel(tr("Cod Lan Launcher"), col);
    title->setObjectName("HeroTitle");
    title->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    title->setWordWrap(true);
    root->addWidget(title, 0, Qt::AlignHCenter);

    auto *titleRepo = new QPushButton(QStringLiteral("GitHub"), col);
    titleRepo->setCursor(Qt::PointingHandCursor);
    titleRepo->setToolTip(QStringLiteral("https://github.com/MestreTM/CLL-CodLanLauncher"));
    connect(titleRepo, &QPushButton::clicked, this, []() {
        QDesktopServices::openUrl(QUrl(QStringLiteral("https://github.com/MestreTM/CLL-CodLanLauncher")));
    });
    root->addWidget(titleRepo, 0, Qt::AlignHCenter);

    m_by = new QLabel(tr("por MestreTM"), col);
    m_by->setObjectName("AboutCredit");
    m_by->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    m_by->setWordWrap(true);
    root->addWidget(m_by, 0, Qt::AlignHCenter);

    auto *ver = new QLabel(QStringLiteral("v") + QStringLiteral(CLL_VERSION), col);
    ver->setObjectName("HeroSub");
    ver->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    root->addWidget(ver, 0, Qt::AlignHCenter);

    m_body = new QLabel(col);
    m_body->setObjectName("AboutBody");
    m_body->setWordWrap(true);
    m_body->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
    m_body->setTextFormat(Qt::PlainText);
    m_body->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    root->addWidget(m_body, 0, Qt::AlignHCenter);

    auto *pluto = new QPushButton(tr("plutonium.pw"), col);
    pluto->setObjectName("CreditLink");
    pluto->setCursor(Qt::PointingHandCursor);
    connect(pluto, &QPushButton::clicked, this, []() {
        QDesktopServices::openUrl(QUrl(QStringLiteral("https://plutonium.pw/")));
    });
    auto *xerxes = new QPushButton(QStringLiteral("xerxes-at"), col);
    xerxes->setObjectName("CreditLink");
    xerxes->setCursor(Qt::PointingHandCursor);
    connect(xerxes, &QPushButton::clicked, this, []() {
        QDesktopServices::openUrl(QUrl(QStringLiteral("https://github.com/xerxes-at")));
    });
    auto *jug = new QPushButton(QStringLiteral("JugAndDoubleTap"), col);
    jug->setObjectName("CreditLink");
    jug->setCursor(Qt::PointingHandCursor);
    connect(jug, &QPushButton::clicked, this, []() {
        QDesktopServices::openUrl(QUrl(QStringLiteral("https://github.com/JugAndDoubleTap/")));
    });
    auto *row = new QHBoxLayout();
    row->setSpacing(10);
    row->setAlignment(Qt::AlignHCenter);
    row->addStretch();
    row->addWidget(pluto);
    row->addWidget(xerxes);
    row->addWidget(jug);
    row->addStretch();
    root->addLayout(row);

    auto *aw = new QPushButton(QStringLiteral("alterware.dev"), col);
    aw->setObjectName("CreditLink");
    aw->setCursor(Qt::PointingHandCursor);
    connect(aw, &QPushButton::clicked, this, []() {
        QDesktopServices::openUrl(QUrl(QStringLiteral("https://alterware.dev")));
    });
    auto *boiii = new QPushButton(QStringLiteral("boiii-community"), col);
    boiii->setObjectName("CreditLink");
    boiii->setCursor(Qt::PointingHandCursor);
    connect(boiii, &QPushButton::clicked, this, []() {
        QDesktopServices::openUrl(QUrl(QStringLiteral("https://gitlab.com/boiii-community/BOIII-Community")));
    });
    auto *ezz = new QPushButton(QStringLiteral("ezz.lol"), col);
    ezz->setObjectName("CreditLink");
    ezz->setCursor(Qt::PointingHandCursor);
    connect(ezz, &QPushButton::clicked, this, []() {
        QDesktopServices::openUrl(QUrl(QStringLiteral("https://ezz.lol")));
    });
    auto *row2 = new QHBoxLayout();
    row2->setSpacing(10);
    row2->setAlignment(Qt::AlignHCenter);
    row2->addStretch();
    row2->addWidget(aw);
    row2->addWidget(boiii);
    row2->addWidget(ezz);
    row2->addStretch();
    root->addLayout(row2);

    outer->addWidget(col, 0, Qt::AlignHCenter);
    outer->addStretch();

    retranslate();
}

void AboutPage::retranslate()
{
    if (m_by)
        m_by->setText(tr("por MestreTM"));
    if (m_body)
        m_body->setText(tr("Launcher offline para Plutonium (T4 / T5 / T6 / IW5) e clients extras.\n\n"
                           "Este programa e de MestreTM.\n"
                           "A ideia e a logica original foram baseadas no LanLauncher de JugAndDoubleTap.\n"
                           "Arquivos de configuracao de servidor LAN (T4 / T5 / T6) by xerxes-at.\n"
                           "Traducao em turco: TehTurkishSpartan.\n"
                           "Codigo-fonte de alguns clients: alterware.dev.\n"
                           "Client alternativo de BO3: boiii-community.\n"
                           "Source do client nativo de BO3: ezz.lol.\n\n"
                           "Plutonium e uma marca da equipe Plutonium. Call of Duty e da Activision / Treyarch.\n"
                           "Este projeto nao e afiliado a nenhuma delas."));
}
