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
        bool needsLogin = false;    // no usable Plutonium account; offer sign-in and retry
    };

    Result launch(AppSettings &settings, const QString &modSelection);

    // Online launch straight into the bootstrapper. We mint the session token
    // from the signed-in account ourselves (PlutoniumAuth), which is the step
    // the official launcher exists to perform, so no launcher window appears
    // and the bootstrapper never answers "Could not authenticate". Sets
    // needsLogin when no account is signed in. A mod cannot be pre-loaded
    // online; fs_game is a LAN-only mechanism.
    Result launchOnline(AppSettings &settings);
    bool onlineHandlerRegistered();

    // ReShade watchdog: ships in the exe, unpacked next to it under tools\.
    // Returns the pid of the PowerShell window, or 0 with an error.
    qint64 startReShadeWatchdog(const QString &plutoniumRoot, QString &error);
    qint64 runningReShadeWatchdog();   // pid of the one this app started, or 0
    void stopReShadeWatchdog();        // called when the app closes
    bool reShadeInstalled(const QString &plutoniumRoot);
    Result launchServer(AppSettings &settings, const QString &modSelection,
                         const QString &configSelection, const QString &port);
    bool terminatePid(qint64 pid);
    bool isPidRunning(qint64 pid);
    void startErrorDialogWatch(qint64 pid);
    qint64 findProcessInFolder(const QString &folder);
    qint64 findProcessByName(const QString &exeName);
    void terminateFolderProcesses(const QString &folder);
}
