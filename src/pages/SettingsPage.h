#pragma once
#include "AppSettings.h"
#include <QWidget>

class QLineEdit;
class QPushButton;
class QLabel;
class QComboBox;
class QCheckBox;

class SettingsPage : public QWidget
{
    Q_OBJECT
public:
    explicit SettingsPage(AppSettings &settings, QWidget *parent = nullptr);
    void pullFromSettings();
    void beginEdit();
    void saveEdit();
    void cancelEdit();
    void retranslate();

signals:
    void plutoniumFolderChanged();
    void homeEnabledChanged(bool enabled);
    void cancelled();
    void checkUpdatesRequested();

private:
    QLineEdit *folderRow(class QVBoxLayout *layout, const QString &label,
                          QString AppSettings::*field);
    void updateKitButton();

    AppSettings &m_settings;
    QLineEdit *m_username = nullptr;
    QLineEdit *m_plutonium = nullptr;
    QPushButton *m_kitBtn = nullptr;
    QLabel *m_kitHint = nullptr;
    QComboBox *m_langCombo = nullptr;
    QComboBox *m_themeCombo = nullptr;
    QCheckBox *m_homeEnabled = nullptr;
    QCheckBox *m_checkUpdates = nullptr;
    QPushButton *m_checkUpdatesBtn = nullptr;
    QPushButton *m_saveBtn = nullptr;
    QPushButton *m_cancelBtn = nullptr;
    AppSettings m_snapshot;
};
