#pragma once
#include <QString>
#include "Version.h"
#include <QList>
#include <functional>

class AppSettings;
class QWidget;

namespace UpdateService
{
    inline const char kFeedUrl[] =
        QOL_UPDATE_FEED_URL;

    // A digest is only a digest if it can actually be compared against. The
    // feed is hand-maintained, and a typo in it must never be able to fail a
    // download: v2.2.1 published a 39-character sha1, and because "not empty"
    // was the only test, every installed copy refused its own update with
    // "SHA-1 mismatch" until the feed was corrected by hand. Anything that is
    // not exactly hexChars lowercase hex digits is dropped, with a warning, and
    // the download falls back to the next-strongest check.
    QString normalizedDigest(const QString &raw, int hexChars);

    struct Item {
        QString id;
        QString name;
        QString repo;
        QString version;
        QString url;
        QString hash;     // sha1 from the feed; empty once resolved live
        QString sha256;   // stronger digest, when the feed carries one
        QString feedVersion; // what the feed claimed, kept after a live lookup
        qint64 size = 0;
        bool live = false; // version/url came from the source repo, not the feed
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

    // Ask each component's own repository what its latest release is, instead
    // of trusting the version baked into the feed. The feed is a file somebody
    // regenerates by hand, so without this a new T7-CLL or S1-CLL release stays
    // invisible until that happens - the app would sit on one client version
    // forever. Only asks about components that are actually installed, and
    // leaves the feed's values in place for anything it cannot reach, so a
    // rate-limited or offline check degrades to the old behaviour.
    void resolveLatest(Catalog &cat, const AppSettings &settings);

    QList<Pending> detect(const Catalog &cat, const AppSettings &settings);

    void stamp(const QString &id, const QString &version, const QString &hash);
    void stampFile(const QString &id, const QString &version, const QString &filePath);
    QString stampedVersion(const QString &id);
    QString stampedHash(const QString &id);

    // Modal: lists pending items. Returns true if the app should quit (self-update).
    bool prompt(QWidget *parent, AppSettings &settings, bool silentIfNone);
}
