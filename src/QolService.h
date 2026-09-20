#pragma once
#include <QString>
#include <QStringList>
#include <QList>
#include <functional>

class AppSettings;

// The Quality of Life series: what each game's mod is, whether it is installed,
// and the extra packs the Black Ops II mod ships (HD textures, custom sounds,
// controller icons) plus ReShade. Everything here is synchronous; callers run
// installs on QtConcurrent with a ProgressDialog, the same as PlayPage does.
namespace QolService
{
    struct SeriesMod {
        QString gameCode;   // t4 t5 t6 t7
        QString gameTitle;  // Black Ops II
        QString repo;       // DavidHiFi/T6-QoL
        QString folder;     // zm_qol (mods folder name), empty when not released
        bool released = false;
    };
    // Name of a running Plutonium game process, or empty. Installs refuse while one runs.
    QString runningGame();

    QList<SeriesMod> series();
    SeriesMod seriesFor(const QString &gameCode);

    // "v2.16.14" from storage/<game>/mods/<folder>/mod.json, or empty.
    QString installedModVersion(const AppSettings &s, const SeriesMod &m);
    // Latest release tag of the mod's repo (network). Empty on failure.
    QString latestModVersion(const SeriesMod &m, QString &error);

    using Progress = std::function<void(const QString &text, int percent)>;

    // Downloads the newest release zip whose name does not look like a texture,
    // sound, controller or portable pack, and copies mod.ff/iwd/json/sabl/sabs
    // into storage/t6/mods/<folder>. Never touches player settings.
    bool installMod(const AppSettings &s, const SeriesMod &m, const Progress &p, QString &error);

    // Packs shipped by the T6 mod's releases (found by keyword in the asset name).
    enum class Pack { Textures, Sounds };
    bool packInstalled(const AppSettings &s, Pack pack);
    bool installPack(const AppSettings &s, Pack pack, const Progress &p, QString &error);
    bool removePack(const AppSettings &s, Pack pack, QString &error);

    // Controller icon packs: "ps5", "switch", "xbox".
    QString installedController(const AppSettings &s); // "" when game defaults
    bool installController(const AppSettings &s, const QString &pack, const Progress &p, QString &error);
    bool removeController(const AppSettings &s, QString &error);

    // ReShade: dxgi.dll + presets into <pluto>\bin, mirrored to the vault the
    // watchdog restores from. Backs up whatever was in bin first.
    bool reShadeInstalled(const AppSettings &s);
    bool installReShade(const AppSettings &s, QString &error);
    bool removeReShade(const AppSettings &s, QString &error);

    // Paths
    QString t6Storage(const AppSettings &s);          // <pluto>/storage/t6
    QString stateDir(const AppSettings &s);           // <pluto>/storage/t6/_zm_qol_installer
    QString reShadeVault(const AppSettings &s);       // stateDir/reshade-vault
    QString modsDir(const AppSettings &s, const QString &gameCode);
}
