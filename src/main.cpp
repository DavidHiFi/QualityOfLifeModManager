#include "AppSettings.h"
#include "Version.h"
#include "GameLauncher.h"
#include "Theme.h"
#include "MainWindow.h"
#include "QolService.h"
#include "UpdateService.h"
#include "PlutoniumAuth.h"
#include "HomeCatalog.h"
#include "VersionCompare.h"

#include <QApplication>
#include <QGuiApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDir>
#include <QTextStream>
#include <QTimer>
#include <cstdio>

namespace {

// Headless mode: build the launch from CLI args and start the bootstrapper.
int runNoGui(AppSettings &settings, const QCommandLineParser &parser)
{
    settings.noGui = true;

    if (parser.isSet("plutoniumdir"))
        settings.plutoniumInstance = parser.value("plutoniumdir");
    if (parser.isSet("name"))
        settings.username = parser.value("name");

    const QString gameId = parser.value("gameid").toUpper();
    const QString mode = parser.value("mode").toUpper();

    if (gameId == "T4") {
        settings.modeId = (mode == "MP") ? "t4mp" : "t4sp";
        settings.gameId = "World at War";
        if (parser.isSet("gamedir")) settings.waw = parser.value("gamedir");
    } else if (gameId == "T5") {
        settings.modeId = (mode == "MP") ? "t5mp" : "t5sp";
        settings.gameId = "Black ops";
        if (parser.isSet("gamedir")) settings.bo1 = parser.value("gamedir");
    } else if (gameId == "T6") {
        settings.modeId = (mode == "MP") ? "t6mp" : "t6zm";
        settings.gameId = "Black ops II";
        if (parser.isSet("gamedir")) settings.bo2 = parser.value("gamedir");
    } else if (gameId == "IW5") {
        settings.modeId = "iw5mp";
        settings.gameId = "Modern Warfare 3";
        if (parser.isSet("gamedir")) settings.mw3 = parser.value("gamedir");
    } else {
        QTextStream(stdout) << "No valid game selected.\n";
        return 1;
    }

    QTextStream out(stdout);
    const bool online = parser.isSet("online");
    const GameLauncher::Result result = online
                                            ? GameLauncher::launchOnline(settings)
                                            : GameLauncher::launch(settings, QString());
    if (result.hasError) {
        if (!result.errorText.isEmpty())
            out << QOL_APP_NAME ": " << result.errorText << Qt::endl;
        else
            out << QOL_APP_NAME ": launch failed (check " QOL_INI_NAME " and the arguments)." << Qt::endl;
        if (result.needsLogin)
            out << "No Plutonium account is signed in. Start the app and press Play online once." << Qt::endl;
        return 1;
    }
    out << QOL_APP_NAME ": started " << settings.modeId << (online ? " online" : " on LAN")
        << ", pid " << result.pid << Qt::endl;
    return 0;
}

} // namespace

int main(int argc, char *argv[])
{
    // Fractional scaling (125 %, 150 %) rounds to the nearest integer by
    // default in Qt 6, which makes everything oversized on laptops. Pass
    // the real factor through and let the layouts scale.
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
    QApplication app(argc, argv);
    QApplication::setApplicationName(QOL_APP_NAME);
    QApplication::setApplicationVersion(QStringLiteral(CLL_VERSION));
    QApplication::setOrganizationName(QOL_AUTHOR);
    QDir::setCurrent(QCoreApplication::applicationDirPath());

    QCommandLineParser parser;
    // Accepts "-nogui", "-gameid", etc. with a single dash
    // instead of treating them as clustered short options (-n -o -g -u -i).
    parser.setSingleDashWordOptionMode(QCommandLineParser::ParseAsLongOptions);
    parser.setApplicationDescription(
        QOL_APP_NAME ": install, update and launch mods for Call of Duty on Plutonium.");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption({"name", "In-game name. Default: the one set in the app.", "name"});
    parser.addOption({"plutoniumdir", "Local da pasta do Plutonium. Deve ser usado com -nogui.", "plutoniumdir"});
    parser.addOption({"mode", "Seletor de modo: \"MP\" ou \"ZM\". Deve ser usado com -nogui.", "mode", "ZM"});
    parser.addOption({"gamedir", "Local em que o jogo esta instalado. Deve ser usado com -nogui.", "gamedir"});
    parser.addOption({"gameid", "ID do jogo: T4, T5, T6, ou IW5. Deve ser usado com -nogui.", "gameid"});
    parser.addOption({"nogui", "Lanca sem interface grafica."});
    parser.addOption({"online", "With -nogui: play online (signs the launch with the saved Plutonium "
                                "account) instead of LAN."});
    parser.addOption({"selftest", "Offline checks against -plutoniumdir (ReShade install/remove, watchdog unpack, "
                                  "online handler probe); prints one line per check and exits non-zero on failure."});
    parser.addOption({"checkupdates", "Print what each component resolves to and what is pending, then exit. "
                                      "Shows whether a version came from the feed or from the component's own repo."});
    parser.process(app);

    if (parser.isSet("checkupdates")) {
        AppSettings settings = AppSettings::loadForStartup();
        if (parser.isSet("plutoniumdir"))
            settings.plutoniumInstance = AppSettings::resolvePath(parser.value("plutoniumdir"));
        QTextStream out(stdout);
        QString error;
        UpdateService::Catalog cat = UpdateService::fetch(error);
        if (cat.items.isEmpty()) {
            out << "feed: " << (error.isEmpty() ? QStringLiteral("empty") : error) << Qt::endl;
            return 1;
        }
        UpdateService::resolveLatest(cat, settings);
        for (const UpdateService::Item &it : cat.items) {
            out << (it.live ? "live " : "feed ") << it.id << " = " << it.version;
            if (it.live && !it.feedVersion.isEmpty() && it.feedVersion != it.version)
                out << " (feed said " << it.feedVersion << ")";
            const QString have = (it.id == QLatin1String("launcher"))
                                     ? QStringLiteral(CLL_VERSION)
                                     : UpdateService::stampedVersion(it.id);
            if (!have.isEmpty())
                out << ", installed " << have;
            out << Qt::endl;
        }
        for (const UpdateService::Pending &p : UpdateService::detect(cat, settings))
            out << "PENDING " << p.title << " -> " << p.remote.version << Qt::endl;
        // The Quality of Life page asks its own question, through a different
        // path; show that too so both are visible in one run.
        for (const QolService::SeriesMod &m : QolService::series()) {
            if (!m.released)
                continue;
            QString e;
            const QString latest = QolService::latestModVersion(m, e);
            out << "series " << m.gameCode << " latest="
                << (latest.isEmpty() ? QStringLiteral("(unknown: ") + e + QLatin1Char(')') : latest)
                << ", installed " << QolService::installedModVersion(settings, m) << Qt::endl;
        }
        return 0;
    }

    if (parser.isSet("selftest")) {
        AppSettings settings = AppSettings::loadForStartup();
        if (parser.isSet("plutoniumdir"))
            settings.plutoniumInstance = AppSettings::resolvePath(parser.value("plutoniumdir"));
        QTextStream out(stdout);
        int failures = 0;
        auto check = [&](const QString &name, bool ok, const QString &detail = QString()) {
            out << (ok ? "PASS " : "FAIL ") << name;
            if (!detail.isEmpty()) out << " - " << detail;
            out << '\n';
            out.flush();
            if (!ok) ++failures;
        };
        auto note = [&](const QString &name, const QString &detail) {
            out << "INFO " << name << " - " << detail << Qt::endl;
        };
        check("plutonium root", AppSettings::isPlutoniumRoot(settings.plutoniumInstance), settings.plutoniumInstance);
        // Online play no longer needs the launcher or its protocol handler, so
        // these are reported, never failed: a PC with neither still plays online.
        note("plutonium:// handler (not required)",
             GameLauncher::onlineHandlerRegistered() ? "registered" : "absent");
        note("plutonium account", PlutoniumAuth::accountSummary(
                 PlutoniumAuth::tokenRoot(settings.plutoniumInstance)));
        QString err;
        // Every ReShade check runs against its own scratch root. They used to
        // use -plutoniumdir, which meant pointing the selftest at a real
        // install silently uninstalled that user's ReShade.
        AppSettings rs = settings;
        rs.plutoniumInstance = QDir(QDir::tempPath()).filePath(QStringLiteral("qol-selftest-reshade"));
        QDir(rs.plutoniumInstance).removeRecursively();
        QDir().mkpath(QDir(rs.plutoniumInstance).filePath(QStringLiteral("bin")));
        check("reshade install", QolService::installReShade(rs, err), err);
        check("reshade dxgi present", QolService::reShadeInstalled(rs));
        check("reshade vault present", QDir(QolService::reShadeVault(rs)).exists());
        check("reshade remove", QolService::removeReShade(rs, err), err);
        check("reshade dxgi gone", !QolService::reShadeInstalled(rs));
        // The regression: ReShade in bin with no manifest to remove it by (put
        // there by hand, by a repair, or by a script). Remove used to delete
        // nothing and still report success, so the button never changed.
        // Hermetic: its own scratch root, never the one passed in, so this can
        // never overwrite a real bin\dxgi.dll.
        {
            const AppSettings &scratch = rs;
            const QString bin = QDir(scratch.plutoniumInstance).filePath(QStringLiteral("bin"));
            QDir().mkpath(bin);
            QFile stray(QDir(bin).filePath(QStringLiteral("dxgi.dll")));
            const bool staged = stray.open(QIODevice::WriteOnly | QIODevice::Truncate)
                                && stray.write("not really reshade") > 0;
            stray.close();
            check("reshade unmanaged staged", staged && QolService::reShadeInstalled(scratch));
            QString rmErr;
            check("reshade remove without a manifest",
                  QolService::removeReShade(scratch, rmErr), rmErr);
            check("reshade unmanaged dxgi gone", !QolService::reShadeInstalled(scratch));
        }
        const qint64 wd = GameLauncher::startReShadeWatchdog(rs.plutoniumInstance, err);
        check("watchdog started", wd > 0, err);
        check("watchdog verifier unpacked",
              QDir(QCoreApplication::applicationDirPath()).exists(QStringLiteral("tools/reshade-verify.ps1")));
        if (wd > 0) GameLauncher::stopReShadeWatchdog();
        QDir(rs.plutoniumInstance).removeRecursively();
        check("themes >= 15", Theme::names().size() >= 15, QString::number(Theme::names().size()));
        for (const QString &key : Theme::names()) {
            Theme::apply(key);
            check("theme " + key, Theme::token("BG").startsWith('#') && !Theme::displayName(key).isEmpty(), Theme::displayName(key));
        }
        check("series has t6", QolService::seriesFor("t6").released);

        // Home card state. These are the cases that used to leave a card saying
        // "Update" forever, so they are checked, not just eyeballed.
        {
            auto presence = [](const QString &catalogVer, const QString &modVer,
                               const QString &releaseVer) {
                HomeCatalog::Mod m;
                m.url = QStringLiteral("https://github.com/Owner/Repo");
                m.version = catalogVer;
                HomeCatalog::LocalInstall li;
                li.sourceUrl = QStringLiteral("https://github.com/Owner/Repo/releases/latest/download/manifest.json");
                li.modVersion = modVer;
                li.releaseVersion = releaseVer;
                return HomeCatalog::presenceOf(m, {li});
            };
            using P = HomeCatalog::Presence;
            check("card: release tag beta2 vs version 2.0 is installed",
                  presence("2.0", "2.0", "beta2") == P::Installed);
            check("card: tag-only beta2 against 2.0 is not an update",
                  presence("2.0", QString(), "beta2") == P::Installed);
            check("card: local newer than catalog is installed",
                  presence("2.16.14", "2.16.16", QString()) == P::Installed);
            check("card: catalog newer than local is an update",
                  presence("2.16.20", "2.16.16", QString()) == P::Update);
            check("card: v-prefixed tag matches plain version",
                  presence("1.2", QString(), "v1.2") == P::Installed);
            HomeCatalog::Mod other;
            other.url = QStringLiteral("https://github.com/Someone/Else");
            other.version = QStringLiteral("1.0");
            check("card: unrelated mod is missing",
                  HomeCatalog::presenceOf(other, {}) == P::Missing);
            // The Quality of Life row on the QoL page asks the same question.
            check("row: v-tag against plain installed version is not an update",
                  !VersionCompare::isUpdate(QStringLiteral("v2.16.16"), QStringLiteral("2.16.16")));
            check("row: colour-coded installed version is not an update",
                  !VersionCompare::isUpdate(QStringLiteral("^32.16.16"), QStringLiteral("v2.16.16")));
            check("row: installed ahead of the release is not an update",
                  !VersionCompare::isUpdate(QStringLiteral("v2.16.16"), QStringLiteral("v2.16.14")));
            check("row: a newer release is an update",
                  VersionCompare::isUpdate(QStringLiteral("v2.16.16"), QStringLiteral("v2.17.0")));
        }

        // ...and what the real install actually reports, for the record.
        {
            const HomeCatalog::Catalog cat = HomeCatalog::load();
            const HomeCatalog::InstallIndex idx =
                HomeCatalog::scanInstalled(settings.plutoniumInstance);
            for (const HomeCatalog::Mod &m : cat.mods) {
                const HomeCatalog::Presence p = HomeCatalog::presenceOf(m, idx);
                note("card " + m.id,
                     QStringLiteral("catalog %1 -> %2").arg(
                         m.version,
                         p == HomeCatalog::Presence::Update      ? QStringLiteral("UPDATE")
                         : p == HomeCatalog::Presence::Installed ? QStringLiteral("installed")
                                                                 : QStringLiteral("not installed")));
            }
        }
        return failures == 0 ? 0 : 2;
    }

    if (parser.isSet("nogui")) {
        AppSettings settings = AppSettings::loadForStartup();
        return runNoGui(settings, parser);
    }

    MainWindow window;
    window.show();

    // Internal debug helper: dump a window screenshot when
    // QOL_SCREENSHOT=path.png is set (useful without an interactive display).
    const QString screenshotPath = qEnvironmentVariable("QOL_SCREENSHOT");
    if (!screenshotPath.isEmpty()) {
        const int pageIndex = qEnvironmentVariableIntValue("QOL_PAGE");
        const int gameIndex = qEnvironmentVariableIntValue("QOL_GAME");
        QTimer::singleShot(100, &window, [&window, pageIndex, gameIndex]() {
            if (qEnvironmentVariableIsSet("QOL_GAME"))
                window.debugSelectGame(gameIndex);
            else
                window.debugShowPage(pageIndex);
        });
        QTimer::singleShot(qMax(400, qEnvironmentVariableIntValue("QOL_SHOT_DELAY")), &window, [&window, screenshotPath]() {
            window.grab().save(screenshotPath);
            QApplication::quit();
        });
    }

    return app.exec();
}
