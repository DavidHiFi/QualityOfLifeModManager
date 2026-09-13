#include "GameCatalog.h"
#include "AppSettings.h"

#include <QColor>
#include <QDir>
#include <QFileInfo>
#include <QPixmap>

namespace {

QString firstExisting(const QStringList &candidates)
{
    for (const QString &c : candidates) {
        if (QFileInfo::exists(c))
            return c;
    }
    return QString();
}

QStringList variants(const QString &dir, const QString &stem)
{
    QStringList out;
    for (const char *ext : {".png", ".jpg", ".jpeg", ".webp"})
        out << QDir(dir).filePath(stem + ext);
    return out;
}

} // namespace

namespace GameCatalog {

QList<Game> all()
{
    return {
        {"t4",  "World at War",       "T4",  "World at War",       "Call of Duty · 2008", true},
        {"t5",  "Black ops",          "T5",  "Black Ops",          "Call of Duty · 2010", true},
        {"t6",  "Black ops II",       "T6",  "Black Ops II",       "Call of Duty · 2012", true},
        {"iw5", "Modern Warfare 3",   "IW5", "Modern Warfare 3",   "Call of Duty · 2011", false},
        {"s1",  "Advanced Warfare",   "S1",  "Advanced Warfare",   "Call of Duty · 2014", true},
        {"t7",  "Black ops III",      "T7",  "Black Ops III",      "Call of Duty · 2015", true},
    };
}

QList<Game> ordered(const QStringList &codes)
{
    const QList<Game> base = all();
    QList<Game> out;
    QStringList used;
    for (const QString &code : codes) {
        for (const Game &g : base) {
            if (g.code.compare(code, Qt::CaseInsensitive) == 0 && !used.contains(g.code)) {
                out << g;
                used << g.code;
                break;
            }
        }
    }
    for (const Game &g : base) {
        if (!used.contains(g.code))
            out << g;
    }
    return out;
}

Game byCode(const QString &code)
{
    for (const Game &g : all())
        if (g.code == code)
            return g;
    return all().first();
}

Game byId(const QString &id)
{
    for (const Game &g : all())
        if (g.id == id)
            return g;
    return all().first();
}

QString userMediaDir()
{
    return QDir(AppSettings::projectRoot()).filePath("media");
}

QString iconPath(const QString &code)
{
    const QString stem = code + "_icon";
    QString p = firstExisting(variants(userMediaDir(), stem));
    if (p.isEmpty())
        p = firstExisting({":/media/" + stem + ".jpg", ":/media/" + stem + ".png"});
    return p;
}

QString backgroundPath(const QString &code, const QString &variant)
{
    // Try the mode art first (t6_bg_mp / t6_bg_zm), then the generic file.
    QStringList stems;
    if (!variant.isEmpty())
        stems << code + "_bg_" + variant;
    stems << code + "_bg";
    if (variant != QLatin1String("mp"))
        stems << code + "_bg_mp";

    for (const QString &stem : stems) {
        QString p = firstExisting(variants(userMediaDir(), stem));
        if (p.isEmpty())
            p = firstExisting({":/media/" + stem + ".jpg", ":/media/" + stem + ".png"});
        if (!p.isEmpty())
            return p;
    }
    return QString();
}

QString logoPath(const QString &code)
{
    const QString stem = code + "_logo";
    QString p = firstExisting(variants(userMediaDir(), stem));
    if (p.isEmpty())
        p = firstExisting({":/media/" + stem + ".png", ":/media/" + stem + ".jpg"});
    return p;
}

QPixmap icon(const QString &code, const QSize &size)
{
    const QString path = iconPath(code);
    QPixmap pm;
    if (!path.isEmpty())
        pm.load(path);
    if (pm.isNull()) {
        pm = QPixmap(size);
        pm.fill(QColor("#1b1c21"));
    }
    return pm.scaled(size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}

QPixmap background(const QString &code, const QSize &size, const QString &variant)
{
    const QString path = backgroundPath(code, variant);
    QPixmap pm;
    if (!path.isEmpty())
        pm.load(path);
    if (pm.isNull()) {
        pm = QPixmap(size.isValid() ? size : QSize(1600, 900));
        pm.fill(QColor("#101120"));
    }
    if (size.isValid() && !size.isEmpty())
        return pm.scaled(size, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    return pm;
}

QPixmap logo(const QString &code, const QSize &size)
{
    const QString path = logoPath(code);
    QPixmap pm;
    if (!path.isEmpty())
        pm.load(path);
    if (pm.isNull())
        return QPixmap();
    if (size.isValid() && !size.isEmpty())
        return pm.scaled(size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    return pm;
}

} // namespace GameCatalog
