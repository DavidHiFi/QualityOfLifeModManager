#pragma once
#include <QString>
#include <QList>
#include <functional>

class AppSettings;
class QWidget;

namespace UpdateService
{
    inline const char kFeedUrl[] =
        "https://mestretm.github.io/CLL-Cod-Lan-Launcher/cll_update.json";

    struct Item {
        QString id;
        QString name;
        QString repo;
        QString version;
        QString url;
        QString hash;
        qint64 size = 0;
    };

    struct Catalog {
        QList<Item> items;
        Item byId(const QString &id) const;
    };

    struct Pending {
        Item remote;
        QString title;
        QString detail;
    };

    Catalog fetch(QString &error);
    QList<Pending> detect(const Catalog &cat, const AppSettings &settings);

    void stamp(const QString &id, const QString &version, const QString &hash);
    void stampFile(const QString &id, const QString &version, const QString &filePath);
    QString stampedVersion(const QString &id);
    QString stampedHash(const QString &id);

    // Modal: lists pending items. Returns true if the app should quit (self-update).
    bool prompt(QWidget *parent, AppSettings &settings, bool silentIfNone);
}
