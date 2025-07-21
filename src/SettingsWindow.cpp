#include "SettingsWindow.h"
#include "LanguageSelect.h"
#include "ThemeSelect.h"
#include "Transmitter.h"
#include "checkversion.h"
#include "MainWindow.h"
#include "window_utils.h"
#include "GameLauncher.h"
#include "globals.h"
#include "logger.h"
#include <QMessageBox>
#include <QDebug>
#include <QPushButton>
#include <QVBoxLayout>
#include <QDirIterator>
#include <QIcon>
#include <QFileDialog>
#include <QProcess>
#include <QTabWidget>
#include <QTabBar> 
#include <QMouseEvent>  // For QMouseEvent in eventFilter
#include <QInputDialog> // ADDED

SettingsWindow::SettingsWindow(QWidget* parent)
    : QMainWindow(parent), transmitter(this), themeSelectWindow(nullptr), languageSelectWindow(nullptr), 
      signalMapper(new QSignalMapper(this))
{
    setObjectName("SettingsWindow");
    soundPlayer = new SoundPlayer(this);
    
    // Load theme data to get the window size
    ThemeSelectWindow themeSelect;
    Theme currentTheme = ThemeManager::instance().loadCurrentTheme();
    
    // Always use the size from the theme JSON
    setFixedSize(currentTheme.settingsWindowPreferredSize);

    // Initialize the list of available sounds
    availableSounds = getAvailableSounds();

    // Load the last selected sound from QSettings
    QSettings settings("KillApiConnect", "KillApiConnectPlus");
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

    // Connect language change signal to retranslate UI
    connect(&LanguageManager::instance(), &LanguageManager::languageChanged, 
            this, &SettingsWindow::retranslateUi);
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

    // Create tab widget
    tabWidget = new QTabWidget(this);
    
    // Setup tabs
    setupGeneralTab(tabWidget);
    setupGameTab(tabWidget);
    setupSoundsLanguageThemesTab(tabWidget);
    
    mainLayout->addWidget(tabWidget);

    // Ensure the correct font is set before relayout
    tabWidget->setFont(this->font());
    QTabBar* tabBar = tabWidget->tabBar();
    if (tabBar) {
        // Set font and expand properties
        tabBar->setFont(this->font());
        tabBar->setExpanding(true); // Force tabs to fill the bar
        
        // Set minimum width for each tab explicitly
        for (int i = 0; i < tabBar->count(); i++) {
            QString tabText = tabWidget->tabText(i);
            QFontMetrics fm(tabBar->font());
            int textWidth = fm.horizontalAdvance(tabText);
            int minTabWidth = textWidth + 60;
            tabBar->setMinimumWidth(minTabWidth);
        }
        
        // Update layout and visuals
        tabBar->updateGeometry();
        tabBar->adjustSize();
        tabBar->repaint();
    }
    
    // Update tab widget layout
    tabWidget->updateGeometry();
    tabWidget->adjustSize();
    tabWidget->repaint();
}

void SettingsWindow::setupGeneralTab(QTabWidget* tabWidget) {
    QWidget* generalTab = new QWidget();
    QVBoxLayout* generalLayout = new QVBoxLayout(generalTab);
    
    // --- Create a horizontal layout for version and update label ---
    QString versionString = tr("Version: ") + QCoreApplication::applicationVersion();
    QLabel* versionLabel = new QLabel(versionString, this);
    QFont tinyFont = versionLabel->font();
    tinyFont.setPointSizeF(tinyFont.pointSizeF() * 0.7); // Make it smaller
    versionLabel->setFont(tinyFont);
    versionLabel->setStyleSheet("color: gray;");
    versionLabel->setObjectName("versionLabel"); // Set object name for retranslation

    QLabel* updateLabel = new QLabel(tr("Check for Updates"), this);
    updateLabel->setContentsMargins(0, 0, 0, 0);
    updateLabel->setObjectName("updateLabel"); // Set object name for retranslation

    QHBoxLayout* versionUpdateLayout = new QHBoxLayout();
    // Add matching right margin (16px)
    versionUpdateLayout->setContentsMargins(16, 0, 16, 0);
    versionUpdateLayout->setSpacing(8);
    versionUpdateLayout->addWidget(updateLabel);
    versionUpdateLayout->addStretch();
    versionUpdateLayout->addWidget(versionLabel);

    generalLayout->addLayout(versionUpdateLayout);

    // Update checkbox and button below the update label
    updateCheckbox = new QCheckBox(tr("Check for new versions on startup"), this);
    connect(updateCheckbox, &QCheckBox::checkStateChanged, this, &SettingsWindow::toggleUpdateCheck);
    checkUpdatesButton = new QPushButton(tr("Check for updates now"), this);
    connect(checkUpdatesButton, &QPushButton::clicked, this, &SettingsWindow::checkForUpdates);

    QVBoxLayout* updateLayout = new QVBoxLayout();
    // Add matching right margin (16px)
    updateLayout->setContentsMargins(16, 0, 16, 0);
    updateLayout->setSpacing(2);
    updateLayout->addWidget(updateCheckbox);
    updateLayout->addWidget(checkUpdatesButton);

    QWidget* updateCard = new QWidget(this);
    updateCard->setLayout(updateLayout);
    generalLayout->addWidget(updateCard);

    // Gunhead Key
    QLabel* apiLabel = new QLabel(tr("Gunhead Key"), this);
    apiLabel->setObjectName("apiLabel"); // Set object name for retranslation
    apiEdit = new QLineEdit(this);
    apiEdit->setEchoMode(QLineEdit::Password);
    saveApiKeyButton = new QPushButton(tr("Save API Key"), this);
    connect(saveApiKeyButton, &QPushButton::clicked, this, &SettingsWindow::saveApiKey);
    saveApiKeyButton->setObjectName("saveApiKeyButton"); // Set object name for retranslation

    QVBoxLayout* apiLayout = new QVBoxLayout();
    // Add matching right margin (16px)
    apiLayout->setContentsMargins(16, 0, 16, 0);
    apiLayout->addWidget(apiLabel);
    apiLayout->addWidget(apiEdit);
    apiLayout->addWidget(saveApiKeyButton);

    QWidget* apiCard = new QWidget(this);
    apiCard->setLayout(apiLayout);
    generalLayout->addWidget(apiCard);

    // Minimize to Tray option
    minimizeToTrayCheckbox = new QCheckBox(tr("Minimize to system tray instead of closing"), this);
    connect(minimizeToTrayCheckbox, &QCheckBox::checkStateChanged, this, &SettingsWindow::toggleMinimizeToTray);
    
    // Add the Start Minimized option
    startMinimizedCheckbox = new QCheckBox(tr("Start application minimized on launch"), this);
    connect(startMinimizedCheckbox, &QCheckBox::checkStateChanged, this, &SettingsWindow::toggleStartMinimized);
    
    QVBoxLayout* trayLayout = new QVBoxLayout();
    // Add matching right margin (16px)
    trayLayout->setContentsMargins(16, 0, 16, 0);
    trayLayout->addWidget(minimizeToTrayCheckbox);
    trayLayout->addWidget(startMinimizedCheckbox);  // Add the new checkbox
    
    QWidget* trayCard = new QWidget(this);
    trayCard->setLayout(trayLayout);
    generalLayout->addWidget(trayCard);

    generalLayout->addStretch();
    tabWidget->addTab(generalTab, tr("General"));

    // Set up Easter Egg for debug mode activation
    versionLabel->setObjectName("versionLabel");
    versionLabel->installEventFilter(this);
    versionLabel->setCursor(Qt::PointingHandCursor); // Visual hint that it's clickable
    debugClickCount = 0;
    debugClickTimer = new QTimer(this);
    debugClickTimer->setSingleShot(true);
    debugClickTimer->setInterval(30000); // 30 seconds timeout
    connect(debugClickTimer, &QTimer::timeout, this, [this]() {
        debugClickCount = 0;
        qDebug() << "Debug mode activation sequence timed out, resetting counter";
    });
}

void SettingsWindow::setupGameTab(QTabWidget* tabWidget) {
    QWidget* gameTab = new QWidget();
    QVBoxLayout* gameLayout = new QVBoxLayout(gameTab);

    // Game Files Path and Launcher Path
    QLabel* pathLabel = new QLabel(tr("Game LIVE Folder Path"), this);
    pathLabel->setObjectName("pathLabel"); // Set object name for retranslation
    pathEdit = new QLineEdit(this);
    QPushButton* browseButton = new QPushButton(tr("Browse"), this);
    connect(browseButton, &QPushButton::clicked, this, &SettingsWindow::updatePath);
    browseButton->setObjectName("browseButton"); // Set object name for retranslation

    QHBoxLayout* pathLayout = new QHBoxLayout();
    pathLayout->addWidget(pathEdit);
    pathLayout->addWidget(browseButton);

    // RSI Launcher Path (initially hidden)
    QLabel* launcherPathLabel = new QLabel(tr("RSI Launcher Path"), this);
    launcherPathLabel->setObjectName("launcherPathLabel");
    launcherPathEdit = new QLineEdit(this);
    QPushButton* browseLauncherButton = new QPushButton(tr("Browse"), this);
    connect(browseLauncherButton, &QPushButton::clicked, this, &SettingsWindow::updateLauncherPath);
    browseLauncherButton->setObjectName("browseLauncherButton");

    QHBoxLayout* launcherPathLayout = new QHBoxLayout();
    launcherPathLayout->addWidget(launcherPathEdit);
    launcherPathLayout->addWidget(browseLauncherButton);

    // Auto-launch checkbox
    autoLaunchGameCheckbox = new QCheckBox(tr("Auto-launch Star Citizen when monitoring starts"), this);
    connect(autoLaunchGameCheckbox, &QCheckBox::checkStateChanged, this, &SettingsWindow::toggleAutoLaunchGame);

    // Add auto-start monitoring checkbox
    startMonitoringOnLaunchCheckbox = new QCheckBox(tr("Start monitoring automatically when application launches"), this);
    connect(startMonitoringOnLaunchCheckbox, &QCheckBox::checkStateChanged, this, &SettingsWindow::toggleStartMonitoringOnLaunch);

    // Save button
    QPushButton* savePathButton = new QPushButton(tr("Save"), this);
    connect(savePathButton, &QPushButton::clicked, this, &SettingsWindow::savePath);
    savePathButton->setObjectName("savePathButton");

    // Create widgets for launcher controls (initially hidden)
    launcherPathLabel->setVisible(false);
    launcherPathEdit->setVisible(false);
    browseLauncherButton->setVisible(false);

    QVBoxLayout* gamePathLayout = new QVBoxLayout();
    gamePathLayout->addWidget(pathLabel);
    gamePathLayout->addLayout(pathLayout);
    gamePathLayout->addWidget(launcherPathLabel);
    gamePathLayout->addLayout(launcherPathLayout);
    gamePathLayout->addWidget(autoLaunchGameCheckbox);
    gamePathLayout->addWidget(startMonitoringOnLaunchCheckbox); // Add the new checkbox
    gamePathLayout->addWidget(savePathButton);

    QWidget* pathCard = new QWidget(this);
    pathCard->setLayout(gamePathLayout);
    gameLayout->addWidget(pathCard);

    gameLayout->addStretch();
    tabWidget->addTab(gameTab, tr("Game"));
}

void SettingsWindow::setupSoundsLanguageThemesTab(QTabWidget* tabWidget) {
    QWidget* soundsTab = new QWidget();
    QVBoxLayout* soundsLayout = new QVBoxLayout(soundsTab);

    // Sound Selector
    QLabel* soundLabel = new QLabel(tr("Select Notification Sound"), this);
    soundLabel->setObjectName("soundLabel"); // Set object name for retranslation
    soundEdit = new QLineEdit(this);
    soundEdit->setReadOnly(true);

    QPushButton* leftButton = new QPushButton("<", this);
    QPushButton* rightButton = new QPushButton(">", this);
    QPushButton* testSoundButton = new QPushButton(tr("Test"), this);
    testSoundButton->setObjectName("testSoundButton"); // Set object name for retranslation

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
    soundsLayout->addWidget(soundCard);

    // Language and Theme Selector Box
    QVBoxLayout* selectorLayout = new QVBoxLayout();
    QPushButton* languageButton = new QPushButton(tr("Change Language"), this);
    languageButton->setObjectName("languageButton"); // Add this line
    connect(languageButton, &QPushButton::clicked, this, &SettingsWindow::toggleLanguageSelectWindow);
    selectorLayout->addWidget(languageButton);

    QPushButton* themeButton = new QPushButton(tr("Change Theme"), this);
    themeButton->setObjectName("themeButton"); // Add this line
    connect(themeButton, &QPushButton::clicked, this, &SettingsWindow::toggleThemeSelectWindow);
    selectorLayout->addWidget(themeButton);

    QWidget* selectorCard = new QWidget(this);
    selectorCard->setLayout(selectorLayout);
    soundsLayout->addWidget(selectorCard);

    soundsLayout->addStretch();
    tabWidget->addTab(soundsTab, tr("User Interface"));
}

void SettingsWindow::loadSettings() {
    QSettings settings("KillApiConnect", "KillApiConnectPlus");    gameFolder = settings.value("gameFolder", "C:/Program Files/Roberts Space Industries/StarCitizen/LIVE").toString();
    launcherPath = settings.value("launcherPath", "C:/Program Files/Roberts Space Industries/RSI Launcher/RSI Launcher.exe").toString();
    
    // If launcher path is empty, set the default and save it
    if (launcherPath.isEmpty()) {
        launcherPath = "C:/Program Files/Roberts Space Industries/RSI Launcher/RSI Launcher.exe";
        settings.setValue("launcherPath", launcherPath);
    }
    
    apiKey = settings.value("apiKey", "").toString();
    checkUpdatesOnStartup = settings.value("checkUpdatesOnStartup", false).toBool();
    autoLaunchGame = settings.value("autoLaunchGame", false).toBool();
    startMonitoringOnLaunch = settings.value("startMonitoringOnLaunch", false).toBool(); // Add this
    minimizeToTray = settings.value("minimizeToTray", false).toBool();
    startMinimized = settings.value("startMinimized", false).toBool();  // Add this line

    pathEdit->setText(gameFolder);
    launcherPathEdit->setText(launcherPath);
    apiEdit->setText(apiKey);
    updateCheckbox->setChecked(checkUpdatesOnStartup);
    autoLaunchGameCheckbox->setChecked(autoLaunchGame);
    startMonitoringOnLaunchCheckbox->setChecked(startMonitoringOnLaunch); // Add this
    minimizeToTrayCheckbox->setChecked(minimizeToTray);
    startMinimizedCheckbox->setChecked(startMinimized);  // Add this line
    
    // Update launcher controls visibility based on auto-launch setting
    toggleLauncherControlsVisibility(autoLaunchGame);
}

void SettingsWindow::saveSettings() {
    QSettings settings("KillApiConnect", "KillApiConnectPlus");

    settings.setValue("gameFolder", gameFolder);
    settings.setValue("launcherPath", launcherPath);
    settings.setValue("apiKey", apiKey);
    settings.setValue("checkUpdatesOnStartup", checkUpdatesOnStartup);
    settings.setValue("autoLaunchGame", autoLaunchGame);
    settings.setValue("startMonitoringOnLaunch", startMonitoringOnLaunch); // Add this
    settings.setValue("minimizeToTray", minimizeToTray);
    settings.setValue("startMinimized", startMinimized);  // Add this line

    qDebug() << "Settings saved:";
    qDebug() << "  Game Folder:" << gameFolder;
    qDebug() << "  Launcher Path:" << launcherPath;
    qDebug() << "  API Key:" << apiKey;
    qDebug() << "  Check Updates on Startup:" << checkUpdatesOnStartup;
    qDebug() << "  Auto Launch Game:" << autoLaunchGame;
    qDebug() << "  Start Monitoring on Launch:" << startMonitoringOnLaunch; // Add this
    qDebug() << "  Minimize to Tray:" << minimizeToTray;
    qDebug() << "  Start Minimized:" << startMinimized;  // Add this line
}

void SettingsWindow::closeSubWindows() {
    if (themeSelectWindow) {
        themeSelectWindow->close();
        themeSelectWindow = nullptr;
    }
    if (languageSelectWindow) {
        languageSelectWindow->close();
        languageSelectWindow = nullptr;
    }
}

QString SettingsWindow::getGameFolder() const {
    return gameFolder;
}

QString SettingsWindow::getLauncherPath() const {
    return launcherPath;
}

QString SettingsWindow::getApiKey() const {
    return apiKey;
}

bool SettingsWindow::getCheckUpdatesOnStartup() const {
    return checkUpdatesOnStartup;
}

bool SettingsWindow::getAutoLaunchGame() const {
    return autoLaunchGame;
}

bool SettingsWindow::getMinimizeToTray() const {
    return minimizeToTray;
}

bool SettingsWindow::getStartMinimized() const {
    return startMinimized;
}

// Add the getter method
bool SettingsWindow::getStartMonitoringOnLaunch() const {
    return startMonitoringOnLaunch;
}

void SettingsWindow::toggleAutoLaunchGame(int state) {
    autoLaunchGame = (state == Qt::Checked);
    saveSettings();
    qDebug() << "Auto-launch game setting changed to:" << autoLaunchGame;
    
    // Toggle launcher controls visibility
    toggleLauncherControlsVisibility(autoLaunchGame);
}

// Add the toggle handler
void SettingsWindow::toggleStartMonitoringOnLaunch(int state) {
    startMonitoringOnLaunch = (state == Qt::Checked);
    saveSettings();
    qDebug() << "Start monitoring on launch setting changed to:" << startMonitoringOnLaunch;
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
    QString launcher = launcherPathEdit->text().trimmed();
    
    if (!isGameFolderValid(folder)) {
        QMessageBox::warning(this, tr("Invalid Folder"), 
            tr("Selected folder does not contain Star Citizen launcher."));
        return;
    }
    
    // Update both paths if they changed
    bool changed = false;
    if (gameFolder != folder) {
        gameFolder = folder;
        changed = true;
    }
    if (launcherPath != launcher) {
        launcherPath = launcher;
        changed = true;
    }
    
    if (changed) {
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
        QApplication::restoreOverrideCursor(); // Restore cursor BEFORE showing dialog
        QMessageBox::warning(this, tr("Panel Closed Error"), 
            tr("The Gunhead panel for this server has been closed or doesn't exist.\n\n"
               "This means your API key may be valid, but the Discord panel associated with it "
               "has been closed by an administrator or does not exist anymore.\n\n"
               "Please reopen the Gunhead panel or contact your Discord server administrator if you believe this is an error."));
    });
    
    QMetaObject::Connection invalidKeyConnection = connect(&transmitter, &Transmitter::invalidApiKeyError, this, [this, &invalidKeyErrorOccurred]() {
        invalidKeyErrorOccurred = true;
        QApplication::restoreOverrideCursor(); // Restore cursor BEFORE showing dialog
        QMessageBox::warning(this, tr("Authentication Error"), 
            tr("Invalid API key. The server rejected your credentials.\n\n"
               "Please check your API key and try again. Make sure you copied the entire key "
               "without any extra spaces or characters."));
    });

    // Attempt to connect to Gunhead API Services
    if (!apiKey.isEmpty()) {
        bool success = transmitter.sendConnectionSuccess(gameFolder, apiKey);
        if (success) {
            QApplication::restoreOverrideCursor(); // Restore cursor BEFORE showing dialog
            QMessageBox::information(this, tr("Success"), tr("Gunhead Connected successfully!"));
        } 
        else if (!panelClosedErrorOccurred && !invalidKeyErrorOccurred) {
            QApplication::restoreOverrideCursor(); // Restore cursor BEFORE showing dialog
            // Only show a generic error if none of the specific errors were handled
            QMessageBox::warning(this, tr("Connection Error"),
                tr("Failed to connect to Gunhead Server. Please check your internet connection and try again."));
        }
    } else {
        QApplication::restoreOverrideCursor(); // Restore cursor BEFORE showing dialog
        QMessageBox::information(this, tr("API Key Saved"), tr("Your API key has been saved."));
    }

    // Disconnect temporary connections
    disconnect(panelClosedConnection);
    disconnect(invalidKeyConnection);

    // Ensure cursor is restored in all cases
    QApplication::restoreOverrideCursor();
    saveApiKeyButton->setEnabled(true); // Re-enable button
}

void SettingsWindow::checkForUpdates() {
    checkUpdatesButton->setEnabled(false);
    QApplication::setOverrideCursor(Qt::WaitCursor);

    CheckVersion versionChecker;
    QString currentAppVersion = QCoreApplication::applicationVersion();
    QString parserFilePath = "data/logfile_regex_rules.json";
    QString localParserVersion = versionChecker.readLocalJsonVersion(parserFilePath);
    QString currentParserVersion = localParserVersion.isEmpty() ? QStringLiteral("0.0.0") : localParserVersion; // Use fallback if needed

    CheckVersion::UpdateTriState appUpdateState = versionChecker.isAppUpdateAvailable(currentAppVersion, 5000);
    CheckVersion::UpdateTriState parserUpdateState = versionChecker.isParserUpdateAvailable(currentParserVersion, 5000);

    if (appUpdateState == CheckVersion::UpdateTriState::Yes || parserUpdateState == CheckVersion::UpdateTriState::Yes) {
        qDebug() << "SettingsWindow: Updates available, prompting user.";
        QStringList updateOptions;
        if (appUpdateState == CheckVersion::UpdateTriState::Yes)
            updateOptions << tr("Download latest App installer");
        if (parserUpdateState == CheckVersion::UpdateTriState::Yes)
            updateOptions << tr("Update parser rules (logfile_regex_rules.json)");

        bool doAppUpdate = false;
        bool doParserUpdate = false;

        if (updateOptions.size() == 1) {
            int ret = QMessageBox::question(this, tr("Update Available"),
                tr("%1 is available. Would you like to update?").arg(updateOptions.first()),
                QMessageBox::Yes | QMessageBox::No);
            if (ret == QMessageBox::Yes) {
                doAppUpdate = (appUpdateState == CheckVersion::UpdateTriState::Yes);
                doParserUpdate = (parserUpdateState == CheckVersion::UpdateTriState::Yes);
            }
        } else {
            QString selected = QInputDialog::getItem(this, tr("Updates Available"),
                tr("Select what to update:"), updateOptions, 0, false);
            doAppUpdate = (selected == tr("Download latest App installer"));
            doParserUpdate = (selected == tr("Update parser rules (logfile_regex_rules.json)"));
        }

        if (doAppUpdate) {
            QString appInstallerUrl = versionChecker.getAppInstallerUrl();
            QString defaultPath = QDir::homePath() + "/Gunhead-Connect-Setup.msi";
            QString savePath = QFileDialog::getSaveFileName(this, tr("Save Installer"), defaultPath, tr("Installer Files (*.msi)"));
            if (savePath.isEmpty()) {
                qDebug() << "SettingsWindow: User canceled the save dialog.";
            } else {
                if (versionChecker.downloadFile(QUrl(appInstallerUrl), savePath, 5000)) {
                    qDebug() << "SettingsWindow: App updated successfully.";
                    QMessageBox::information(this, tr("Update Successful"), tr("App installer downloaded successfully."));
                    // Prompt the user to run the installer
                    QMessageBox::StandardButton installReply = QMessageBox::question(
                        this, tr("Run Installer"),
                        tr("The installer has been downloaded. Would you like to run it now?"),
                        QMessageBox::Yes | QMessageBox::No);
                    if (installReply == QMessageBox::Yes) {
                        QProcess::startDetached("msiexec", QStringList() << "/i" << savePath);
                    }
                } else {
                    qDebug() << "SettingsWindow: App update failed.";
                    QMessageBox::warning(this, tr("Update Failed"), tr("Failed to download app installer."));
                }
            }
        }
        if (doParserUpdate) {
            QString parserUrl = versionChecker.getParserRulesUrl();
            if (versionChecker.downloadFile(QUrl(parserUrl), parserFilePath, 5000)) {
                QMessageBox::information(this, tr("Parser Updated"),
                    tr("Parser rules have been updated successfully."));
            } else {
                QMessageBox::warning(this, tr("Parser Update Failed"),
                    tr("Failed to update parser rules."));
            }
        }
    } else {
        QMessageBox::information(
            this, tr("No Update"),
            tr("You are running the latest version of the application and parser."));
    }

    QApplication::restoreOverrideCursor();
    checkUpdatesButton->setEnabled(true);
}

void SettingsWindow::toggleThemeSelectWindow() {
    if (themeSelectWindow) {
        themeSelectWindow->close();
        themeSelectWindow = nullptr;
    } else {
        themeSelectWindow = new ThemeSelectWindow(this);
        themeSelectWindow->setAttribute(Qt::WA_DeleteOnClose);

        // Connect to ThemeManager's signal instead of ThemeSelectWindow's
        connect(&ThemeManager::instance(), &ThemeManager::themeChanged, 
                this, &SettingsWindow::onThemeChanged);

        // Clean up the pointer when the window is closed
        connect(themeSelectWindow, &ThemeSelectWindow::destroyed, this, [this]() {
            themeSelectWindow = nullptr;
        });

        // Position the window before showing it
        WindowUtils::positionWindowOnScreen(themeSelectWindow, this);
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

        // Position the window before showing it
        WindowUtils::positionWindowOnScreen(languageSelectWindow, this);
        languageSelectWindow->show();
    }
}

void SettingsWindow::onThemeChanged(const Theme& theme) {
    qDebug() << "SettingsWindow: Theme changed - updating window size to" << theme.settingsWindowPreferredSize;
    setFixedSize(theme.settingsWindowPreferredSize);
    emit settingsChanged(theme); // Pass the updated theme data
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

void SettingsWindow::retranslateUi() {
    // Update window title
    setWindowTitle(tr("Settings"));
    
    // Update tab titles
    if (tabWidget) {
        tabWidget->setTabText(0, tr("General"));
        tabWidget->setTabText(1, tr("Game"));
        tabWidget->setTabText(2, tr("Sounds/Language/Themes"));
    }
    
    // Update labels by object name
    QList<QLabel*> labels = findChildren<QLabel*>();
    for (QLabel* label : labels) {
        if (label->objectName() == "versionLabel") {
            label->setText(tr("Version: ") + QCoreApplication::applicationVersion());
        }
        else if (label->objectName() == "updateLabel") {
            label->setText(tr("Check for Updates"));
        }        else if (label->objectName() == "pathLabel") {
            label->setText(tr("Game LIVE Folder Path"));
        }
        else if (label->objectName() == "launcherPathLabel") {
            label->setText(tr("RSI Launcher Path"));
        }
        else if (label->objectName() == "apiLabel") {
            label->setText(tr("Gunhead Key"));
        }        else if (label->objectName() == "soundLabel") {
            label->setText(tr("Select Notification Sound"));
        }
        else if (label->objectName() == "autoLaunchGameLabel") {
            label->setText(tr("Auto-launch Star Citizen"));
        }
    }
      // Update checkboxes
    updateCheckbox->setText(tr("Check for new versions on startup"));
    autoLaunchGameCheckbox->setText(tr("Auto-launch Star Citizen when monitoring starts"));
    startMonitoringOnLaunchCheckbox->setText(tr("Start monitoring automatically when application launches")); // Add this
    minimizeToTrayCheckbox->setText(tr("Minimize to system tray instead of closing"));
    startMinimizedCheckbox->setText(tr("Start application minimized on launch"));  // Add this line
    checkUpdatesButton->setText(tr("Check for updates now"));
    
    // Find all buttons by object name and update their text
    QList<QPushButton*> buttons = findChildren<QPushButton*>();
    for (QPushButton* button : buttons) {        if (button->objectName() == "browseButton") {
            button->setText(tr("Browse"));
        }
        else if (button->objectName() == "browseLauncherButton") {
            button->setText(tr("Browse"));
        }
        else if (button->objectName() == "savePathButton") {
            button->setText(tr("Save"));
        }
        else if (button->objectName() == "saveApiKeyButton") {
            button->setText(tr("Save API Key"));
        }
        else if (button->objectName() == "testSoundButton") {
            button->setText(tr("Test"));
        }
        else if (button->objectName() == "languageButton") {
            button->setText(tr("Change Language"));
        }
        else if (button->objectName() == "themeButton") {
            button->setText(tr("Change Theme"));
        }
    }
    
    qDebug() << "SettingsWindow UI retranslated";
}

void SettingsWindow::updateLauncherPath() {
    QString file = QFileDialog::getOpenFileName(this, tr("Select RSI Launcher"), launcherPath, tr("Executable Files (*.exe)"));
    if (!file.isEmpty()) {
        launcherPath = file;
        launcherPathEdit->setText(file);
    }
}

void SettingsWindow::toggleLauncherControlsVisibility(bool visible) {
    // Find the launcher controls by object name
    QLabel* launcherPathLabel = findChild<QLabel*>("launcherPathLabel");
    QPushButton* browseLauncherButton = findChild<QPushButton*>("browseLauncherButton");
    
    if (launcherPathLabel) launcherPathLabel->setVisible(visible);
    if (launcherPathEdit) launcherPathEdit->setVisible(visible);
    if (browseLauncherButton) browseLauncherButton->setVisible(visible);
}

void SettingsWindow::toggleMinimizeToTray(int state) {
    minimizeToTray = (state == Qt::Checked);
    saveSettings();
    qDebug() << "Minimize to tray setting changed to:" << minimizeToTray;
    
    emit minimizeToTrayChanged(minimizeToTray);
}

void SettingsWindow::toggleStartMinimized(int state) {
    startMinimized = (state == Qt::Checked);
    saveSettings();
    qDebug() << "Start minimized setting changed to:" << startMinimized;
}

bool SettingsWindow::eventFilter(QObject* watched, QEvent* event)
{
    if (watched->objectName() == "versionLabel" && event->type() == QEvent::MouseButtonPress) {
        // Cast to QMouseEvent* using static_cast (not reinterpret_cast)
        QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
        
        // Check for Ctrl+Shift+Click
        if (mouseEvent->button() == Qt::LeftButton && 
            (QApplication::keyboardModifiers() & (Qt::ControlModifier | Qt::ShiftModifier)) == 
            (Qt::ControlModifier | Qt::ShiftModifier)) {
            
            // Start the timer on first click
            if (debugClickCount == 0) {
                debugClickTimer->start();
            }
            
            debugClickCount++;
            qDebug() << "Debug activation click:" << debugClickCount;
            
            // Check if we've reached 5 clicks
            if (debugClickCount >= 5) {
                debugClickCount = 0; 
                debugClickTimer->stop();
                activateDebugMode();
            }
            return true; // Event handled
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

void SettingsWindow::activateDebugMode()
{
    // Change global setting
    QSettings settings("KillApiConnect", "KillApiConnectPlus");
    bool currentDebugMode = settings.value("DebugMode", false).toBool();
    bool newDebugMode = !currentDebugMode;
    settings.setValue("DebugMode", newDebugMode);
    
    // Update the global debug flag directly
    extern bool ISDEBUG;
    ISDEBUG = newDebugMode;
    
    // Get the version label to flash it
    QLabel* versionLabel = findChild<QLabel*>("versionLabel");
    if (!versionLabel) {
        qWarning() << "Could not find versionLabel for debug mode activation feedback";
        return;
    }
    
    // Visual feedback - flash the version label
    QString originalStyleSheet = versionLabel->styleSheet();
    versionLabel->setStyleSheet("color: red; font-weight: bold;");
    
    qDebug() << "Debug mode changed to:" << newDebugMode;
    
    // Emit the signal about the debug mode change
    emit debugModeChanged(newDebugMode);
    
    // Show feedback without reinitialization
    QTimer::singleShot(100, this, [this, versionLabel, originalStyleSheet, newDebugMode]() {
        if (versionLabel) {
            versionLabel->setStyleSheet(originalStyleSheet);
        }
        
        QMessageBox::information(this, 
            newDebugMode ? "Debug Mode Activated" : "Debug Mode Deactivated",
            newDebugMode ? 
                "Debug mode has been activated. Additional logging will be enabled." :
                "Debug mode has been deactivated. Returning to normal logging level.");
    });
}
