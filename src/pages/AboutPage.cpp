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

namespace {

QPushButton *link(QWidget *parent, const QString &text, const QString &url, const char *objectName = "CreditLink")
{
    auto *b = new QPushButton(text, parent);
    b->setObjectName(QLatin1String(objectName));
    b->setCursor(Qt::PointingHandCursor);
    b->setToolTip(url);
    QObject::connect(b, &QPushButton::clicked, parent, [url]() { QDesktopServices::openUrl(QUrl(url)); });
    return b;
}

} // namespace

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

    auto *title = new QLabel(QStringLiteral(QOL_APP_NAME), col);
    title->setObjectName("HeroTitle");
    title->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    title->setWordWrap(true);
    root->addWidget(title, 0, Qt::AlignHCenter);

    m_by = new QLabel(QStringLiteral("by " QOL_AUTHOR), col);
    m_by->setObjectName("AboutCredit");
    m_by->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    root->addWidget(m_by, 0, Qt::AlignHCenter);

    auto *ver = new QLabel(QStringLiteral("v") + QStringLiteral(CLL_VERSION), col);
    ver->setObjectName("HeroSub");
    ver->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    root->addWidget(ver, 0, Qt::AlignHCenter);

    auto *repos = new QHBoxLayout();
    repos->setSpacing(10);
    repos->addStretch();
    for (QPushButton *b : {link(col, QStringLiteral("GitHub"), QStringLiteral(QOL_REPO_URL), "AboutLink"),
                           link(col, tr("The series"), QStringLiteral(QOL_SERIES_URL), "AboutLink"),
                           link(col, tr("Licence"), QStringLiteral(QOL_REPO_URL "/blob/main/LICENSE"), "AboutLink")}) {
        b->setMinimumSize(110, 36);
        repos->addWidget(b);
    }
    repos->addStretch();
    root->addLayout(repos);

    m_body = new QLabel(col);
    m_body->setObjectName("AboutBody");
    m_body->setWordWrap(true);
    m_body->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
    m_body->setTextFormat(Qt::PlainText);
    root->addWidget(m_body, 0, Qt::AlignHCenter);

    auto *row = new QHBoxLayout();
    row->setSpacing(10);
    row->addStretch();
    row->addWidget(link(col, QStringLiteral("DavidHiFi"), QStringLiteral(QOL_AUTHOR_URL)));
    row->addWidget(link(col, QStringLiteral("Cod Lan Launcher"), QStringLiteral(CLL_UPSTREAM_URL)));
    row->addWidget(link(col, QStringLiteral("plutonium.pw"), QStringLiteral("https://plutonium.pw/")));
    row->addWidget(link(col, QStringLiteral("ReShade"), QStringLiteral("https://reshade.me/")));
    row->addStretch();
    root->addLayout(row);

    auto *row2 = new QHBoxLayout();
    row2->setSpacing(10);
    row2->addStretch();
    row2->addWidget(link(col, QStringLiteral("JugAndDoubleTap"), QStringLiteral("https://github.com/JugAndDoubleTap/")));
    row2->addWidget(link(col, QStringLiteral("xerxes-at"), QStringLiteral("https://github.com/xerxes-at")));
    row2->addWidget(link(col, QStringLiteral("alterware.dev"), QStringLiteral("https://alterware.dev")));
    row2->addWidget(link(col, QStringLiteral("boiii-community"), QStringLiteral("https://gitlab.com/boiii-community/BOIII-Community")));
    row2->addStretch();
    root->addLayout(row2);

    outer->addWidget(col, 0, Qt::AlignHCenter);
    outer->addStretch();

    retranslate();
}

void AboutPage::retranslate()
{
    if (m_body)
        m_body->setText(tr("Installs, updates, launches and removes mods for Call of Duty on Plutonium, "
                           "with the Quality of Life series front and centre.\n\n"
                           "Written by DavidHiFi. Forked from Cod Lan Launcher by MestreTM (LGPL-3.0), "
                           "which grew out of JugAndDoubleTap's LanLauncher. LAN server configs by xerxes-at. "
                           "Some standalone client sources come from alterware.dev; the alternative Black Ops III "
                           "client is by boiii-community. ReShade is by crosire.\n\n"
                           "Plutonium belongs to the Plutonium team. Call of Duty belongs to Activision and Treyarch. "
                           "This project is not affiliated with any of them."));
}
