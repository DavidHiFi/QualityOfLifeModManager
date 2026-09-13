#pragma once
#include <QString>
#include <QStringList>

// Settings stored in LanLauncher.ini plus session state.
class AppSettings
{
public:
    QString theme = "nocturne";
    QString username;
    QString plutoniumInstance;
    QString waw;
    QString bo1;
    QString bo2;
    QString mw3;
    QString aw;
    QString bo3;
    QString bo3Client = "cll"; // cll | competitive
    bool bo3ClientChosen = false;
    bool awClientReady = false;
    QString bo3CllArgs = QStringLiteral("-launch -noconsole -nowatermark -nointro");
    QString bo3CompArgs = QStringLiteral("-nointro");
    bool setupCompleted = false;
    QString language = "en";
    bool homeEnabled = true;
    QStringList gameOrder;
    bool gameOrderHintSeen = false;
    bool checkUpdatesOnStart = true;

    QString modId;
    QString gameId;
    QString serverId;
    QString modeId;          // t4mp, t6zm, iw5mp...
    bool    noGui = false;
    QString wawServMult;
    QString activeGame;
    bool serverMultiplayerSelected = false;

    static QString projectRoot();
    static QString iniPath();
    static QString localPuDir();
    static QString localPuBootstrapper();
    bool usingLocalPortableKit() const;
    static QString officialPlutoniumDir();
    static bool isPlutoniumRoot(const QString &root);
    static QString plutoniumRootSummary(const QString &root);
    // "./pu" is resolved to an absolute path next to the exe
    static QString resolvePath(const QString &path);

    void resolveStoredPaths();
    void useLocalPuIfPresent();
    bool loadFromIni();
    void saveToIni() const;
    void applyDefaultPlutoniumInstanceIfEmpty();
    QString gameFolder(const QString &gameIdName) const;
    static AppSettings loadForStartup();
};
