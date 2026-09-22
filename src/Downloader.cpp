#include "Downloader.h"
#include "Version.h"

#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>

namespace Downloader {

bool downloadToFile(const QString &url, const QString &destPath, QString &error,
                     const std::function<void(qint64, qint64)> &onProgress,
                     int timeoutMs)
{
    QDir().mkpath(QFileInfo(destPath).absolutePath());

    QFile file(destPath);
    if (!file.open(QIODevice::WriteOnly)) {
        error = QObject::tr("Nao foi possivel criar o arquivo: %1").arg(destPath);
        return false;
    }

    QNetworkAccessManager manager;
    QNetworkRequest request{QUrl(url)};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setHeader(QNetworkRequest::UserAgentHeader, QLatin1String(CLL_USER_AGENT));

    QNetworkReply *reply = manager.get(request);

    // Every byte the network handed us has to actually reach the disk. Without
    // this, a full disk or a failing write produced a short file and a cheerful
    // "true" - and the caller only found out when unpacking it failed a CRC
    // check, which reads like a corrupt download rather than a local problem.
    bool writeFailed = false;
    qint64 written = 0;
    const auto drain = [&]() {
        const QByteArray chunk = reply->readAll();
        if (chunk.isEmpty() || writeFailed)
            return;
        const qint64 n = file.write(chunk);
        if (n != chunk.size())
            writeFailed = true;
        else
            written += n;
    };

    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(reply, &QNetworkReply::readyRead, drain);
    if (onProgress) {
        QObject::connect(reply, &QNetworkReply::downloadProgress, [&](qint64 got, qint64 total) {
            onProgress(got, total);
        });
    }

    // safety timeout (2 minutes) so a dead network cannot hang forever
    QTimer timeoutTimer;
    timeoutTimer.setSingleShot(true);
    QObject::connect(&timeoutTimer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timeoutTimer.start(timeoutMs > 0 ? timeoutMs : 120000);

    loop.exec();

    drain();
    const bool flushed = file.flush();
    file.close();

    // What the server said it was going to send. A transfer that ends early
    // without an error - a dropped connection on a keep-alive socket, a proxy
    // that closes mid-stream - otherwise lands here looking like a success.
    // Only meaningful for an identity encoding: when the body arrived
    // compressed, Content-Length counts the compressed bytes and Qt hands us
    // the expanded ones, so the two legitimately differ.
    const QByteArray encoding =
        reply->rawHeader(QByteArrayLiteral("Content-Encoding")).trimmed().toLower();
    const bool plainBody = encoding.isEmpty() || encoding == QByteArrayLiteral("identity");
    const QVariant declared = reply->header(QNetworkRequest::ContentLengthHeader);
    const qint64 expected = (plainBody && declared.isValid()) ? declared.toLongLong() : -1;

    bool ok = reply->error() == QNetworkReply::NoError && timeoutTimer.isActive();
    if (ok && (writeFailed || !flushed)) {
        ok = false;
        error = QObject::tr("Could not write %1 to disk. It may be full, or the file may be in use.")
                    .arg(QDir::toNativeSeparators(destPath));
    } else if (ok && expected > 0 && written != expected) {
        ok = false;
        error = QObject::tr("The download of %1 ended early: %2 of %3 bytes. "
                            "Check your connection and try again.")
                    .arg(url).arg(written).arg(expected);
    } else if (!ok) {
        error = reply->error() != QNetworkReply::NoError
                    ? reply->errorString()
                    : QObject::tr("Tempo esgotado ao baixar %1").arg(url);
    }
    if (!ok)
        file.remove();
    reply->deleteLater();
    return ok;
}

QByteArray downloadBytes(const QString &url, QString &error, int timeoutMs)
{
    QNetworkAccessManager manager;
    QNetworkRequest request{QUrl(url)};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setHeader(QNetworkRequest::UserAgentHeader, QLatin1String(CLL_USER_AGENT));
    QNetworkReply *reply = manager.get(request);

    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QTimer timeoutTimer;
    timeoutTimer.setSingleShot(true);
    QObject::connect(&timeoutTimer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timeoutTimer.start(timeoutMs > 0 ? timeoutMs : 60000);
    loop.exec();

    QByteArray data;
    const bool ok = reply->error() == QNetworkReply::NoError && timeoutTimer.isActive();
    if (ok)
        data = reply->readAll();
    else
        error = reply->error() != QNetworkReply::NoError
                    ? reply->errorString()
                    : QObject::tr("Tempo esgotado ao baixar %1").arg(url);
    reply->deleteLater();
    return data;
}

QString redirectTargetOf(const QString &url, QString &error, int timeoutMs)
{
    QNetworkAccessManager manager;
    QNetworkRequest request{QUrl(url)};
    request.setHeader(QNetworkRequest::UserAgentHeader, QLatin1String(CLL_USER_AGENT));
    // Do not follow it: the redirect itself is the answer we want.
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::ManualRedirectPolicy);
    QNetworkReply *reply = manager.head(request);

    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    timeout.start(timeoutMs > 0 ? timeoutMs : 10000);
    loop.exec();

    if (!timeout.isActive()) {
        reply->abort();
        reply->deleteLater();
        error = QObject::tr("Timed out asking for %1").arg(url);
        return {};
    }
    const QVariant target = reply->attribute(QNetworkRequest::RedirectionTargetAttribute);
    const QString errText = reply->errorString();
    const bool failed = reply->error() != QNetworkReply::NoError;
    reply->deleteLater();
    if (!target.isValid()) {
        error = failed ? errText : QObject::tr("%1 did not redirect").arg(url);
        return {};
    }
    // A relative Location is legal; resolve it against what we asked for.
    return QUrl(url).resolved(target.toUrl()).toString();
}

} // namespace Downloader
