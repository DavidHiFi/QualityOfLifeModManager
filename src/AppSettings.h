#pragma once
#include <QString>
#include <QStringList>

// Settings stored in QualityOfLife.ini (next to the exe) plus session state.
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
    // Play page launch options (persisted).
    bool launchOnline = false;   // false = LAN (bootstrapper), true = plutonium://play/<id> through the launcher
    bool launchReShade = false;  // start the ReShade watchdog beside the game
    // DLSS 5 is its own choice, not part of ReShade: the neural pass runs in
    // a 64-bit helper process and this 32-bit game waits on the round trip
    // every frame. Off unless asked for.
    //
    // An earlier note here put that cost at "120 fps -> 30". That number was
    // wrong: every measurement behind it was taken with the game in the
    // background while NVIDIA's background frame-rate limit was set to 30,
    // which pins any game to a flat 30.0 with the GPU idle and looks exactly
    // like a pipeline stall. Measure with the game in front, or not at all.
    bool launchDlss5 = false;

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
    void fillGameFoldersFromSteam();
    QString gameFolder(const QString &gameIdName) const;
    static AppSettings loadForStartup();
};
