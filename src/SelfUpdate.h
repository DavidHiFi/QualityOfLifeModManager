#pragma once
#include <QList>
#include <QString>

// Replacing a running .exe is the step of an update that actually fails, and it
// used to fail silently. Windows locks an executable while any copy of it is
// running, so with a second instance alive the helper's copy never happened -
// and because nothing checked, the helper started the old build again and the
// app offered the same update on the next tick, forever.
//
// This module owns that step: it closes the app's other instances first, it
// verifies the swap really happened, and it writes down a failure so the next
// start can say so instead of looping.
namespace SelfUpdate
{
    // <exe>.new - the downloaded build, waiting to be swapped in.
    QString stagedPath();
    // <exe>.update-log.txt - what the helper did, in its own words.
    QString logPath();

    // Every other process running this same executable image. These are what
    // hold the write lock; the app's own PID is never in the list.
    QList<quint32> siblingProcesses();

    // Ask the siblings to close, then terminate whatever is still holding the
    // file. Returns how many were still alive when it gave up (0 is success).
    int closeSiblings(int graceMs = 4000);

    // Stage `downloaded` beside the exe and start the helper that swaps it in.
    // Records the attempt so a swap that does not take can be recognised later.
    bool begin(const QString &downloaded, const QString &version, QString &error);

    struct Outcome {
        bool failed = false;
        QString version;  // what the attempt was trying to install
        int attempts = 0; // consecutive failures at that version
        QString reason;   // the helper's last complaint, when it left one
    };

    // Call once at startup. Compares the version that is actually running
    // against the one the last attempt promised: clears the record on success,
    // counts the failure otherwise.
    Outcome reconcile();

    // An attempt at this version has failed twice. The automatic check stops
    // offering it, which is what breaks the loop; a manual check still works.
    bool isExhausted(const QString &version);

    // Forget the failure record - the user asked to try again, or installed by
    // hand.
    void forget();

    // Leave, now, so the file can be replaced. Hiding the windows and calling
    // quit() is not enough: an instance that hangs on the way out keeps the
    // lock and poisons every future update.
    [[noreturn]] void quitForSwap();
}
