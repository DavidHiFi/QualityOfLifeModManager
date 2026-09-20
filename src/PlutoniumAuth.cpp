#include "PlutoniumAuth.h"
#include "AppSettings.h"

#include <QByteArray>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QObject>
#include <QTimer>
#include <QUrl>

#ifdef Q_OS_WIN
#  include <windows.h>
#  include <wincrypt.h>
#endif

namespace {

const char *const kApiBase = "https://nix.plutonium.pw";

// The service sits behind a filter that answers only this exact client string:
// "nix/3.0" and "Nix/3.1" both come back 403 with a challenge page, and so does
// our own user agent. This is the identity of the protocol we are speaking, so
// it is a constant, not a preference. Changing it turns every online launch
// back into "Could not authenticate".
const char *const kApiUserAgent = "Nix/3.0";

QString dpapiError()
{
#ifdef Q_OS_WIN
    return QObject::tr("Windows could not unlock the saved Plutonium login (error %1).")
        .arg(GetLastError());
#else
    return QObject::tr("Saved Plutonium logins are a Windows feature.");
#endif
}

#ifdef Q_OS_WIN
QByteArray dpapiUnprotect(const QByteArray &blob, bool *ok)
{
    DATA_BLOB in{static_cast<DWORD>(blob.size()),
                 reinterpret_cast<BYTE *>(const_cast<char *>(blob.constData()))};
    DATA_BLOB out{0, nullptr};
    if (!CryptUnprotectData(&in, nullptr, nullptr, nullptr, nullptr, 0, &out)) {
        *ok = false;
        return {};
    }
    QByteArray plain(reinterpret_cast<const char *>(out.pbData), static_cast<int>(out.cbData));
    LocalFree(out.pbData);
    *ok = true;
    return plain;
}

QByteArray dpapiProtect(const QByteArray &plain, bool *ok)
{
    DATA_BLOB in{static_cast<DWORD>(plain.size()),
                 reinterpret_cast<BYTE *>(const_cast<char *>(plain.constData()))};
    DATA_BLOB out{0, nullptr};
    // Flags 0 = current user scope, which is what the official launcher wrote.
    if (!CryptProtectData(&in, L"Plutonium", nullptr, nullptr, nullptr, 0, &out)) {
        *ok = false;
        return {};
    }
    QByteArray blob(reinterpret_cast<const char *>(out.pbData), static_cast<int>(out.cbData));
    LocalFree(out.pbData);
    *ok = true;
    return blob;
}
#endif

void setError(QString *slot, const QString &text)
{
    if (slot)
        *slot = text;
}

QJsonObject readConfig(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    return QJsonDocument::fromJson(f.readAll()).object();
}

// The still-encrypted token, for "is there a login here at all" questions that
// must not pay for a DPAPI round trip or care whether it decrypts.
QString rawToken(const QString &plutoniumRoot)
{
    if (plutoniumRoot.isEmpty())
        return {};
    return readConfig(QDir(plutoniumRoot).filePath(QStringLiteral("config.json")))
        .value(QStringLiteral("token"))
        .toString();
}

// One blocking request. status is the HTTP code, or 0 for a transport failure.
QJsonObject apiCall(const QString &method, const QString &path, const QJsonObject &body,
                    const QString &userToken, int *status, QString *error)
{
    QNetworkAccessManager nam;
    QNetworkRequest req{QUrl(QLatin1String(kApiBase) + path)};
    req.setHeader(QNetworkRequest::UserAgentHeader, QLatin1String(kApiUserAgent));
    req.setRawHeader("Accept", "application/json");
    if (!userToken.isEmpty())
        req.setRawHeader("Authorization", QByteArray("UserToken ") + userToken.toUtf8());

    QNetworkReply *reply = nullptr;
    if (method == QLatin1String("POST")) {
        req.setHeader(QNetworkRequest::ContentTypeHeader, QLatin1String("application/json"));
        reply = nam.post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
    } else {
        reply = nam.get(req);
    }

    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    timeout.start(20000);
    loop.exec();

    if (!timeout.isActive()) {
        reply->abort();
        reply->deleteLater();
        *status = 0;
        setError(error, QObject::tr("The Plutonium login service did not answer in time."));
        return {};
    }

    const QByteArray raw = reply->readAll();
    *status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QNetworkReply::NetworkError netErr = reply->error();
    const QString netErrText = reply->errorString();
    reply->deleteLater();

    if (*status == 0) {
        setError(error, QObject::tr("Could not reach the Plutonium login service: %1").arg(netErrText));
        return {};
    }

    const QJsonObject obj = QJsonDocument::fromJson(raw).object();
    if (*status >= 400) {
        const QString apiErr = obj.value(QStringLiteral("error")).toString();
        if (!apiErr.isEmpty())
            setError(error, apiErr);
        else if (*status == 401)
            setError(error, QObject::tr("Plutonium rejected the saved login."));
        else if (netErr != QNetworkReply::NoError)
            setError(error, netErrText);
        else
            setError(error, QObject::tr("The Plutonium login service answered %1.").arg(*status));
    }
    return obj;
}

} // namespace

namespace PlutoniumAuth {

QString configPath(const QString &plutoniumRoot)
{
    return QDir(plutoniumRoot).filePath(QStringLiteral("config.json"));
}

QString tokenRoot(const QString &preferredRoot)
{
    if (!preferredRoot.isEmpty() && !rawToken(preferredRoot).isEmpty())
        return preferredRoot;
    const QString official = AppSettings::officialPlutoniumDir();
    if (!rawToken(official).isEmpty())
        return official;
    return preferredRoot.isEmpty() ? official : preferredRoot;
}

QString savedToken(const QString &plutoniumRoot, QString *error)
{
#ifndef Q_OS_WIN
    Q_UNUSED(plutoniumRoot);
    setError(error, dpapiError());
    return {};
#else
    const QJsonObject cfg = readConfig(configPath(plutoniumRoot));
    const QString b64 = cfg.value(QStringLiteral("token")).toString();
    if (b64.isEmpty()) {
        setError(error, QObject::tr("No Plutonium account is signed in on this PC yet."));
        return {};
    }
    bool ok = false;
    const QByteArray plain = dpapiUnprotect(QByteArray::fromBase64(b64.toUtf8()), &ok);
    if (!ok) {
        setError(error, dpapiError());
        return {};
    }
    // The launcher stores it NUL padded.
    QString token = QString::fromUtf8(plain);
    const int nul = token.indexOf(QChar(QChar::Null));
    if (nul >= 0)
        token.truncate(nul);
    return token.trimmed();
#endif
}

bool hasSavedToken(const QString &plutoniumRoot)
{
    return !savedToken(plutoniumRoot).isEmpty();
}

bool saveToken(const QString &plutoniumRoot, const QString &token, QString *error)
{
#ifndef Q_OS_WIN
    Q_UNUSED(plutoniumRoot);
    Q_UNUSED(token);
    setError(error, dpapiError());
    return false;
#else
    const QString path = configPath(plutoniumRoot);
    QJsonObject cfg = readConfig(path); // keep the launcher game paths intact
    if (token.isEmpty()) {
        cfg.remove(QStringLiteral("token"));
    } else {
        bool ok = false;
        const QByteArray blob = dpapiProtect(token.toUtf8(), &ok);
        if (!ok) {
            setError(error, dpapiError());
            return false;
        }
        cfg.insert(QStringLiteral("token"), QString::fromUtf8(blob.toBase64()));
    }
    QDir().mkpath(plutoniumRoot);
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        setError(error, QObject::tr("Could not write %1.").arg(QDir::toNativeSeparators(path)));
        return false;
    }
    f.write(QJsonDocument(cfg).toJson(QJsonDocument::Compact));
    f.close();
    return true;
#endif
}

bool clearToken(const QString &plutoniumRoot, QString *error)
{
    return saveToken(plutoniumRoot, QString(), error);
}

Account validate(const QString &userToken, QString *error)
{
    Account a;
    if (userToken.isEmpty()) {
        setError(error, QObject::tr("No Plutonium account is signed in on this PC yet."));
        return a;
    }
    int status = 0;
    const QJsonObject obj = apiCall(QStringLiteral("GET"), QStringLiteral("/api/auth/validate"),
                                    {}, userToken, &status, error);
    if (status != 200)
        return a;
    a.ok = true;
    a.userId = obj.value(QStringLiteral("userId")).toInt();
    a.username = obj.value(QStringLiteral("username")).toString();
    a.email = obj.value(QStringLiteral("email")).toString();
    a.emailVerified = obj.value(QStringLiteral("emailVerified")).toBool(true);
    return a;
}

QString login(const QString &username, const QString &password, Account *account, QString *error)
{
    QJsonObject body;
    body.insert(QStringLiteral("username"), username);
    body.insert(QStringLiteral("password"), password);
    int status = 0;
    const QJsonObject obj = apiCall(QStringLiteral("POST"), QStringLiteral("/api/auth/login"),
                                    body, QString(), &status, error);
    if (status != 200)
        return {};
    const QString token = obj.value(QStringLiteral("token")).toString();
    if (token.isEmpty()) {
        setError(error, QObject::tr("Plutonium signed in but returned no token."));
        return {};
    }
    if (account) {
        account->ok = true;
        account->userId = obj.value(QStringLiteral("userId")).toInt();
        account->username = obj.value(QStringLiteral("username")).toString(username);
        account->email = obj.value(QStringLiteral("email")).toString();
        account->emailVerified = obj.value(QStringLiteral("emailVerified")).toBool(true);
    }
    return token;
}

QString createSession(const QString &userToken, const QString &game, QString *error)
{
    QJsonObject body;
    body.insert(QStringLiteral("game"), game);
    int status = 0;
    const QJsonObject obj = apiCall(QStringLiteral("POST"), QStringLiteral("/api/auth/session"),
                                    body, userToken, &status, error);
    if (status != 200)
        return {};
    const QString token = obj.value(QStringLiteral("token")).toString();
    if (token.isEmpty())
        setError(error, QObject::tr("Plutonium returned an empty session token."));
    return token;
}

QString accountSummary(const QString &plutoniumRoot)
{
    const QString token = savedToken(plutoniumRoot);
    if (token.isEmpty())
        return QObject::tr("Not signed in");
    const Account a = validate(token);
    if (!a.ok)
        return QObject::tr("Saved login expired");
    return QObject::tr("Signed in as %1").arg(a.username);
}

} // namespace PlutoniumAuth
