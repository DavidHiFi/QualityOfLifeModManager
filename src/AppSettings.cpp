#include "AppSettings.h"
#include "Version.h"
#include "I18n.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QObject>
#include <QSettings>

QString AppSettings::projectRoot()
{
    return QCoreApplication::applicationDirPath();
}

QString AppSettings::iniPath()
{
    return QDir(projectRoot()).filePath(QOL_INI_NAME);
}

QString AppSettings::resolvePath(const QString &path)
{
    QString p = path.trimmed();
    if (p.isEmpty())
        return p;
    if (p == QLatin1String(".") || p == QLatin1String("./") || p == QLatin1String(".\\"))
        return QDir::cleanPath(projectRoot());
    while (p.startsWith(QLatin1String("./")) || p.startsWith(QLatin1String(".\\")))
        p = p.mid(2);
    if (p.startsWith(QLatin1Char('~'))) {
        const QString home = QDir::homePath();
        if (p == QLatin1String("~"))
            p = home;
        else if (p.startsWith(QLatin1String("~/")) || p.startsWith(QLatin1String("~\\")))
            p = home + p.mid(1);
    }
    const QFileInfo fi(p);
    if (fi.isAbsolute())
        return QDir::cleanPath(fi.absoluteFilePath());
    return QDir::cleanPath(QDir(projectRoot()).absoluteFilePath(p));
}

void AppSettings::resolveStoredPaths()
{
    plutoniumInstance = resolvePath(plutoniumInstance);
    waw = resolvePath(waw);
    bo1 = resolvePath(bo1);
    bo2 = resolvePath(bo2);
    mw3 = resolvePath(mw3);
    aw = resolvePath(aw);
    bo3 = resolvePath(bo3);
}

QString AppSettings::localPuDir()
{
    return QDir(projectRoot()).filePath("pu");
}

QString AppSettings::localPuBootstrapper()
{
    return QDir(localPuDir()).filePath("bin/plutonium-bootstrapper-win32.exe");
}

bool AppSettings::usingLocalPortableKit() const
{
    if (!isPlutoniumRoot(localPuDir()))
        return false;
    const QString a = QDir::cleanPath(QFileInfo(plutoniumInstance).absoluteFilePath());
    const QString b = QDir::cleanPath(localPuDir());
    return QString::compare(a, b, Qt::CaseInsensitive) == 0;
}

QString AppSettings::officialPlutoniumDir()
{
    const QString la = qEnvironmentVariable("LOCALAPPDATA");
    if (la.isEmpty())
        return QDir::homePath() + "/AppData/Local/Plutonium";
    return QDir(la).filePath("Plutonium");
}

bool AppSettings::isPlutoniumRoot(const QString &root)
{
    if (root.trimmed().isEmpty())
        return false;
    return QFileInfo::exists(QDir(root).filePath("bin/plutonium-bootstrapper-win32.exe"));
}

QString AppSettings::plutoniumRootSummary(const QString &root)
{
    if (root.trimmed().isEmpty())
        return QObject::tr("Pasta vazia.");
    QStringList bits;
    bits << (isPlutoniumRoot(root)
                 ? QObject::tr("bootstrapper OK")
                 : QObject::tr("bootstrapper AUSENTE"));
    const QString games = QDir(root).filePath("games");
    bits << (QDir(games).exists() ? QObject::tr("games OK") : QObject::tr("games ausente"));
    for (const QString &g : QStringList{"t4", "t5", "t6", "iw5"}) {
        const bool ok = QDir(QDir(root).filePath("storage/" + g)).exists();
        bits << (ok ? (g + QObject::tr(" OK")) : (g + QObject::tr(" ausente")));
    }
    return bits.join(QStringLiteral(" · "));
}

void AppSettings::useLocalPuIfPresent()
{
    if (QFileInfo::exists(localPuBootstrapper()))
        plutoniumInstance = localPuDir();
}

bool AppSettings::loadFromIni()
{
    if (!QFileInfo::exists(iniPath()))
        return false;

    QSettings ini(iniPath(), QSettings::IniFormat);
    ini.beginGroup("QualityOfLife");
    username          = ini.value("username", username).toString();
    plutoniumInstance = ini.value("plutonium folder", plutoniumInstance).toString();
    waw               = ini.value("world at war folder", waw).toString();
    bo1               = ini.value("black ops 1 folder", bo1).toString();
    bo2               = ini.value("black ops 2 folder", bo2).toString();
    mw3               = ini.value("modernwarfare 3 folder", mw3).toString();
    aw                = ini.value("advanced warfare folder", aw).toString();
    bo3               = ini.value("black ops 3 folder", bo3).toString();
    theme             = ini.value("theme", theme).toString();
    setupCompleted    = ini.value("setup completed", false).toBool();
    language          = ini.value("language", language).toString();
    homeEnabled       = ini.value("home enabled", true).toBool();
    gameOrder         = ini.value("game order").toString().split(QLatin1Char(','), Qt::SkipEmptyParts);
    gameOrderHintSeen = ini.value("game order hint seen", false).toBool();
    checkUpdatesOnStart = ini.value("check updates on start", true).toBool();
    launchOnline      = ini.value("launch online", false).toBool();
    launchReShade     = ini.value("launch reshade", false).toBool();
    bo3Client         = ini.value("bo3 client", QStringLiteral("cll")).toString();
    if (bo3Client != QLatin1String("competitive"))
        bo3Client = QStringLiteral("cll");
    bo3ClientChosen   = ini.value("bo3 client chosen", false).toBool();
    awClientReady     = ini.value("aw client ready", false).toBool();
    bo3CllArgs        = ini.value("bo3 cll args",
                                  QStringLiteral("-launch -noconsole -nowatermark -nointro")).toString();
    bo3CompArgs       = ini.value("bo3 competitive args",
                                  QStringLiteral("-nointro")).toString();
    if (!setupCompleted && (!username.isEmpty() || !waw.isEmpty() || !bo1.isEmpty() || !bo2.isEmpty() || !mw3.isEmpty() || !aw.isEmpty() || !bo3.isEmpty()))
        setupCompleted = true;
    ini.endGroup();
    resolveStoredPaths();
    return true;
}

void AppSettings::saveToIni() const
{
    AppSettings copy = *this;
    copy.resolveStoredPaths();
    QSettings ini(iniPath(), QSettings::IniFormat);
    ini.beginGroup("QualityOfLife");
    ini.setValue("username", copy.username);
    ini.setValue("plutonium folder", copy.plutoniumInstance);
    ini.setValue("world at war folder", copy.waw);
    ini.setValue("black ops 1 folder", copy.bo1);
    ini.setValue("black ops 2 folder", copy.bo2);
    ini.setValue("modernwarfare 3 folder", copy.mw3);
    ini.setValue("advanced warfare folder", copy.aw);
    ini.setValue("black ops 3 folder", copy.bo3);
    ini.setValue("theme", copy.theme);
    ini.setValue("setup completed", copy.setupCompleted);
    ini.setValue("language", copy.language);
    ini.setValue("home enabled", copy.homeEnabled);
    ini.setValue("game order", copy.gameOrder.join(QLatin1Char(',')));
    ini.setValue("game order hint seen", copy.gameOrderHintSeen);
    ini.setValue("check updates on start", copy.checkUpdatesOnStart);
    ini.setValue("launch online", copy.launchOnline);
    ini.setValue("launch reshade", copy.launchReShade);
    ini.setValue("bo3 client", copy.bo3Client);
    ini.setValue("bo3 client chosen", copy.bo3ClientChosen);
    ini.setValue("aw client ready", copy.awClientReady);
    ini.setValue("bo3 cll args", copy.bo3CllArgs);
    ini.setValue("bo3 competitive args", copy.bo3CompArgs);
    ini.endGroup();
    ini.sync();
}

void AppSettings::applyDefaultPlutoniumInstanceIfEmpty()
{
    if (!plutoniumInstance.isEmpty())
        return;

    const QString candidate = QDir::homePath() + "/AppData/Local/Plutonium";
    if (QDir(candidate).exists())
        plutoniumInstance = candidate;
}

QString AppSettings::gameFolder(const QString &gameIdName) const
{
    if (gameIdName == "World at War")       return waw;
    if (gameIdName == "Black ops")          return bo1;
    if (gameIdName == "Black ops II")       return bo2;
    if (gameIdName == "Modern Warfare 3")   return mw3;
    if (gameIdName == "Advanced Warfare")   return aw;
    if (gameIdName == "Black ops III")      return bo3;
    return QString();
}

AppSettings AppSettings::loadForStartup()
{
    AppSettings s;
    s.loadFromIni();
    if (s.language.trimmed().isEmpty())
        s.language = QStringLiteral("en");
    s.resolveStoredPaths();
    s.useLocalPuIfPresent();
    s.applyDefaultPlutoniumInstanceIfEmpty();
    s.resolveStoredPaths();
    return s;
}
