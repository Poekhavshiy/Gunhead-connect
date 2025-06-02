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
#include <QFileDialog>
#include <QProcess>

SettingsWindow::SettingsWindow(QWidget* parent)
    : QMainWindow(parent), transmitter(this), themeSelectWindow(nullptr), languageSelectWindow(nullptr), signalMapper(new QSignalMapper(this)) 
{
    soundPlayer = new SoundPlayer(this);
    
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
    setWindowTitle(tr("Settings"));
    setWindowIcon(QIcon()); // Clear the window icon

    QVBoxLayout* mainLayout = new QVBoxLayout();
    QWidget* container = new QWidget(this);
    container->setLayout(mainLayout);
    setCentralWidget(container);

    // --- Create a horizontal layout for version and update label ---
    QString versionString = tr("Version: ") + QCoreApplication::applicationVersion();
    QLabel* versionLabel = new QLabel(versionString, this);
    QFont tinyFont = versionLabel->font();
    tinyFont.setPointSizeF(tinyFont.pointSizeF() * 0.7); // Make it smaller
    versionLabel->setFont(tinyFont);
    versionLabel->setStyleSheet("color: gray;");

    QLabel* updateLabel = new QLabel(tr("Check for Updates"), this);
    updateLabel->setContentsMargins(0, 0, 0, 0);

    QHBoxLayout* versionUpdateLayout = new QHBoxLayout();
    versionUpdateLayout->setContentsMargins(0, 0, 0, 0);
    versionUpdateLayout->setSpacing(8); // Small spacing between version and update label
    versionUpdateLayout->addWidget(updateLabel);
    versionUpdateLayout->addStretch();
    versionUpdateLayout->addWidget(versionLabel);

    mainLayout->addLayout(versionUpdateLayout);

    // Update checkbox and button below the update label
    updateCheckbox = new QCheckBox(tr("Check for new versions on startup"), this);
    connect(updateCheckbox, &QCheckBox::checkStateChanged, this, &SettingsWindow::toggleUpdateCheck);
    checkUpdatesButton = new QPushButton(tr("Check for updates now"), this);
    connect(checkUpdatesButton, &QPushButton::clicked, this, &SettingsWindow::checkForUpdates);

    QVBoxLayout* updateLayout = new QVBoxLayout();
    updateLayout->setContentsMargins(0, 0, 0, 0);
    updateLayout->setSpacing(2); // Minimal spacing
    updateLayout->addWidget(updateCheckbox);
    updateLayout->addWidget(checkUpdatesButton);

    QWidget* updateCard = new QWidget(this);
    updateCard->setLayout(updateLayout);
    mainLayout->addWidget(updateCard);

    // Game Files Path
    QLabel* pathLabel = new QLabel(tr("Game LIVE Folder Path"), this);
    pathEdit = new QLineEdit(this);
    QPushButton* browseButton = new QPushButton(tr("Browse"), this);
    connect(browseButton, &QPushButton::clicked, this, &SettingsWindow::updatePath);
    QPushButton* savePathButton = new QPushButton(tr("Save"), this);
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
    QLabel* apiLabel = new QLabel(tr("KillAPI Key"), this);
    apiEdit = new QLineEdit(this);
    apiEdit->setEchoMode(QLineEdit::Password);
    saveApiKeyButton = new QPushButton(tr("Save API Key"), this);
    connect(saveApiKeyButton, &QPushButton::clicked, this, &SettingsWindow::saveApiKey);

    QVBoxLayout* apiLayout = new QVBoxLayout();
    apiLayout->addWidget(apiLabel);
    apiLayout->addWidget(apiEdit);
    apiLayout->addWidget(saveApiKeyButton);

    QWidget* apiCard = new QWidget(this);
    apiCard->setLayout(apiLayout);
    mainLayout->addWidget(apiCard);

    // Sound Selector
    QLabel* soundLabel = new QLabel(tr("Select Notification Sound"), this);
    soundEdit = new QLineEdit(this);
    soundEdit->setReadOnly(true);

    QPushButton* leftButton = new QPushButton("<", this);
    QPushButton* rightButton = new QPushButton(">", this);
    QPushButton* testSoundButton = new QPushButton(tr("Test"), this);

    connect(leftButton, &QPushButton::clicked, this, &SettingsWindow::selectPreviousSound);
    connect(rightButton, &QPushButton::clicked, this, &SettingsWindow::selectNextSound);
    connect(testSoundButton, &QPushButton::clicked, this, [this]() {
        QSettings settings("KillApiConnect", "KillApiConnectPlus");
        QString soundFile = settings.value("LogDisplay/SoundFile", ":/sounds/beep-beep.mp3").toString();
        soundPlayer->playSimpleSound(soundFile);
    });

    QHBoxLayout* soundSelectorLayout = new QHBoxLayout();
    soundSelectorLayout->addWidget(leftButton);
    soundSelectorLayout->addWidget(soundEdit);
    soundSelectorLayout->addWidget(rightButton);
    soundSelectorLayout->addWidget(testSoundButton);

    QVBoxLayout* soundLayout = new QVBoxLayout();
    soundLayout->addWidget(soundLabel);
    soundLayout->addLayout(soundSelectorLayout);

    QWidget* soundCard = new QWidget(this);
    soundCard->setLayout(soundLayout);
    mainLayout->addWidget(soundCard);

    // Language and Theme Selector Box
    QVBoxLayout* selectorLayout = new QVBoxLayout();
    QPushButton* languageButton = new QPushButton(tr("Change Language"), this);
    connect(languageButton, &QPushButton::clicked, this, &SettingsWindow::toggleLanguageSelectWindow);
    selectorLayout->addWidget(languageButton);

    QPushButton* themeButton = new QPushButton(tr("Change Theme"), this);
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
    QString folder = QFileDialog::getExistingDirectory(this, tr("Select Game Folder"), gameFolder);
    if (!folder.isEmpty()) {
        if (!isGameFolderValid(folder)) {
            QMessageBox::warning(this, tr("Invalid Folder"), 
                tr("Selected folder does not contain Star Citizen launcher."));
            return;
        }
        
        // Only update if the path actually changed
        if (gameFolder != folder) {
            gameFolder = folder;
            pathEdit->setText(folder);
            saveSettings();
            emit gameFolderChanged(folder); // Notify MainWindow and LogDisplayWindow
        }
    }
}

void SettingsWindow::savePath() {
    QString folder = pathEdit->text().trimmed();
    if (!isGameFolderValid(folder)) {
        QMessageBox::warning(this, tr("Invalid Folder"), 
            tr("Selected folder does not contain Star Citizen launcher."));
        return;
    }
    
    // Only update if the path actually changed
    if (gameFolder != folder) {
        gameFolder = folder;
        saveSettings();
        emit gameFolderChanged(folder); // Notify MainWindow and LogDisplayWindow
    }
}

void SettingsWindow::saveApiKey() {
    saveApiKeyButton->setEnabled(false); // Disable button
    QApplication::setOverrideCursor(Qt::WaitCursor); // Set waiting cursor

    apiKey = apiEdit->text().trimmed();

    // Save the API key and other settings
    saveSettings();

    // Add explicit variables to track which error occurred
    bool panelClosedErrorOccurred = false;
    bool invalidKeyErrorOccurred = false;

    // Connect to error signals temporarily for this operation
    QMetaObject::Connection panelClosedConnection = connect(&transmitter, &Transmitter::panelClosedError, this, [this, &panelClosedErrorOccurred]() {
        panelClosedErrorOccurred = true;
        QMessageBox::warning(this, tr("Panel Closed Error"), 
            tr("The KillAPI panel for this server has been closed or doesn't exist.\n\n"
               "This means your API key may be valid, but the Discord panel associated with it "
               "has been closed by an administrator or does not exist anymore.\n\n"
               "Please reopen the KillAPI panel or contact your Discord server administrator if you believe this is an error."));
    });
    
    QMetaObject::Connection invalidKeyConnection = connect(&transmitter, &Transmitter::invalidApiKeyError, this, [this, &invalidKeyErrorOccurred]() {
        invalidKeyErrorOccurred = true;
        QMessageBox::warning(this, tr("Authentication Error"), 
            tr("Invalid API key. The server rejected your credentials.\n\n"
               "Please check your API key and try again. Make sure you copied the entire key "
               "without any extra spaces or characters."));
    });

    // Attempt to connect to KillAPI
    if (!apiKey.isEmpty()) {
        bool success = transmitter.sendConnectionSuccess(gameFolder, apiKey);
        if (success) {
            QMessageBox::information(this, tr("Success"), tr("KillAPI connected successfully!"));
        } 
        else if (!panelClosedErrorOccurred && !invalidKeyErrorOccurred) {
            // Only show a generic error if none of the specific errors were handled
            QMessageBox::warning(this, tr("Connection Error"),
                tr("Failed to connect to KillAPI. Please check your internet connection and try again."));
        }
    } else {
        QMessageBox::information(this, tr("API Key Saved"), tr("Your API key has been saved."));
    }

    // Disconnect temporary connections
    disconnect(panelClosedConnection);
    disconnect(invalidKeyConnection);

    QApplication::restoreOverrideCursor(); // Restore cursor
    saveApiKeyButton->setEnabled(true); // Re-enable button
}

void SettingsWindow::checkForUpdates() {
    checkUpdatesButton->setEnabled(false); // Disable button
    QApplication::setOverrideCursor(Qt::WaitCursor); // Set waiting cursor

    CheckVersion versionChecker;
    QString jsonFilePath = "data/logfile_regex_rules.json";
    QString currentAppVersion = QCoreApplication::applicationVersion();
    const QUrl jsonDownloadUrl("https://gunhead.sparked.network/static/data/logfile_regex_rules.json");
    const QUrl releaseDownloadUrl("https://github.com/Poekhavshiy/KillAPI-connect-plus/releases/latest/download/KillApiConnectPlusSetup.msi");

    CheckVersion::UpdateTriState appUpdateState = versionChecker.isAppUpdateAvailable(currentAppVersion, 5000);
    CheckVersion::UpdateTriState jsonUpdateState = versionChecker.isJsonUpdateAvailable(jsonFilePath, 5000);

    if (appUpdateState == CheckVersion::UpdateTriState::Yes || jsonUpdateState == CheckVersion::UpdateTriState::Yes) {
        qDebug() << "SettingsWindow: Updates available, prompting user.";
        QMessageBox::StandardButton reply = QMessageBox::question(
            this, tr("Updates Available"),
            tr("Updates are available. Would you like to download them?"),
            QMessageBox::Yes | QMessageBox::No);
        if (reply == QMessageBox::Yes) {
            if (appUpdateState == CheckVersion::UpdateTriState::Yes) {
                qDebug() << "SettingsWindow: User asked to update the App.";

                // Ask the user where to save the file
                QString savePath = QFileDialog::getSaveFileName(
                    this, 
                    tr("Save Installer"), 
                    QDir::homePath() + "/KillAPi.connect.msi", 
                    tr("Installer Files (*.msi)")
                );

                if (savePath.isEmpty()) {
                    qDebug() << "SettingsWindow: User canceled the save dialog.";
                    return;
                }

                // Download the file
                if (versionChecker.downloadFile(releaseDownloadUrl, savePath, 5000)) {
                    qDebug() << "SettingsWindow: App updated successfully.";
                    QMessageBox::information(this, tr("Update Successful"), tr("App updated successfully."));

                    // Prompt the user to run the installer
                    QMessageBox::StandardButton installReply = QMessageBox::question(
                        this, tr("Run Installer"),
                        tr("The installer has been downloaded. Would you like to run it now?"),
                        QMessageBox::Yes | QMessageBox::No);

                    if (installReply == QMessageBox::Yes) {
                        // Launch the installer
                        QProcess::startDetached("msiexec", {"/i", savePath});
                    }
                } else {
                    qDebug() << "SettingsWindow: App update failed.";
                    QMessageBox::warning(this, tr("Update Failed"), tr("Failed to update app."));
                }
qDebug() << "Downloading latest app version.";
                // Add logic to download and install the app
            }

            if (jsonUpdateState == CheckVersion::UpdateTriState::Yes) {
                qDebug() << "SettingsWindow: User asked to update the JSON file.";
                if (versionChecker.downloadFile(jsonDownloadUrl, jsonFilePath, 5000)) {
                    qDebug() << "SettingsWindow: JSON file updated successfully.";
                    QMessageBox::information(this, tr("Update Successful"), tr("JSON file updated successfully."));
                } else {
                    qDebug() << "SettingsWindow: JSON file update failed.";
                    QMessageBox::warning(this, tr("Update Failed"), tr("Failed to update JSON file."));
                }
qDebug() << "Downloading latest JSON version.";
            }
        }
    } else if (appUpdateState == CheckVersion::UpdateTriState::No && jsonUpdateState == CheckVersion::UpdateTriState::No) {
        qDebug() << "SettingsWindow: No updates available.";
        QMessageBox::information(this, tr("No Updates"), tr("Your application and JSON file are up-to-date."));
    } else {
        qDebug() << "SettingsWindow: Error occurred while checking for updates.";
        QMessageBox::warning(this, tr("Error"), tr("An error occurred while checking for updates.\n Please check your internet connection and try again."));
    }

    QApplication::restoreOverrideCursor(); // Restore cursor
    checkUpdatesButton->setEnabled(true); // Re-enable button
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

        // Connect language change signal to track changes
        connect(languageSelectWindow, &LanguageSelectWindow::languageChanged, 
                this, [this](const QString& language) {
                    transmitter.addSettingsChange("language", language);
                });

        // Connect settingsChanged to LanguageSelectWindow's onThemeChanged slot
        connect(this, &SettingsWindow::settingsChanged, languageSelectWindow, &LanguageSelectWindow::onThemeChanged);

        languageSelectWindow->show();
    }
}

void SettingsWindow::onThemeChanged(const Theme& theme) {
    qDebug() << "Settings Window: === Settings Window Preferred Size:" << theme.settingsWindowPreferredSize;
    setFixedSize(theme.settingsWindowPreferredSize); // Resize SettingsWindow

    // Add theme change to transmitter queue
    transmitter.addSettingsChange("theme", theme.friendlyName);

    // Re-emit the themeChanged signal as settingsChanged
    emit settingsChanged(theme);
}

void SettingsWindow::selectPreviousSound() {
    QSettings settings("KillApiConnect", "KillApiConnectPlus");
    QString currentSound = settings.value("LogDisplay/SoundFile", ":/sounds/beep-beep.mp3").toString();
    
    // Get all available sounds (both MP3 and WAV)
    QStringList soundFiles = soundPlayer->getAvailableSoundFiles();
    
    if (soundFiles.isEmpty()) {
        return;
    }
    
    int currentIndex = soundFiles.indexOf(currentSound);
    if (currentIndex == -1) currentIndex = 0;
    
    int prevIndex = (currentIndex - 1 + soundFiles.size()) % soundFiles.size();
    
    QString prevSound = soundFiles[prevIndex];
    settings.setValue("LogDisplay/SoundFile", prevSound);
    
    // Add sound change to transmitter queue
    transmitter.addSettingsChange("sound", QFileInfo(prevSound).fileName());
    
    // Update display
    soundEdit->setText(QFileInfo(prevSound).fileName());
    
    // Play sound preview
    soundPlayer->playSimpleSound(prevSound);
}

void SettingsWindow::selectNextSound() {
    QSettings settings("KillApiConnect", "KillApiConnectPlus");
    QString currentSound = settings.value("LogDisplay/SoundFile", ":/sounds/beep-beep.mp3").toString();
    
    // Get all available sounds (both MP3 and WAV)
    QStringList soundFiles = soundPlayer->getAvailableSoundFiles();
    
    if (soundFiles.isEmpty()) {
        return;
    }
    
    int currentIndex = soundFiles.indexOf(currentSound);
    int nextIndex = (currentIndex + 1) % soundFiles.size();
    
    QString nextSound = soundFiles[nextIndex];
    settings.setValue("LogDisplay/SoundFile", nextSound);
    
    // Add sound change to transmitter queue
    transmitter.addSettingsChange("sound", QFileInfo(nextSound).fileName());
    
    // Update display
    soundEdit->setText(QFileInfo(nextSound).fileName());
    
    // Play sound preview
    soundPlayer->playSimpleSound(nextSound);
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

bool SettingsWindow::isGameFolderValid(const QString& folder) const {
    QFileInfo launcher(folder + "/StarCitizen_Launcher.exe");
    return launcher.exists() && launcher.isFile();
    // Removed game.log check as it may only exist during runtime
}