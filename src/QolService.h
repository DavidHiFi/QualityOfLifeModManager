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

    // ---------------------------------------------------------------------
    //  Two ReShade builds, one bin folder.
    //
    //  Online play gets the stock build. Ask it for an add-on and it answers,
    //  in its own words, "this build of ReShade has only limited add-on
    //  functionality" - which is the whole of what people mean when they say
    //  ReShade works online "but limited".
    //
    //  LAN play is not signed in to anything, so it gets the add-on build,
    //  and with it DLSS 5: the DLSS5-Feeder add-on manufactures a DLSS
    //  contract for a game that has none, and a 64-bit helper in host64\ runs
    //  the neural pass for this 32-bit engine.
    //
    //  Which build is in bin is decided at launch, by the button that was
    //  pressed. applyReShadeMode is the only thing that writes it.
    // ---------------------------------------------------------------------
    enum class ReShadeMode { Online, Lan };
    struct DlssRelease {
        QString version;
        QString url;
        QString sha256;
        qint64 size = 0;
        bool isValid() const { return !version.isEmpty() && !url.isEmpty() && sha256.size() == 64 && size > 0; }
    };
    ReShadeMode activeReShadeMode(const AppSettings &s);
    // True when the add-on build is the one currently in bin.
    bool addonReShadeActive(const AppSettings &s);
    // The DLSS 5 payload is published under its own release tag
    // (QOL_DLSS_RELEASE_URL), never shipped in the exe. Install downloads it,
    // checks the archive and every file in it, fetches LumeniteFX from its
    // author, and swaps it in whole; nothing half-installed is ever used.
    bool dlssPayloadReady(const AppSettings &s);
    QString dlssPayloadSummary(const AppSettings &s);
    QString installedDlssVersion(const AppSettings &s);   // "" for none, or a pre-2.3 import
    bool latestDlssRelease(DlssRelease &release, QString &error);
    bool installDlssPayload(const AppSettings &s, const DlssRelease &release, const Progress &p, QString &error);
    bool removeDlssPayload(const AppSettings &s, QString &error);
    // True only when bin holds nothing an online session must not carry: the
    // add-on runtime, the feeder, any add-on, the helper or the DLSS preset.
    bool onlineReShadeSafe(const AppSettings &s, QString &error);
    // Puts the right build, add-ons, shader and preset entries in bin. `dlss`
    // asks for the DLSS 5 payload on top, and is ignored unless mode is Lan.
    bool applyReShadeMode(const AppSettings &s, ReShadeMode mode, bool dlss, QString &error);
    // After a LAN session ends: bin goes back to its online state, so what is
    // left on disk between sessions is never the add-on build. A no-op when
    // the last session was not LAN.
    bool leaveLanState(const AppSettings &s, QString &error);
    // ReShade must not be there at all: what an unticked box asks for.
    bool ensureReShadeAbsent(const AppSettings &s, QString &error);

    // Paths
    QString t6Storage(const AppSettings &s);          // <pluto>/storage/t6
    QString stateDir(const AppSettings &s);           // <pluto>/storage/t6/_zm_qol_installer
    QString reShadeVault(const AppSettings &s);       // stateDir/reshade-vault
    QString reShadeLanVault(const AppSettings &s);    // stateDir/reshade-lan
    QString modsDir(const AppSettings &s, const QString &gameCode);
}
