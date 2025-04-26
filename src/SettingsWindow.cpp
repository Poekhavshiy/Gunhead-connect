#include "SettingsWindow.h"
#include "LanguageSelect.h"
#include "ThemeSelect.h"
#include "Transmitter.h"
#include "checkversion.h"
#include "MainWindow.h"
#include <QMessageBox>
#include <QDebug>
#include <QPushButton>
#include <QVBoxLayout>
#include <QDirIterator>
#include <QIcon>

SettingsWindow::SettingsWindow(QWidget* parent)
    : QMainWindow(parent), transmitter(this), themeSelectWindow(nullptr), languageSelectWindow(nullptr), signalMapper(new QSignalMapper(this)) {
    // === Load persisted theme ===
    QSettings settings("KillApiConnect", "KillApiConnectPlus");
    QSize settingsWindowSize = settings.value("settingsWindowPreferredSize", QSize(500, 600)).toSize();
    setFixedSize(settingsWindowSize);

    // Initialize the list of available sounds
    availableSounds = getAvailableSounds();

    // Load the last selected sound from QSettings
    QString lastSelectedSound = settings.value("LogDisplay/SoundFile", "").toString();
    currentSoundIndex = availableSounds.indexOf(lastSelectedSound);
    if (currentSoundIndex == -1) {
        currentSoundIndex = 0; // Default to the first sound if the saved sound is not found
    }

    setupUI();
    loadSettings();

    // Update the sound edit box with the current sound
    if (!availableSounds.isEmpty()) {
        updateSelectedSound(availableSounds[currentSoundIndex]);
    }
}

void SettingsWindow::init() {
    loadSettings();
}

void SettingsWindow::setupUI() {
    setWindowTitle("Settings");
    setWindowIcon(QIcon()); // Clear the window icon

    QVBoxLayout* mainLayout = new QVBoxLayout();
    QWidget* container = new QWidget(this);
    container->setLayout(mainLayout);
    setCentralWidget(container);

    // Check for Updates
    QLabel* updateLabel = new QLabel("Check for Updates", this);
    updateCheckbox = new QCheckBox("Check for new versions on startup", this);
    connect(updateCheckbox, &QCheckBox::checkStateChanged, this, &SettingsWindow::toggleUpdateCheck);
    QPushButton* checkUpdatesButton = new QPushButton("Check for updates now", this);
    connect(checkUpdatesButton, &QPushButton::clicked, this, &SettingsWindow::checkForUpdates);

    QVBoxLayout* updateLayout = new QVBoxLayout();
    updateLayout->addWidget(updateLabel);
    updateLayout->addWidget(updateCheckbox);
    updateLayout->addWidget(checkUpdatesButton);

    QWidget* updateCard = new QWidget(this);
    updateCard->setLayout(updateLayout);
    mainLayout->addWidget(updateCard);

    // Game Files Path
    QLabel* pathLabel = new QLabel("Game LIVE Folder Path", this);
    pathEdit = new QLineEdit(this);
    QPushButton* browseButton = new QPushButton("Browse", this);
    connect(browseButton, &QPushButton::clicked, this, &SettingsWindow::updatePath);
    QPushButton* savePathButton = new QPushButton("Save", this);
    connect(savePathButton, &QPushButton::clicked, this, &SettingsWindow::savePath);

    QHBoxLayout* pathButtonsLayout = new QHBoxLayout();
    pathButtonsLayout->addWidget(browseButton);
    pathButtonsLayout->addWidget(savePathButton);

    QVBoxLayout* pathLayout = new QVBoxLayout();
    pathLayout->addWidget(pathLabel);
    pathLayout->addWidget(pathEdit);
    pathLayout->addLayout(pathButtonsLayout);

    QWidget* pathCard = new QWidget(this);
    pathCard->setLayout(pathLayout);
    mainLayout->addWidget(pathCard);

    // KillAPI Key
    QLabel* apiLabel = new QLabel("KillAPI Key", this);
    apiEdit = new QLineEdit(this);
    apiEdit->setEchoMode(QLineEdit::Password);
    QPushButton* saveApiKeyButton = new QPushButton("Save API Key", this);
    connect(saveApiKeyButton, &QPushButton::clicked, this, &SettingsWindow::saveApiKey);

    QVBoxLayout* apiLayout = new QVBoxLayout();
    apiLayout->addWidget(apiLabel);
    apiLayout->addWidget(apiEdit);
    apiLayout->addWidget(saveApiKeyButton);

    QWidget* apiCard = new QWidget(this);
    apiCard->setLayout(apiLayout);
    mainLayout->addWidget(apiCard);

    // Sound Selector
    QLabel* soundLabel = new QLabel("Select Notification Sound", this);
    soundEdit = new QLineEdit(this);
    soundEdit->setReadOnly(true);

    QPushButton* leftButton = new QPushButton("<", this);
    QPushButton* rightButton = new QPushButton(">", this);

    connect(leftButton, &QPushButton::clicked, this, &SettingsWindow::selectPreviousSound);
    connect(rightButton, &QPushButton::clicked, this, &SettingsWindow::selectNextSound);

    QHBoxLayout* soundSelectorLayout = new QHBoxLayout();
    soundSelectorLayout->addWidget(leftButton);
    soundSelectorLayout->addWidget(soundEdit);
    soundSelectorLayout->addWidget(rightButton);

    QVBoxLayout* soundLayout = new QVBoxLayout();
    soundLayout->addWidget(soundLabel);
    soundLayout->addLayout(soundSelectorLayout);

    QWidget* soundCard = new QWidget(this);
    soundCard->setLayout(soundLayout);
    mainLayout->addWidget(soundCard);

    // Language and Theme Selector Box
    QVBoxLayout* selectorLayout = new QVBoxLayout();
    QPushButton* languageButton = new QPushButton("Change Language", this);
    connect(languageButton, &QPushButton::clicked, this, &SettingsWindow::toggleLanguageSelectWindow);
    selectorLayout->addWidget(languageButton);

    QPushButton* themeButton = new QPushButton("Change Theme", this);
    connect(themeButton, &QPushButton::clicked, this, &SettingsWindow::toggleThemeSelectWindow);
    selectorLayout->addWidget(themeButton);

    QWidget* selectorCard = new QWidget(this);
    selectorCard->setLayout(selectorLayout);
    mainLayout->addWidget(selectorCard);
}

void SettingsWindow::loadSettings() {
    QSettings settings("KillApiConnect", "KillApiConnectPlus");

    gameFolder = settings.value("gameFolder", "C:/Program Files/Roberts Space Industries/StarCitizen/LIVE").toString();
    apiKey = settings.value("apiKey", "").toString();
    checkUpdatesOnStartup = settings.value("checkUpdatesOnStartup", false).toBool();

    pathEdit->setText(gameFolder);
    apiEdit->setText(apiKey);
    updateCheckbox->setChecked(checkUpdatesOnStartup);
}

void SettingsWindow::saveSettings() {
    QSettings settings("KillApiConnect", "KillApiConnectPlus");

    settings.setValue("gameFolder", gameFolder);
    settings.setValue("apiKey", apiKey);
    settings.setValue("checkUpdatesOnStartup", checkUpdatesOnStartup);

    qDebug() << "Settings saved:";
    qDebug() << "  Game Folder:" << gameFolder;
    qDebug() << "  API Key:" << apiKey;
    qDebug() << "  Check Updates on Startup:" << checkUpdatesOnStartup;
}

QString SettingsWindow::getGameFolder() const {
    return gameFolder;
}

QString SettingsWindow::getApiKey() const {
    return apiKey;
}

bool SettingsWindow::getCheckUpdatesOnStartup() const {
    return checkUpdatesOnStartup;
}

void SettingsWindow::toggleUpdateCheck(int state) {
    checkUpdatesOnStartup = (state == Qt::Checked);
    saveSettings();
}

void SettingsWindow::updatePath() {
    QString folder = QFileDialog::getExistingDirectory(this, "Select Game Folder", gameFolder);
    if (!folder.isEmpty()) {
        gameFolder = folder;
        pathEdit->setText(folder);
        saveSettings();
    }
}

void SettingsWindow::savePath() {
    gameFolder = pathEdit->text().trimmed();
    saveSettings();
}

void SettingsWindow::saveApiKey() {
    apiKey = apiEdit->text().trimmed();

    // Save the API key and other settings
    saveSettings();

    // Attempt to connect to KillAPI
    if (!apiKey.isEmpty()) {
        bool success = transmitter.sendConnectionSuccess(gameFolder, apiKey);
        if (success) {
            QMessageBox::information(this, "Success", "KillAPI connected successfully!");
        } else {
            QMessageBox::warning(this, "Error", "Failed to connect to KillAPI. Please check your API key.");
            apiKey.clear(); // Clear the API key on failure
            saveSettings(); // Save the cleared API key
        }
    } else {
        QMessageBox::information(this, "API Key Saved", "Your API key has been saved.");
    }
}

void SettingsWindow::checkForUpdates() {
    CheckVersion versionChecker;
    QString jsonFilePath = "data/logfile_regex_rules.json";
    QString currentAppVersion = "0.1.1";
    const QUrl jsonDownloadUrl("https://gunhead.sparked.network/static/data/logfile_regex_rules.json");
    const QUrl releaseDownloadUrl("https://github.com/Poekhavshiy/KillAPI-connect.plus/releases/latest/download/KillAPi.connect.plus.exe");
    
    
    CheckVersion::UpdateTriState appUpdateState = versionChecker.isAppUpdateAvailable(currentAppVersion, 5000);
    CheckVersion::UpdateTriState jsonUpdateState = versionChecker.isJsonUpdateAvailable(jsonFilePath, 5000);

    if (appUpdateState == CheckVersion::UpdateTriState::Yes || jsonUpdateState == CheckVersion::UpdateTriState::Yes) {
        qDebug() << "SettingsWindow: Updates available, prompting user.";
        QMessageBox::StandardButton reply = QMessageBox::question(
            this, "Updates Available",
            "Updates are available. Would you like to download them?",
            QMessageBox::Yes | QMessageBox::No);
        if (reply == QMessageBox::Yes) {
            if (appUpdateState == CheckVersion::UpdateTriState::Yes) {
                qDebug() << "SettingsWindow: User asked to update the App.";
                if (versionChecker.downloadFile(releaseDownloadUrl, "KillAPi.connect.exe", 5000)) {
                    qDebug() << "SettingsWindow: App updated successfully.";
                    QMessageBox::information(this, "Update Successful", "App updated successfully.");
                } else {
                    qDebug() << "SettingsWindow: App update failed.";
                    QMessageBox::warning(this, "Update Failed", "Failed to update app.");
                }
                qDebug() << "Downloading latest app version.";
                // Add logic to download and install the app
            }

            if (jsonUpdateState == CheckVersion::UpdateTriState::Yes) {
                qDebug() << "SettingsWindow: User asked to update the JSON file.";
                if (versionChecker.downloadFile(jsonDownloadUrl, jsonFilePath, 5000)) {
                    qDebug() << "SettingsWindow: JSON file updated successfully.";
                    QMessageBox::information(this, "Update Successful", "JSON file updated successfully.");
                } else {
                    qDebug() << "SettingsWindow: JSON file update failed.";
                    QMessageBox::warning(this, "Update Failed", "Failed to update JSON file.");
                }
                qDebug() << "Downloading latest JSON version.";
            }
        }
    } else if (appUpdateState == CheckVersion::UpdateTriState::No && jsonUpdateState == CheckVersion::UpdateTriState::No) {
        qDebug() << "SettingsWindow: No updates available.";
        QMessageBox::information(this, "No Updates", "Your application and JSON file are up-to-date.");
    } else {
        qDebug() << "SettingsWindow: Error occurred while checking for updates.";
        QMessageBox::warning(this, "Error", "An error occurred while checking for updates.\n Please check your internet connection and try again.");
    }
}

void SettingsWindow::toggleThemeSelectWindow() {
    if (themeSelectWindow) {
        themeSelectWindow->close();
        themeSelectWindow = nullptr;
    } else {
        themeSelectWindow = new ThemeSelectWindow(this);
        themeSelectWindow->setAttribute(Qt::WA_DeleteOnClose);

        // Connect ThemeSelectWindow's signal to SettingsWindow
        connect(themeSelectWindow, &ThemeSelectWindow::themeChanged, 
                this, &SettingsWindow::onThemeChanged);

        // Clean up the pointer when the window is closed
        connect(themeSelectWindow, &ThemeSelectWindow::destroyed, this, [this]() {
            themeSelectWindow = nullptr;
        });

        themeSelectWindow->show();
    }
}

void SettingsWindow::toggleLanguageSelectWindow() {
    if (languageSelectWindow) {
        languageSelectWindow->close();
        languageSelectWindow = nullptr;
    } else {
        languageSelectWindow = new LanguageSelectWindow(this);
        languageSelectWindow->setAttribute(Qt::WA_DeleteOnClose);

        // Connect settingsChanged to LanguageSelectWindow's onThemeChanged slot
        connect(this, &SettingsWindow::settingsChanged, languageSelectWindow, &LanguageSelectWindow::onThemeChanged);

        languageSelectWindow->show();
    }
}

void SettingsWindow::onThemeChanged(const Theme& theme) {
    qDebug() << "Settings Window: === Settings Window Preferred Size:" << theme.settingsWindowPreferredSize;
    setFixedSize(theme.settingsWindowPreferredSize); // Resize SettingsWindow

    // Re-emit the themeChanged signal as settingsChanged
    emit settingsChanged(theme);
}

void SettingsWindow::selectPreviousSound() {
    if (availableSounds.isEmpty()) {
        return; // No sounds available
    }

    // Decrement the index and wrap around if necessary
    currentSoundIndex = (currentSoundIndex - 1 + availableSounds.size()) % availableSounds.size();
    updateSelectedSound(availableSounds[currentSoundIndex]);
}

void SettingsWindow::selectNextSound() {
    if (availableSounds.isEmpty()) {
        return; // No sounds available
    }

    // Increment the index and wrap around if necessary
    currentSoundIndex = (currentSoundIndex + 1) % availableSounds.size();
    updateSelectedSound(availableSounds[currentSoundIndex]);
}

QStringList SettingsWindow::getAvailableSounds() const {
    QStringList sounds;

    // Add sounds from the qrc resources
    QDirIterator it(":/", QDirIterator::Subdirectories);
    while (it.hasNext()) {
        QString path = it.next();
        if (path.startsWith(":/sounds/") && path.endsWith(".mp3")) {
            sounds << path;
            qDebug() << "Found sound:" << path;
        }
    }

    // Add sounds from the application root "sounds/" folder
    QDir soundFilesystemDir(QCoreApplication::applicationDirPath() + "/sounds");
    if (soundFilesystemDir.exists()) {
        QStringList files = soundFilesystemDir.entryList(QStringList() << "*.mp3", QDir::Files);
        for (const QString& file : files) {
            sounds << soundFilesystemDir.absoluteFilePath(file);
            qDebug() << "Found sound in filesystem:" << soundFilesystemDir.absoluteFilePath(file);
        }
    }

    return sounds;
}

void SettingsWindow::updateSelectedSound(const QString& soundFile) {
    soundEdit->setText(QFileInfo(soundFile).fileName());

    qDebug() << "Selected sound file:" << QFileInfo(soundFile).fileName() << "at path:" << soundFile;

    // Save the selected sound to QSettings
    QSettings settings("KillApiConnect", "KillApiConnectPlus");
    settings.setValue("LogDisplay/SoundFile", soundFile);
}