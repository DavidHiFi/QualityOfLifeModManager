#pragma once
#include <QByteArray>
#include <QString>
#include <functional>

// synchronous (blocks the worker thread only — callers run this via QtConcurrent)
namespace Downloader
{
    // Download url to destPath. onProgress is optional (received, total).
    bool downloadToFile(const QString &url, const QString &destPath, QString &error,
                         const std::function<void(qint64, qint64)> &onProgress = nullptr,
                         int timeoutMs = 120000);

    QByteArray downloadBytes(const QString &url, QString &error,
                             int timeoutMs = 60000);

    // The Location of a single redirect, without following it and without
    // downloading the body. GitHub answers /releases/latest/download/<asset>
    // with a redirect to /releases/download/<tag>/<asset>, which is how the
    // latest release tag is read without touching the rate-limited API.
    QString redirectTargetOf(const QString &url, QString &error, int timeoutMs = 10000);
}
