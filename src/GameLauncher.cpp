#include "GameLauncher.h"
#include "AppSettings.h"
#include "SmartModInstaller.h"
#include "Storage.h"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QHash>
#include <QSettings>
#include <QUrl>
#include <QList>
#include <QFileInfo>
#include <QProcess>
#include <QStringList>
#include <QThread>
#include <QtConcurrent>
#include <atomic>

#ifdef Q_OS_WIN
#  include <windows.h>
#  include <tlhelp32.h>
#else
#  include <signal.h>
#  include <errno.h>
#endif

namespace {

const QString kExePath = "/bin/plutonium-bootstrapper-win32.exe";
std::atomic<qint64> g_errorWatchPid{0};

QStringList splitFlag(const QString &flag)
{
    return flag.split(' ', Qt::SkipEmptyParts);
}

} // namespace

namespace GameLauncher {

Result launch(AppSettings &settings, const QString &modSelection)
{
    Result r;

    if (settings.gameId == QLatin1String("Advanced Warfare")) {
        settings.activeGame = settings.aw;
        const QString exe = QDir(settings.activeGame).filePath(QStringLiteral("s1.exe"));
        if (settings.activeGame.isEmpty() || !QFileInfo::exists(exe)) {
            r.hasError = true;
            r.errorMsg = Dialogs::Msg::S1;
            return r;
        }
        QStringList args;
        if (settings.modeId == QLatin1String("s1sp"))
            args << QStringLiteral("-singleplayer");
        else if (settings.modeId == QLatin1String("s1sv"))
            args << QStringLiteral("-survival");
        else if (settings.modeId == QLatin1String("s1zm"))
            args << QStringLiteral("-zombies");
        else
            args << QStringLiteral("-multiplayer");
        args << QStringLiteral("-noconsole") << QStringLiteral("-nowatermark");
        if (!settings.username.isEmpty())
            args << QStringLiteral("-nick") << settings.username;
        qint64 pid = 0;
        if (!QProcess::startDetached(exe, args, settings.activeGame, &pid)) {
            r.hasError = true;
            r.errorMsg = Dialogs::Msg::S1;
            return r;
        }
        r.ok = true;
        r.pid = pid;
        return r;
    }

    if (settings.gameId == QLatin1String("Black ops III")) {
        settings.activeGame = settings.bo3;
        QString exe;
        for (const QString &name : {QStringLiteral("boiii.exe"), QStringLiteral("t7.exe"),
                                    QStringLiteral("bo3.exe"), QStringLiteral("BlackOps3.exe")}) {
            const QString cand = QDir(settings.activeGame).filePath(name);
            if (QFileInfo::exists(cand)) {
                exe = cand;
                break;
            }
        }
        if (settings.activeGame.isEmpty() || exe.isEmpty()) {
            r.hasError = true;
            r.errorMsg = Dialogs::Msg::T7;
            return r;
        }
        QStringList args;
        const bool competitive = settings.bo3Client == QLatin1String("competitive");
        const QString raw = competitive ? settings.bo3CompArgs.trimmed()
                                        : settings.bo3CllArgs.trimmed();
        if (!raw.isEmpty())
            args = QProcess::splitCommand(raw);
        if (!competitive && !settings.username.isEmpty())
            args << QStringLiteral("-nick") << settings.username;
        qint64 pid = 0;
        if (!QProcess::startDetached(exe, args, settings.activeGame, &pid)) {
            r.hasError = true;
            r.errorMsg = Dialogs::Msg::T7;
            return r;
        }
        QThread::msleep(400);
        const qint64 live = findProcessInFolder(settings.activeGame);
        if (live > 0)
            pid = live;
        r.ok = true;
        r.pid = pid;
        if (competitive)
            startErrorDialogWatch(pid);
        return r;
    }

    if (settings.username.isEmpty()) {
        r.hasError = true;
        r.errorMsg = Dialogs::Msg::Username;
        return r;
    }

    const QString bootstrapper = settings.plutoniumInstance + kExePath;
    if (!QFileInfo::exists(bootstrapper)) {
        r.hasError = true;
        r.errorMsg = Dialogs::Msg::Plutonium;
        return r;
    }

    if (settings.gameId == "World at War") {
        settings.activeGame = settings.waw;
        if (!QFileInfo::exists(settings.activeGame + "/main/iw_00.iwd")) {
            r.hasError = true; r.errorMsg = Dialogs::Msg::T4; return r;
        }
    } else if (settings.gameId == "Black ops") {
        settings.activeGame = settings.bo1;
        if (!QFileInfo::exists(settings.activeGame + "/main/iw_00.iwd")) {
            r.hasError = true; r.errorMsg = Dialogs::Msg::T5; return r;
        }
    } else if (settings.gameId == "Black ops II") {
        settings.activeGame = settings.bo2;
        if (!QFileInfo::exists(settings.activeGame + "/zone/all/base.ipak")) {
            r.hasError = true; r.errorMsg = Dialogs::Msg::T6; return r;
        }
    } else if (settings.gameId == "Modern Warfare 3") {
        settings.activeGame = settings.mw3;
        if (!QFileInfo::exists(settings.activeGame + "/main/iw_00.iwd")) {
            r.hasError = true; r.errorMsg = Dialogs::Msg::IW5; return r;
        }
    }

    QStringList args;
    args << settings.modeId << settings.activeGame << "+name" << settings.username << "-lan";

    if (!modSelection.isEmpty()) {
        if (settings.gameId != settings.modId) {
            r.hasError = true;
            r.errorMsg = Dialogs::Msg::WrongGame;
            return r;
        }
        const QString sid = Storage::gameStorageId(settings.gameId);
        SmartModInstaller::cleanupEmptyAliasFolder(settings.plutoniumInstance, sid, modSelection);
        const QString folder = SmartModInstaller::resolveFsGameFolder(
            settings.plutoniumInstance, sid, modSelection);
        if (!folder.isEmpty())
            args << "+set" << "fs_game" << ("mods/" + folder);
    }

    qint64 pid = 0;
    if (!QProcess::startDetached(bootstrapper, args, settings.plutoniumInstance, &pid)) {
        r.hasError = true;
        r.errorMsg = Dialogs::Msg::Plutonium;
        return r;
    }
    r.ok = true;
    r.pid = pid;
    return r;
}

bool onlineHandlerRegistered()
{
#ifdef Q_OS_WIN
    // The Plutonium launcher registers plutonium:// under HKCU\Software\Classes
    // (or HKLM for an all-users install). A protocol key advertises an empty
    // "URL Protocol" value; the command value names the launcher exe.
    for (const QString &root : {QStringLiteral("HKEY_CURRENT_USER"), QStringLiteral("HKEY_LOCAL_MACHINE")}) {
        QSettings cls(root + QStringLiteral("\\Software\\Classes\\plutonium"), QSettings::NativeFormat);
        if (!cls.contains(QStringLiteral("URL Protocol")))
            continue;
        QSettings cmd(root + QStringLiteral("\\Software\\Classes\\plutonium\\shell\\open\\command"), QSettings::NativeFormat);
        QString c = cmd.value(QStringLiteral("Default")).toString();
        if (c.isEmpty())
            c = cmd.value(QStringLiteral(".")).toString();
        if (c.isEmpty())
            continue;
        QString exe = c;
        if (exe.startsWith(QLatin1Char('"')))
            exe = exe.mid(1, exe.indexOf(QLatin1Char('"'), 1) - 1);
        else
            exe = exe.section(QLatin1Char(' '), 0, 0);
        if (QFileInfo::exists(exe))
            return true;
    }
#endif
    return false;
}

Result launchOnline(AppSettings &settings)
{
    Result r;
    const QString code = settings.modeId.left(settings.modeId.startsWith(QLatin1String("iw5")) ? 3 : 2);
    if (code != QLatin1String("t4") && code != QLatin1String("t5")
        && code != QLatin1String("t6") && code != QLatin1String("iw5")) {
        r.hasError = true;
        r.errorText = QObject::tr("Online launch is only available for the Plutonium games.");
        return r;
    }
    // The handler always starts the registered install (%LOCALAPPDATA%\Plutonium).
    // Refuse when the app is pointed somewhere else, or the user would play from a
    // different folder than the one they configured.
    const QString official = QDir::cleanPath(AppSettings::officialPlutoniumDir()).toLower();
    const QString chosen = QDir::cleanPath(settings.plutoniumInstance).toLower();
    if (chosen != official) {
        r.hasError = true;
        r.errorText = QObject::tr("Online play always uses the Plutonium installed in %1.\n"
                                  "Point Settings at that folder, or play in LAN mode from the current one.")
                          .arg(QDir::toNativeSeparators(AppSettings::officialPlutoniumDir()));
        return r;
    }
    if (!onlineHandlerRegistered()) {
        r.hasError = true;
        r.errorText = QObject::tr("The Plutonium launcher is not registered on this PC.\n"
                                  "Install Plutonium from plutonium.pw and run it once, then try again.");
        return r;
    }
    if (!QDesktopServices::openUrl(QUrl(QStringLiteral("plutonium://play/") + settings.modeId))) {
        r.hasError = true;
        r.errorText = QObject::tr("Windows refused to open plutonium://play/%1.").arg(settings.modeId);
        return r;
    }
    r.ok = true;
    r.onlineHandoff = true;
    return r;
}

QString toolsDir()
{
    return QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("tools"));
}

// Copies a resource to tools\ when it is missing or older than the one built in.
bool unpackTool(const QString &name, QString &outPath, QString &error)
{
    QDir().mkpath(toolsDir());
    outPath = QDir(toolsDir()).filePath(name);
    QFile res(QStringLiteral(":/tools/") + name);
    if (!res.open(QIODevice::ReadOnly)) {
        error = QObject::tr("%1 is missing from this build.").arg(name);
        return false;
    }
    const QByteArray want = res.readAll();
    QFile have(outPath);
    if (have.open(QIODevice::ReadOnly) && have.readAll() == want)
        return true;
    have.close();
    if (!have.open(QIODevice::WriteOnly | QIODevice::Truncate) || have.write(want) != want.size()) {
        error = QObject::tr("Could not write %1.").arg(QDir::toNativeSeparators(outPath));
        return false;
    }
    return true;
}

bool reShadeInstalled(const QString &plutoniumRoot)
{
    return QFileInfo::exists(QDir(plutoniumRoot).filePath(QStringLiteral("bin/dxgi.dll")));
}

qint64 g_watchdogPid = 0;

qint64 runningReShadeWatchdog()
{
    return (g_watchdogPid > 0 && isPidRunning(g_watchdogPid)) ? g_watchdogPid : 0;
}

void stopReShadeWatchdog()
{
    if (g_watchdogPid > 0 && isPidRunning(g_watchdogPid))
        QProcess::execute(QStringLiteral("taskkill"), {QStringLiteral("/PID"), QString::number(g_watchdogPid),
                                                        QStringLiteral("/T"), QStringLiteral("/F")});
    g_watchdogPid = 0;
}

qint64 startReShadeWatchdog(const QString &plutoniumRoot, QString &error)
{
#ifndef Q_OS_WIN
    Q_UNUSED(plutoniumRoot);
    error = QObject::tr("ReShade is Windows only.");
    return 0;
#else
    // One watchdog per app session is enough; it watches every launch.
    if (const qint64 live = runningReShadeWatchdog())
        return live;
    QString script;
    if (!unpackTool(QStringLiteral("reshade-watchdog.ps1"), script, error))
        return 0;
    const QString ps = QDir(qEnvironmentVariable("SystemRoot", QStringLiteral("C:\\Windows")))
                           .filePath(QStringLiteral("System32/WindowsPowerShell/v1.0/powershell.exe"));
    qint64 pid = 0;
    const QStringList args{QStringLiteral("-NoProfile"), QStringLiteral("-ExecutionPolicy"), QStringLiteral("Bypass"),
                           QStringLiteral("-File"), QDir::toNativeSeparators(script),
                           QStringLiteral("-PlutoRoot"), QDir::toNativeSeparators(plutoniumRoot)};
    if (!QProcess::startDetached(ps, args, plutoniumRoot, &pid)) {
        error = QObject::tr("Could not start PowerShell for the ReShade watchdog.");
        return 0;
    }
    g_watchdogPid = pid;
    return pid;
#endif
}

Result launchServer(AppSettings &settings, const QString &modSelection,
                     const QString &configSelection, const QString &port)
{
    Result r;

    const QString bootstrapper = settings.plutoniumInstance + kExePath;
    if (!QFileInfo::exists(bootstrapper)) {
        r.hasError = true;
        r.errorMsg = Dialogs::Msg::Plutonium;
        return r;
    }

    if (settings.serverId == "World at War") {
        settings.activeGame = settings.waw;
        settings.wawServMult = settings.serverMultiplayerSelected ? QString() : QStringLiteral("+set zombiemode 1");
        if (!QFileInfo::exists(settings.activeGame + "/main/iw_00.iwd")) {
            r.hasError = true; r.errorMsg = Dialogs::Msg::T4; return r;
        }
    } else if (settings.serverId == "Black ops") {
        settings.activeGame = settings.bo1;
        if (!QFileInfo::exists(settings.activeGame + "/main/iw_00.iwd")) {
            r.hasError = true; r.errorMsg = Dialogs::Msg::T5; return r;
        }
    } else if (settings.serverId == "Black ops II") {
        if (!QDir(settings.plutoniumInstance + "/storage/t6/gamesettings").exists()) {
            r.hasError = true;
            r.needsBo2GameSettings = true;
            return r;
        }
        settings.activeGame = settings.bo2;
        if (!QFileInfo::exists(settings.activeGame + "/zone/all/base.ipak")) {
            r.hasError = true; r.errorMsg = Dialogs::Msg::T6; return r;
        }
    } else if (settings.serverId == "Modern Warfare 3") {
        settings.activeGame = settings.mw3;
        if (!QFileInfo::exists(settings.activeGame + "/main/iw_00.iwd")) {
            r.hasError = true; r.errorMsg = Dialogs::Msg::IW5; return r;
        }
    }

    QStringList args;
    args << settings.modeId << settings.activeGame << "-lan" << "-dedicated";
    args << splitFlag(settings.wawServMult);

    if (!modSelection.isEmpty()) {
        if (settings.serverId != settings.modId) {
            r.hasError = true;
            r.errorMsg = Dialogs::Msg::WrongGameServer;
            return r;
        }
        const QString sid = Storage::gameStorageId(settings.serverId);
        SmartModInstaller::cleanupEmptyAliasFolder(settings.plutoniumInstance, sid, modSelection);
        const QString folder = SmartModInstaller::resolveFsGameFolder(
            settings.plutoniumInstance, sid, modSelection);
        if (!folder.isEmpty())
            args << "+set" << "fs_game" << ("mods/" + folder);
    }

    args << "+exec" << configSelection << "+set" << "net_port" << port << "+map_rotate";

    qint64 pid = 0;
    if (!QProcess::startDetached(bootstrapper, args, settings.plutoniumInstance, &pid)) {
        r.hasError = true;
        r.errorMsg = Dialogs::Msg::Plutonium;
        return r;
    }
    r.ok = true;
    r.pid = pid;
    return r;
}

bool terminatePid(qint64 pid)
{
    if (pid <= 0)
        return false;
#ifdef Q_OS_WIN
    if (g_errorWatchPid.load() == pid)
        g_errorWatchPid.store(0);
#endif
#ifdef Q_OS_WIN
    // Ask the game to close first: WM_CLOSE on its top-level windows lets the
    // engine quit cleanly (saves settings, releases the shared bin folder for
    // any other Plutonium session). Force only if it is still alive afterwards.
    struct Ctx { DWORD pid; bool sent; };
    Ctx ctx{static_cast<DWORD>(pid), false};
    EnumWindows([](HWND hwnd, LPARAM lp) -> BOOL {
        auto *c = reinterpret_cast<Ctx *>(lp);
        DWORD wp = 0;
        GetWindowThreadProcessId(hwnd, &wp);
        if (wp == c->pid && GetWindow(hwnd, GW_OWNER) == nullptr) {
            wchar_t title[64] = {};
            GetWindowTextW(hwnd, title, 64);
            // Only the game's own window; the engine also owns a console window
            // whose WM_CLOSE is ignored.
            if (wcsncmp(title, L"Plutonium", 9) == 0) {
                PostMessageW(hwnd, WM_CLOSE, 0, 0);
                c->sent = true;
            }
        }
        return TRUE;
    }, reinterpret_cast<LPARAM>(&ctx));
    if (ctx.sent) {
        for (int i = 0; i < 80 && isPidRunning(pid); ++i)
            QThread::msleep(250);
    }
    if (!isPidRunning(pid))
        return true;
    return QProcess::startDetached("taskkill", {"/PID", QString::number(pid), "/T", "/F"});
#else
    return ::kill(static_cast<pid_t>(pid), SIGTERM) == 0 || errno == ESRCH;
#endif
}

#ifdef Q_OS_WIN
bool processRelatedTo(DWORD rootPid, DWORD pid)
{
    if (pid == 0 || rootPid == 0)
        return false;
    if (pid == rootPid)
        return true;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE)
        return false;
    PROCESSENTRY32W pe;
    pe.dwSize = sizeof(pe);
    QHash<DWORD, DWORD> parent;
    if (Process32FirstW(snap, &pe)) {
        do {
            parent.insert(pe.th32ProcessID, pe.th32ParentProcessID);
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    DWORD cur = pid;
    for (int i = 0; i < 16 && cur && cur != rootPid; ++i)
        cur = parent.value(cur, 0);
    return cur == rootPid;
}

bool clickDialogButton(HWND dialog)
{
    const wchar_t *labels[] = {L"OK", L"Ok", L"Fechar", L"Close", L"&OK"};
    for (const wchar_t *label : labels) {
        HWND btn = FindWindowExW(dialog, nullptr, L"Button", label);
        if (btn) {
            SendMessageW(btn, BM_CLICK, 0, 0);
            return true;
        }
    }
    HWND btn = GetDlgItem(dialog, IDOK);
    if (btn) {
        SendMessageW(btn, BM_CLICK, 0, 0);
        return true;
    }
    return false;
}

struct EnumCtx { DWORD rootPid; };

BOOL CALLBACK enumErrorDialogs(HWND hwnd, LPARAM lp)
{
    auto *ctx = reinterpret_cast<EnumCtx *>(lp);
    wchar_t cls[64] = {};
    wchar_t title[64] = {};
    GetClassNameW(hwnd, cls, 64);
    GetWindowTextW(hwnd, title, 64);
    if (lstrcmpW(cls, L"#32770") != 0 || lstrcmpW(title, L"Error") != 0)
        return TRUE;
    DWORD wndPid = 0;
    GetWindowThreadProcessId(hwnd, &wndPid);
    if (!processRelatedTo(ctx->rootPid, wndPid))
        return TRUE;
    clickDialogButton(hwnd);
    if (IsWindow(hwnd))
        PostMessageW(hwnd, WM_CLOSE, 0, 0);
    return TRUE;
}

void closeErrorDialogs(DWORD rootPid)
{
    EnumCtx ctx{rootPid};
    EnumWindows(enumErrorDialogs, reinterpret_cast<LPARAM>(&ctx));
}
#endif

#ifdef Q_OS_WIN
QString processImagePath(DWORD pid)
{
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!h)
        return {};
    wchar_t buf[MAX_PATH] = {};
    DWORD n = MAX_PATH;
    const BOOL ok = QueryFullProcessImageNameW(h, 0, buf, &n);
    CloseHandle(h);
    if (!ok)
        return {};
    return QDir::cleanPath(QString::fromWCharArray(buf));
}

bool pathIsUnder(const QString &filePath, const QString &folder)
{
    const QString root = QDir::cleanPath(folder).toLower() + QLatin1Char('/');
    const QString path = QDir::cleanPath(filePath).toLower();
    return path.startsWith(root) || path == QDir::cleanPath(folder).toLower();
}
#endif

qint64 findProcessInFolder(const QString &folder)
{
    if (folder.isEmpty())
        return 0;
#ifdef Q_OS_WIN
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE)
        return 0;
    PROCESSENTRY32W pe;
    pe.dwSize = sizeof(pe);
    qint64 found = 0;
    if (Process32FirstW(snap, &pe)) {
        do {
            const QString img = processImagePath(pe.th32ProcessID);
            if (!img.isEmpty() && pathIsUnder(img, folder)) {
                found = static_cast<qint64>(pe.th32ProcessID);
                break;
            }
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    return found;
#else
    Q_UNUSED(folder);
    return 0;
#endif
}

qint64 findProcessByName(const QString &exeName)
{
#ifdef Q_OS_WIN
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE)
        return 0;
    PROCESSENTRY32W pe;
    pe.dwSize = sizeof(pe);
    qint64 found = 0;
    if (Process32FirstW(snap, &pe)) {
        do {
            if (exeName.compare(QString::fromWCharArray(pe.szExeFile), Qt::CaseInsensitive) == 0) {
                found = static_cast<qint64>(pe.th32ProcessID);
                break;
            }
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    return found;
#else
    Q_UNUSED(exeName);
    return 0;
#endif
}

void terminateFolderProcesses(const QString &folder)
{
    g_errorWatchPid.store(0);
    if (folder.isEmpty())
        return;
#ifdef Q_OS_WIN
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE)
        return;
    PROCESSENTRY32W pe;
    pe.dwSize = sizeof(pe);
    QList<DWORD> pids;
    if (Process32FirstW(snap, &pe)) {
        do {
            const QString img = processImagePath(pe.th32ProcessID);
            if (!img.isEmpty() && pathIsUnder(img, folder))
                pids << pe.th32ProcessID;
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    for (DWORD pid : pids) {
        HANDLE h = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
        if (h) {
            TerminateProcess(h, 1);
            CloseHandle(h);
        }
        QProcess::execute(QStringLiteral("taskkill"),
                          {QStringLiteral("/F"), QStringLiteral("/T"),
                           QStringLiteral("/PID"), QString::number(pid)});
    }
    const QStringList names = {
        QStringLiteral("boiii.exe"), QStringLiteral("BlackOps3.exe"),
        QStringLiteral("t7.exe"), QStringLiteral("bo3.exe"),
        QStringLiteral("s1.exe")
    };
    for (const QString &name : names) {
        QProcess::execute(QStringLiteral("taskkill"),
                          {QStringLiteral("/F"), QStringLiteral("/T"),
                           QStringLiteral("/IM"), name});
    }
#else
    Q_UNUSED(folder);
#endif
}

void startErrorDialogWatch(qint64 pid)
{
#ifndef Q_OS_WIN
    Q_UNUSED(pid);
#else
    if (pid <= 0)
        return;
    g_errorWatchPid.store(pid);
    const QFuture<void> future = QtConcurrent::run([pid]() {
        const DWORD root = static_cast<DWORD>(pid);
        while (g_errorWatchPid.load() == pid && isPidRunning(pid)) {
            closeErrorDialogs(root);
            QThread::msleep(400);
        }
    });
    Q_UNUSED(future);
#endif
}

bool isPidRunning(qint64 pid)
{
    if (pid <= 0)
        return false;
#ifdef Q_OS_WIN
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, static_cast<DWORD>(pid));
    if (!h)
        return false;
    DWORD code = 0;
    const BOOL ok = GetExitCodeProcess(h, &code);
    CloseHandle(h);
    return ok && code == STILL_ACTIVE;
#else
    return ::kill(static_cast<pid_t>(pid), 0) == 0 || errno == EPERM;
#endif
}

} // namespace GameLauncher
