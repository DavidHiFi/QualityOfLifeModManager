#pragma once
#include <QDialog>
#include <QString>

class QLabel;
class QLineEdit;
class QPushButton;

// Signs in to a Plutonium account so online play can mint its own session
// tokens. Writes the user token where the official launcher keeps it, so the
// two stay in step: sign in here and the launcher is signed in too.
class LoginDialog : public QDialog
{
    Q_OBJECT
public:
    explicit LoginDialog(const QString &plutoniumRoot, QWidget *parent = nullptr);

    // Account name that was signed in, once exec() returned Accepted.
    QString username() const { return m_signedInAs; }

private slots:
    void onSignIn();

private:
    void setBusy(bool busy);

    QString m_plutoniumRoot;
    QString m_signedInAs;
    QLineEdit *m_user = nullptr;
    QLineEdit *m_pass = nullptr;
    QLabel *m_status = nullptr;
    QPushButton *m_signIn = nullptr;
    QPushButton *m_cancel = nullptr;
};
