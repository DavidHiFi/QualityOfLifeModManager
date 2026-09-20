#include "AppSettings.h"
#include "Version.h"
#include "GameLauncher.h"
#include "Theme.h"
#include "MainWindow.h"
#include "QolService.h"
#include "PlutoniumAuth.h"

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
    parser.process(app);

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
        check("reshade install", QolService::installReShade(settings, err), err);
        check("reshade dxgi present", QolService::reShadeInstalled(settings));
        check("reshade vault present", QDir(QolService::reShadeVault(settings)).exists());
        check("reshade remove", QolService::removeReShade(settings, err), err);
        check("reshade dxgi gone", !QolService::reShadeInstalled(settings));
        const qint64 wd = GameLauncher::startReShadeWatchdog(settings.plutoniumInstance, err);
        check("watchdog started", wd > 0, err);
        if (wd > 0) GameLauncher::stopReShadeWatchdog();
        check("themes >= 15", Theme::names().size() >= 15, QString::number(Theme::names().size()));
        for (const QString &key : Theme::names()) {
            Theme::apply(key);
            check("theme " + key, Theme::token("BG").startsWith('#') && !Theme::displayName(key).isEmpty(), Theme::displayName(key));
        }
        check("series has t6", QolService::seriesFor("t6").released);
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
        QTimer::singleShot(100, &window, [&window, pageIndex]() { window.debugShowPage(pageIndex); });
        QTimer::singleShot(qMax(400, qEnvironmentVariableIntValue("QOL_SHOT_DELAY")), &window, [&window, screenshotPath]() {
            window.grab().save(screenshotPath);
            QApplication::quit();
        });
    }

    return app.exec();
}
