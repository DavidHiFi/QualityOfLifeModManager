#pragma once
#include <QString>

// Plutonium's own account service (nix.plutonium.pw) - the one the official
// launcher talks to. Speaking it ourselves is what lets "Play online" start the
// game directly, with no launcher window and no "Could not authenticate".
//
// Two different tokens are involved and they are NOT interchangeable:
//
//   user token     long-lived, one per account. Kept DPAPI-encrypted (current
//                  user) in <plutonium>\config.json under "token", which is the
//                  same file and the same format the official launcher uses, so
//                  signing in here signs you in there and vice versa.
//
//   session token  16 hex characters, minted per launch from the user token and
//                  handed to the bootstrapper as `-token`. The bootstrapper has
//                  no login of its own: started without a fresh session token it
//                  prints "Could not authenticate to Plutonium" and quits.
namespace PlutoniumAuth
{
    struct Account
    {
        bool ok = false;
        int userId = 0;
        QString username;
        QString email;
        bool emailVerified = true;
    };

    // --- saved user token, shared with the official launcher -----------------
    // The root whose config.json we should read and write: the configured
    // install when it already holds a login, otherwise %LOCALAPPDATA%\Plutonium,
    // which is where the official launcher keeps one. Without this a portable
    // kit would ask an already signed-in user to sign in again.
    QString tokenRoot(const QString &preferredRoot);
    QString configPath(const QString &plutoniumRoot);
    bool hasSavedToken(const QString &plutoniumRoot);
    QString savedToken(const QString &plutoniumRoot, QString *error = nullptr);
    bool saveToken(const QString &plutoniumRoot, const QString &token, QString *error = nullptr);
    bool clearToken(const QString &plutoniumRoot, QString *error = nullptr);

    // --- network -------------------------------------------------------------
    // Each call blocks on its own event loop. The endpoints answer in well under
    // a second; callers on the GUI thread show a busy cursor rather than a thread.
    Account validate(const QString &userToken, QString *error = nullptr);
    // Returns the user token on success and fills account when non-null.
    QString login(const QString &username, const QString &password,
                  Account *account = nullptr, QString *error = nullptr);
    // game is the bootstrapper mode id: t6zm, t6mp, t5sp, iw5mp...
    QString createSession(const QString &userToken, const QString &game, QString *error = nullptr);

    // Human-readable "signed in as X" / "not signed in", for Settings.
    QString accountSummary(const QString &plutoniumRoot);
}
