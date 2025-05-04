#ifndef SETTINGSWINDOW_H
#define SETTINGSWINDOW_H

#include <QMainWindow>
#include <QVBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QCheckBox>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QSettings>
#include <QPointer>
#include <QSignalMapper>
#include <QVariant>
#include "Transmitter.h"
#include "ThemeSelect.h"
#include "LanguageSelect.h"

class SettingsWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit SettingsWindow(QWidget* parent = nullptr);

    // Public methods
    void init(); // Load settings
    QString getGameFolder() const;
    QString getApiKey() const;
    bool getCheckUpdatesOnStartup() const;
    void checkForUpdates(); // Declare the function here

signals:
    void gameFolderChanged(const QString& newFolder);
    void settingsChanged(Theme themeData); // Signal to notify when settings change

private:
    // Settings fields
    QString gameFolder;
    QString apiKey;
    bool checkUpdatesOnStartup;

    // UI elements
    QLineEdit* pathEdit;
    QLineEdit* apiEdit;
    QLineEdit* soundEdit;
    QCheckBox* updateCheckbox;
    QSignalMapper* signalMapper;

    // Transmitter instance
    Transmitter transmitter;

    // Additional windows
    QPointer<ThemeSelectWindow> themeSelectWindow;
    QPointer<LanguageSelectWindow> languageSelectWindow;

    // Sound management
    QStringList availableSounds; // List of available sounds
    int currentSoundIndex = 0;   // Index of the currently selected sound

    // Private methods
    void setupUI();
    void loadSettings();
    void saveSettings();
    void onThemeChanged(const Theme& theme);
    void selectPreviousSound();
    void selectNextSound();
    QStringList getAvailableSounds() const;
    void updateSelectedSound(const QString& sound);

private slots:
    void toggleUpdateCheck(int state);
    void updatePath();
    void savePath();
    void saveApiKey();
    void toggleLanguageSelectWindow();
    void toggleThemeSelectWindow();
    bool isGameFolderValid(const QString& folder) const;
};

#endif