#pragma once
#include <QRegularExpression>
#include <QString>
#include <QStringList>

// One place to decide whether a version is genuinely newer than another.
//
// Every "Update" button in this app is a claim that upstream is ahead of what
// the user has. Both of the strings involved arrive from somewhere sloppy - a
// GitHub release tag, a mod.json field an author typed, a catalog entry - so
// the rule is: only say Update when we can actually show it, and read anything
// else as up to date. A button that asks forever, and still asks after the
// user does what it said, is the worse failure.
namespace VersionCompare
{

// Lowercase, trimmed, no leading "v", no T6 colour codes ("^32.16.16").
inline QString normalise(QString s)
{
    s = s.remove(QRegularExpression(QStringLiteral("\\^[0-9]"))).trimmed().toLower();
    if (s.startsWith(QLatin1Char('v')))
        s.remove(0, 1);
    return s;
}

// A version we are willing to reason about: digits and dots only. Release tags
// such as "beta2" deliberately fail this, because they are not comparable with
// a version like "2.0" - they are a different naming scheme for the same thing.
inline bool isComparable(const QString &normalised)
{
    static const QRegularExpression re(QStringLiteral("^[0-9]+(\\.[0-9]+)*$"));
    return !normalised.isEmpty() && re.match(normalised).hasMatch();
}

// -1 a<b, 0 equal, 1 a>b. Only meaningful for two isComparable() strings.
inline int compare(const QString &a, const QString &b)
{
    const QStringList pa = a.split(QLatin1Char('.'));
    const QStringList pb = b.split(QLatin1Char('.'));
    for (int i = 0; i < qMax(pa.size(), pb.size()); ++i) {
        const int va = i < pa.size() ? pa.at(i).toInt() : 0;
        const int vb = i < pb.size() ? pb.at(i).toInt() : 0;
        if (va != vb)
            return va < vb ? -1 : 1;
    }
    return 0;
}

// The question every caller actually has: should we offer an update from
// `have` to `latest`? True only when both parse and latest is strictly ahead.
inline bool isUpdate(const QString &have, const QString &latest)
{
    const QString h = normalise(have);
    const QString l = normalise(latest);
    if (h.isEmpty() || l.isEmpty() || h == l)
        return false;
    if (!isComparable(h) || !isComparable(l))
        return false;
    return compare(l, h) > 0;
}

} // namespace VersionCompare
