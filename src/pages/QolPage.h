#pragma once
#include <QWidget>
#include <QList>
#include <functional>
#include "../QolService.h"

class QLabel;
class QPushButton;
class QVBoxLayout;
class QFrame;
class AppSettings;

// The Quality of Life series page: one row per game's mod (install, update,
// remove), the Black Ops II extras (HD textures, custom sounds, controller
// icons), ReShade (install, start the watchdog) and backups of the player's
// own files. This is the part the upstream launcher does not have.
class QolPage : public QWidget
{
    Q_OBJECT
public:
    explicit QolPage(AppSettings &settings, QWidget *parent = nullptr);
    void refresh();
    void retranslate();
    // Install or update a series mod by game code (t6...). Returns false if unknown.
    bool installSeriesMod(const QString &gameCode);

signals:
    void installedChanged();

private:
    struct Row {
        QFrame *card = nullptr;
        QLabel *status = nullptr;
        QPushButton *primary = nullptr;
        QPushButton *secondary = nullptr;
        QPushButton *tertiary = nullptr;   // backup rows only
    };
    Row addRow(QVBoxLayout *into, const QString &kicker, const QString &title, const QString &desc);
    void runJob(const QString &title, const std::function<bool(QString &, const std::function<void(const QString &, int)> &)> &job,
                const std::function<void()> &after = {},
                const std::function<void(bool)> &finished = {});
    bool confirm(const QString &title, const QString &text);
    void openUrl(const QString &url);
    // Remove a pack, then offer the player's own files back when a backup of
    // them exists - the "remove and put my originals back" row of the script.
    void removeWithRestore(const QString &title, const QString &question, const QString &backupKind,
                           const std::function<bool(QString &)> &remove);
    void backupMenu(const QString &kind);
    void refreshBackups();

    AppSettings &m_settings;
    QLabel *m_intro = nullptr;
    QLabel *m_plutoHint = nullptr;
    QList<Row> m_modRows;     // parallel to QolService::series()
    Row m_textures, m_sounds, m_controller, m_reshade, m_dlss;
    Row m_backupAll;
    QList<Row> m_backupRows;  // parallel to QolBackups::kinds()
    QPushButton *m_backupOpen = nullptr;
    class QCheckBox *m_backupAuto = nullptr;
    QStringList m_latest;     // cached latest tags, parallel to series()
    QolService::DlssRelease m_dlssRelease;
    bool m_dlssCheckStarted = false;
};
