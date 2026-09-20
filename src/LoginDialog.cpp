#include "LoginDialog.h"
#include "PlutoniumAuth.h"

#include <QApplication>
#include <QDesktopServices>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>

LoginDialog::LoginDialog(const QString &plutoniumRoot, QWidget *parent)
    : QDialog(parent), m_plutoniumRoot(plutoniumRoot)
{
    setWindowTitle(tr("Sign in to Plutonium"));
    setModal(true);
    resize(430, 0);

    auto *lay = new QVBoxLayout(this);
    lay->setSpacing(10);

    auto *intro = new QLabel(
        tr("Online play needs your Plutonium forum account. This is the same "
           "login the Plutonium launcher uses, and it is stored the same way, "
           "so you only do this once."), this);
    intro->setWordWrap(true);
    lay->addWidget(intro);

    m_user = new QLineEdit(this);
    m_user->setPlaceholderText(tr("Username or email"));
    lay->addWidget(m_user);

    m_pass = new QLineEdit(this);
    m_pass->setPlaceholderText(tr("Password"));
    m_pass->setEchoMode(QLineEdit::Password);
    lay->addWidget(m_pass);

    auto *register_ = new QLabel(
        tr("No account yet? <a href=\"https://forum.plutonium.pw/register\">Create one on the forum</a>."), this);
    register_->setTextFormat(Qt::RichText);
    register_->setOpenExternalLinks(true);
    register_->setWordWrap(true);
    lay->addWidget(register_);

    m_status = new QLabel(this);
    m_status->setWordWrap(true);
    m_status->setVisible(false);
    lay->addWidget(m_status);

    auto *row = new QHBoxLayout();
    m_cancel = new QPushButton(tr("Cancel"), this);
    m_signIn = new QPushButton(tr("Sign in"), this);
    m_signIn->setProperty("cssClass", "primary");
    m_signIn->setDefault(true);
    row->addStretch();
    row->addWidget(m_cancel);
    row->addWidget(m_signIn);
    lay->addLayout(row);

    connect(m_cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_signIn, &QPushButton::clicked, this, &LoginDialog::onSignIn);
    connect(m_pass, &QLineEdit::returnPressed, this, &LoginDialog::onSignIn);
    connect(m_user, &QLineEdit::returnPressed, this, &LoginDialog::onSignIn);
}

void LoginDialog::setBusy(bool busy)
{
    m_user->setEnabled(!busy);
    m_pass->setEnabled(!busy);
    m_signIn->setEnabled(!busy);
    m_cancel->setEnabled(!busy);
    m_signIn->setText(busy ? tr("Signing in...") : tr("Sign in"));
}

void LoginDialog::onSignIn()
{
    const QString user = m_user->text().trimmed();
    const QString pass = m_pass->text();
    if (user.isEmpty() || pass.isEmpty()) {
        m_status->setText(tr("Enter your username and password."));
        m_status->setVisible(true);
        return;
    }

    setBusy(true);
    m_status->setText(tr("Contacting Plutonium..."));
    m_status->setVisible(true);
    QApplication::setOverrideCursor(Qt::WaitCursor);
    QApplication::processEvents();

    QString error;
    PlutoniumAuth::Account account;
    const QString token = PlutoniumAuth::login(user, pass, &account, &error);
    QApplication::restoreOverrideCursor();

    if (token.isEmpty()) {
        setBusy(false);
        m_status->setText(error.isEmpty() ? tr("Sign in failed.") : error);
        m_pass->selectAll();
        m_pass->setFocus();
        return;
    }

    if (!PlutoniumAuth::saveToken(m_plutoniumRoot, token, &error)) {
        setBusy(false);
        m_status->setText(error);
        return;
    }

    m_signedInAs = account.username.isEmpty() ? user : account.username;
    accept();
}
