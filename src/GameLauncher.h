#pragma once
#include "Dialogs.h"
#include <QtGlobal>
#include <QString>

class AppSettings;

namespace GameLauncher
{
    struct Result
    {
        bool ok = false;
        bool hasError = false;
        Dialogs::Msg errorMsg = Dialogs::Msg::Plutonium;
        QString errorText;   // when non-empty, shown instead of errorMsg
        bool needsBo2GameSettings = false;
        qint64 pid = 0;
        bool onlineHandoff = false; // launched through plutonium://; no pid to track
    };

    Result launch(AppSettings &settings, const QString &modSelection);

    // Online launch through the registered plutonium://play/<modeId> handler.
    // The Plutonium launcher does the login; the bootstrapper alone answers
    // "Could not authenticate (401)". Only the registered install can be used,
    // and a mod cannot be pre-loaded (fs_game is a LAN-only mechanism).
    Result launchOnline(AppSettings &settings);
    bool onlineHandlerRegistered();

    // ReShade watchdog: ships in the exe, unpacked next to it under tools\.
    // Returns the pid of the PowerShell window, or 0 with an error.
    qint64 startReShadeWatchdog(const QString &plutoniumRoot, QString &error);
    bool reShadeInstalled(const QString &plutoniumRoot);
    Result launchServer(AppSettings &settings, const QString &modSelection,
                         const QString &configSelection, const QString &port);
    bool terminatePid(qint64 pid);
    bool isPidRunning(qint64 pid);
    void startErrorDialogWatch(qint64 pid);
    qint64 findProcessInFolder(const QString &folder);
    void terminateFolderProcesses(const QString &folder);
}
