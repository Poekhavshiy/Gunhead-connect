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
#include <QTabWidget>
#include <QTimer>
#include "Transmitter.h"
#include "ThemeSelect.h"
#include "ThemeManager.h"
#include "LanguageSelect.h"
#include "SoundPlayer.h"
#include "language_manager.h" // Include language manager header

class SettingsWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit SettingsWindow(QWidget* parent = nullptr);    // Public methods
    void init(); // Load settings
    QString getGameFolder() const;
    QString getLauncherPath() const;  // Add launcher path getter
    QString getApiKey() const;
    bool getCheckUpdatesOnStartup() const;
    bool getAutoLaunchGame() const;  // Add getter for auto-launch game setting
    bool getMinimizeToTray() const;  // Add getter for minimize to tray setting
    bool getStartMinimized() const;  // Add this getter
    bool getStartMonitoringOnLaunch() const; // Add this getter
    void checkForUpdates(); // Declare the function here
    void retranslateUi(); // Method to retranslate UI elements
    bool eventFilter(QObject* watched, QEvent* event) override;
    void activateDebugMode();

signals:
    void gameFolderChanged(const QString& newFolder);
    void settingsChanged(Theme themeData); // Signal to notify when settings change
    void minimizeToTrayChanged(bool enabled); // Signal when minimize to tray setting changes
    void debugModeChanged(bool enabled); // Add debug mode changed signal

private:    // Settings fields
    QString gameFolder;
    QString launcherPath;  // Add launcher path setting
    QString apiKey;
    bool checkUpdatesOnStartup;
    bool autoLaunchGame;
    bool minimizeToTray;  // Add minimize to tray setting
    bool startMinimized;  // Add this with the other boolean settings
    bool startMonitoringOnLaunch; // Add this to the private settings fields

    // UI elements
    QTabWidget* tabWidget;
    QLineEdit* pathEdit;
    QLineEdit* launcherPathEdit;  // Add launcher path input
    QLineEdit* apiEdit;
    QLineEdit* soundEdit;
    QCheckBox* updateCheckbox;
    QCheckBox* autoLaunchGameCheckbox;  // Add auto-launch game checkbox
    QCheckBox* minimizeToTrayCheckbox;  // Add minimize to tray checkbox
    QCheckBox* startMinimizedCheckbox;  // Add this with the other checkboxes
    QCheckBox* startMonitoringOnLaunchCheckbox; // Add this to the UI elements section
    QSignalMapper* signalMapper;

    // Transmitter instance
    Transmitter transmitter;    // Additional windows
    QPointer<ThemeSelectWindow> themeSelectWindow;
    QPointer<LanguageSelectWindow> languageSelectWindow;

    // Sound management
    QStringList availableSounds; // List of available sounds
    int currentSoundIndex = 0;   // Index of the currently selected sound

    // Private methods
    void setupUI();
    void setupGeneralTab(QTabWidget* tabWidget);
    void setupGameTab(QTabWidget* tabWidget);
    void setupSoundsLanguageThemesTab(QTabWidget* tabWidget);
    void loadSettings();
    void saveSettings();
    void onThemeChanged(const Theme& theme);    void selectPreviousSound();
    void selectNextSound();
    void toggleLauncherControlsVisibility(bool visible);  // Add helper method
    QStringList getAvailableSounds() const;
    void updateSelectedSound(const QString& sound);
    
    QPushButton* checkUpdatesButton; // Declare checkUpdatesButton
    QPushButton* saveApiKeyButton;

    // Sound player
    SoundPlayer* soundPlayer;

    // Debug mode support
    int debugClickCount;
    QTimer* debugClickTimer;

private slots:
    void toggleUpdateCheck(int state);
    void toggleMinimizeToTray(int state);  // Add slot for minimize to tray toggle
    void toggleStartMinimized(int state);  // Add this toggle method
    void toggleStartMonitoringOnLaunch(int state); // Add this to the private slots
    void updatePath();
    void updateLauncherPath();  // Add launcher path update slot
    void savePath();
    void saveApiKey();
    void toggleLanguageSelectWindow();
    void toggleThemeSelectWindow();
    void toggleAutoLaunchGame(int state);  // Add slot for auto-launch game toggle
    bool isGameFolderValid(const QString& folder) const;
};

#endif