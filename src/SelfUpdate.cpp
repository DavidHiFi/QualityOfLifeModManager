#include "SelfUpdate.h"
#include "AppSettings.h"
#include "VersionCompare.h"
#include "Version.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QSettings>
#include <QThread>
#include <QWidget>

#ifdef Q_OS_WIN
#  include <windows.h>
#  include <tlhelp32.h>
#endif

namespace {

const char kGroup[]    = "Updates";
const char kPending[]  = "launcher/pending";
const char kAttempts[] = "launcher/attempts";
const char kError[]    = "launcher/error";

QSettings ini()
{
    return QSettings(AppSettings::iniPath(), QSettings::IniFormat);
}

QString readKey(const char *key)
{
    QSettings s = ini();
    s.beginGroup(QLatin1String(kGroup));
    return s.value(QLatin1String(key)).toString();
}

void writeKeys(const QString &pending, int attempts, const QString &error)
{
    QSettings s = ini();
    s.beginGroup(QLatin1String(kGroup));
    if (pending.isEmpty()) {
        s.remove(QLatin1String(kPending));
        s.remove(QLatin1String(kAttempts));
        s.remove(QLatin1String(kError));
    } else {
        s.setValue(QLatin1String(kPending), pending);
        s.setValue(QLatin1String(kAttempts), attempts);
        if (error.isEmpty())
            s.remove(QLatin1String(kError));
        else
            s.setValue(QLatin1String(kError), error);
    }
    s.endGroup();
    s.sync();
}

#ifdef Q_OS_WIN
QString imagePathOf(DWORD pid)
{
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!h)
        return {};
    wchar_t buf[MAX_PATH * 2];
    DWORD len = DWORD(sizeof(buf) / sizeof(buf[0]));
    QString out;
    if (QueryFullProcessImageNameW(h, 0, buf, &len))
        out = QString::fromWCharArray(buf, int(len));
    CloseHandle(h);
    return QDir::cleanPath(out);
}

BOOL CALLBACK postCloseToWindow(HWND hwnd, LPARAM param)
{
    DWORD owner = 0;
    GetWindowThreadProcessId(hwnd, &owner);
    if (owner == DWORD(param))
        PostMessageW(hwnd, WM_CLOSE, 0, 0);
    return TRUE;
}
#endif

// The batch the helper runs. Written as one string so the quoting, the retries
// and the verification stay together and readable.
QString helperScript()
{
    return QStringLiteral(
        "@echo off\r\n"
        "setlocal\r\n"
        "set \"EXE=%~1\"\r\n"
        "set \"NEW=%~2\"\r\n"
        "set \"PID=%~3\"\r\n"
        "set \"LOG=%~4\"\r\n"
        ">\"%LOG%\" echo [start] %DATE% %TIME%\r\n"
        ">>\"%LOG%\" echo exe=%EXE%\r\n"
        ">>\"%LOG%\" echo new=%NEW%\r\n"
        "\r\n"
        "rem The app that asked for this still holds the file open. Wait it out,\r\n"
        "rem then stop waiting: a build that hangs on exit must not be able to\r\n"
        "rem block its own replacement forever.\r\n"
        "set /a n=0\r\n"
        ":wait\r\n"
        "tasklist /FI \"PID eq %PID%\" /NH 2>nul | find \"%PID%\" >nul\r\n"
        "if errorlevel 1 goto swap\r\n"
        "set /a n+=1\r\n"
        "if %n% GEQ 30 goto kill\r\n"
        "ping -n 2 127.0.0.1 >nul\r\n"
        "goto wait\r\n"
        ":kill\r\n"
        ">>\"%LOG%\" echo [warn] pid %PID% still running after 30s, terminating it\r\n"
        "taskkill /F /PID %PID% >>\"%LOG%\" 2>&1\r\n"
        "ping -n 3 127.0.0.1 >nul\r\n"
        "\r\n"
        ":swap\r\n"
        "set /a t=0\r\n"
        ":try\r\n"
        "copy /Y \"%NEW%\" \"%EXE%\" >>\"%LOG%\" 2>&1\r\n"
        "if not errorlevel 1 goto verify\r\n"
        "set /a t+=1\r\n"
        ">>\"%LOG%\" echo [retry %t%] copy refused, the file is still locked\r\n"
        "if %t% GEQ 8 goto failed\r\n"
        "ping -n 3 127.0.0.1 >nul\r\n"
        "goto try\r\n"
        "\r\n"
        ":verify\r\n"
        "rem copy reports success on a short write too, so compare the sizes.\r\n"
        "set \"SN=\"\r\n"
        "set \"SE=\"\r\n"
        "for %%A in (\"%NEW%\") do set \"SN=%%~zA\"\r\n"
        "for %%A in (\"%EXE%\") do set \"SE=%%~zA\"\r\n"
        "if not \"%SN%\"==\"%SE%\" goto failed\r\n"
        ">>\"%LOG%\" echo [ok] replaced, %SE% bytes\r\n"
        "del \"%NEW%\" >nul 2>nul\r\n"
        "start \"\" \"%EXE%\"\r\n"
        "del \"%~f0\" >nul 2>nul\r\n"
        "exit /b 0\r\n"
        "\r\n"
        ":failed\r\n"
        "rem Leave %NEW% where it is. Its presence is how the app knows, on the\r\n"
        "rem next start, that the build it is running is not the one it installed.\r\n"
        ">>\"%LOG%\" echo [failed] could not replace the app; another copy of it is still running\r\n"
        "start \"\" \"%EXE%\"\r\n"
        "del \"%~f0\" >nul 2>nul\r\n"
        "exit /b 1\r\n");
}

QString lastComplaint()
{
    QFile f(SelfUpdate::logPath());
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    QString found;
    while (!f.atEnd()) {
        const QString line = QString::fromLocal8Bit(f.readLine()).trimmed();
        if (line.startsWith(QLatin1String("[failed]")) || line.startsWith(QLatin1String("[warn]")))
            found = line.section(QLatin1Char(']'), 1).trimmed();
    }
    return found;
}

} // namespace

namespace SelfUpdate {

QString stagedPath()
{
    return QCoreApplication::applicationFilePath() + QStringLiteral(".new");
}

QString logPath()
{
    return QCoreApplication::applicationFilePath() + QStringLiteral(".update-log.txt");
}

QList<quint32> siblingProcesses()
{
    QList<quint32> out;
#ifdef Q_OS_WIN
    const QString self = QDir::cleanPath(QCoreApplication::applicationFilePath());
    const QString base = QFileInfo(self).fileName();
    const DWORD selfPid = GetCurrentProcessId();

    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE)
        return out;
    PROCESSENTRY32W pe;
    pe.dwSize = sizeof(pe);
    if (Process32FirstW(snap, &pe)) {
        do {
            if (pe.th32ProcessID == selfPid || pe.th32ProcessID == 0)
                continue;
            if (base.compare(QString::fromWCharArray(pe.szExeFile), Qt::CaseInsensitive) != 0)
                continue;
            // Same name is not enough: a portable copy somewhere else is not
            // ours to close, and it is not what holds this file either.
            if (imagePathOf(pe.th32ProcessID).compare(self, Qt::CaseInsensitive) != 0)
                continue;
            out << quint32(pe.th32ProcessID);
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
#endif
    return out;
}

int closeSiblings(int graceMs)
{
#ifdef Q_OS_WIN
    QList<quint32> pids = siblingProcesses();
    if (pids.isEmpty())
        return 0;

    for (quint32 pid : pids)
        EnumWindows(postCloseToWindow, LPARAM(pid));

    QElapsedTimer t;
    t.start();
    while (t.elapsed() < graceMs) {
        QThread::msleep(150);
        pids = siblingProcesses();
        if (pids.isEmpty())
            return 0;
    }

    // Still there. They are copies of this same app and the user has just asked
    // for it to be replaced, so stop asking.
    for (quint32 pid : pids) {
        if (HANDLE h = OpenProcess(PROCESS_TERMINATE, FALSE, DWORD(pid))) {
            TerminateProcess(h, 0);
            CloseHandle(h);
        }
    }
    QThread::msleep(600);
    return siblingProcesses().size();
#else
    Q_UNUSED(graceMs);
    return 0;
#endif
}

bool begin(const QString &downloaded, const QString &version, QString &error)
{
#ifdef Q_OS_WIN
    const QString exe = QCoreApplication::applicationFilePath();
    const QString staged = stagedPath();

    QFile::remove(staged);
    if (!QFile::copy(downloaded, staged)) {
        error = QCoreApplication::translate(
            "SelfUpdate", "Could not write the new build next to the app.");
        return false;
    }
    QFile::remove(downloaded);

    // Count the attempt before making it. If this build never comes back, the
    // count is the only thing that will still be true.
    const QString previous = readKey(kPending);
    const int attempts = (previous == version) ? readKey(kAttempts).toInt() + 1 : 1;
    writeKeys(version, attempts, QString());

    // Whatever else is running this exe holds a write lock on it, and the copy
    // cannot happen until it lets go.
    const int stubborn = closeSiblings();

    const QString bat = QDir::temp().filePath(QStringLiteral("qol_self_update.cmd"));
    QFile f(bat);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        error = QCoreApplication::translate("SelfUpdate", "Could not write the updater helper.");
        return false;
    }
    f.write(helperScript().toLatin1());
    f.close();

    // cmd's copy tolerates forward slashes; its del does not, and Qt hands out
    // paths like H:/x/y.exe. Give the batch native separators throughout or it
    // leaves the 12 MB staged build behind after a successful swap.
    const QString pid = QString::number(QCoreApplication::applicationPid());
    if (!QProcess::startDetached(QStringLiteral("cmd.exe"),
                                 {QStringLiteral("/c"),
                                  QDir::toNativeSeparators(bat),
                                  QDir::toNativeSeparators(exe),
                                  QDir::toNativeSeparators(staged),
                                  pid,
                                  QDir::toNativeSeparators(logPath())},
                                 QCoreApplication::applicationDirPath())) {
        error = QCoreApplication::translate("SelfUpdate", "Could not start the updater helper.");
        return false;
    }
    if (stubborn > 0) {
        // Not fatal - the helper retries for half a minute - but worth saying.
        QFile log(logPath());
        if (log.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
            log.write(QStringLiteral("[warn] %1 other copies of the app would not close\r\n")
                          .arg(stubborn).toLatin1());
    }
    return true;
#else
    Q_UNUSED(downloaded); Q_UNUSED(version);
    error = QCoreApplication::translate("SelfUpdate", "Self-update is Windows only.");
    return false;
#endif
}

Outcome reconcile()
{
    Outcome out;
    const QString pending = readKey(kPending);
    const QString running = QStringLiteral(CLL_VERSION);

    if (pending.isEmpty()) {
        // No attempt on record. A staged build left over from an older release
        // of this app is just clutter; the version on disk is the one running.
        QFile::remove(stagedPath());
        QSettings s = ini();
        s.beginGroup(QLatin1String(kGroup));
        if (s.value(QStringLiteral("launcher/version")).toString() != running) {
            s.setValue(QStringLiteral("launcher/version"), running);
            s.endGroup();
            s.sync();
        }
        return out;
    }

    // The only honest test of "did the update take" is the version of the build
    // that is now running - not a stamp the previous build wrote about itself.
    if (!VersionCompare::isUpdate(running, pending)) {
        writeKeys(QString(), 0, QString());
        QFile::remove(stagedPath());
        QFile::remove(logPath());
        QSettings s = ini();
        s.beginGroup(QLatin1String(kGroup));
        s.setValue(QStringLiteral("launcher/version"), running);
        s.endGroup();
        s.sync();
        return out;
    }

    out.failed = true;
    out.version = pending;
    out.attempts = qMax(1, readKey(kAttempts).toInt());
    // Whatever the helper wrote, in its words. It may have written nothing at
    // all - a build that was killed mid-swap leaves no complaint - and an empty
    // reason is better than a made-up one.
    out.reason = lastComplaint();
    writeKeys(pending, out.attempts, out.reason);
    return out;
}

bool isExhausted(const QString &version)
{
    return !version.isEmpty() && readKey(kPending) == version
           && readKey(kAttempts).toInt() >= 2;
}

void forget()
{
    writeKeys(QString(), 0, QString());
    QFile::remove(stagedPath());
}

void quitForSwap()
{
    if (qApp) {
        const auto windows = QApplication::topLevelWidgets();
        for (QWidget *w : windows)
            w->hide();
        QCoreApplication::processEvents(QEventLoop::AllEvents, 250);
    }
#ifdef Q_OS_WIN
    // Not quit(): an instance that stalls on the way out keeps the write lock
    // on the exe, and then no update can ever succeed again. The settings that
    // matter were synced before this point.
    ExitProcess(0);
#else
    ::_exit(0);
#endif
}

} // namespace SelfUpdate
