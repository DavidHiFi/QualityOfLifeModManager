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

    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(reply, &QNetworkReply::readyRead, [&]() {
        file.write(reply->readAll());
    });
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

    file.write(reply->readAll());
    file.close();

    const bool ok = reply->error() == QNetworkReply::NoError && timeoutTimer.isActive();
    if (!ok) {
        error = reply->error() != QNetworkReply::NoError
                    ? reply->errorString()
                    : QObject::tr("Tempo esgotado ao baixar %1").arg(url);
        file.remove();
    }
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
