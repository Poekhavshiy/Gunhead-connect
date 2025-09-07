#include "MainWindow.h"

#include "SettingsWindow.h"
#include "LogDisplayWindow.h"
#include "LogMonitor.h"
#include "Transmitter.h"
#include "log_parser.h"
#include "window_utils.h"
#include "GameLauncher.h"
#include "language_manager.h"
#include "ThemeManager.h"

#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QTimer>
#include <QThread>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QDebug>
#include <QFile>
#include <QStyle>
#include <QIcon>
#include <QVBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QSettings>
#include <QStandardPaths>
#include <QDir>
#include <QResource>   // for QResource
#include <QPixmap>     // for QPixmap
#include <QPainter>    // for QPainter
#include <QFont>       // for QFont

#ifdef Q_OS_WIN
#include <windows.h>
#endif

MainWindow::MainWindow(QWidget* parent, LoadingScreen* loadingScreen)
    : QMainWindow(parent)
    , settingsWindow(nullptr)
    , logDisplayWindow(nullptr)
    , logMonitor(new LogMonitor(this)) // Initialize LogMonitor
    , transmitter(this) // Initialize Transmitter
    , isMonitoring(false) // Initialize monitoring state
    , showPvP(false)
    , showPvE(true)
    , showShips(true)
    , showOther(false)
    , showNPCNames(true)
    , systemTrayIcon(nullptr)
    , trayIconMenu(nullptr)
{
    // Set up the main window
    setWindowTitle(tr("Gunhead Connect"));
    setWindowIcon(QIcon(":/icons/Gunhead.ico"));
    setObjectName("MainWindow"); // Set object name for identification
    // Load the game log file path from QSettings
    gameLogFilePath = settings.value("gameFolder", "").toString() + "/game.log";
    logDisplayVisible = settings.value("LogDisplay/Visible", false).toBool();

    // Load filter settings from QSettings
    showPvP = settings.value("LogDisplay/ShowPvP", false).toBool();
    showPvE = settings.value("LogDisplay/ShowPvE", true).toBool();
    showShips = settings.value("LogDisplay/ShowShips", true).toBool();
    showOther = settings.value("LogDisplay/ShowOther", false).toBool();
    showNPCNames = settings.value("LogDisplay/ShowNPCNames", true).toBool();
    
    // STANDBY MODE: Initialize auto-start setting
    updateAutoStartSetting();

    gameLauncherInstance = new GameLauncher(this);
    
    // Load the current theme directly
    Theme currentTheme = ThemeManager::instance().loadCurrentTheme();

    // Apply the theme's preferred size
    setFixedSize(currentTheme.mainWindowPreferredSize);

    // Create and name the central widget for styling
    QWidget* central = new QWidget(this);
    central->setObjectName("mainContainer");
    setCentralWidget(central);

    // Layout for UI controls
    QVBoxLayout* mainLayout = new QVBoxLayout(central);
    mainLayout->setAlignment(Qt::AlignRight | Qt::AlignVCenter); // Align buttons and label to center-right

    const int buttonWidth = 280; // Fixed width for buttons
    int buttonRightSpace = currentTheme.mainButtonRightSpace; // Use value from theme JSON
    qDebug() << "Button right spacing:" << buttonRightSpace;

    // Create buttons and set their fixed width
    startButton = new QPushButton(tr("Start Monitoring"), central);
    startButton->setFixedWidth(buttonWidth);

    settingsButton = new QPushButton(tr("Settings"), central);
    settingsButton->setFixedWidth(buttonWidth);

    logButton = new QPushButton(tr("Show Log Display"), central);
    logButton->setFixedWidth(buttonWidth);

    // Use status label size from theme
    QSize statusLabelSize = currentTheme.statusLabelPreferredSize;
    
    statusLabel = new QLabel(tr("Ready"), central);
    statusLabel->setObjectName("statusLabel");
    statusLabel->setFixedSize(statusLabelSize);
    statusLabel->setWordWrap(true);
    statusLabel->setAlignment(Qt::AlignCenter);

    debugPaths();
    loadRegexRules();  
    qDebug() << "Initializing Game Mode...";
    initializeGameMode();  

    // Add buttons and label with a configurable gap to the right
    auto addWidgetWithGap = [&](QWidget* widget) {
        QHBoxLayout* widgetLayout = new QHBoxLayout();
        widgetLayout->addWidget(widget);
        widgetLayout->addSpacing(buttonRightSpace); // Use theme-configurable spacing
        mainLayout->addLayout(widgetLayout);
        // Store the layout for later updating
        buttonLayouts.append(widgetLayout); // Ensure buttonLayouts is defined
    };

    addWidgetWithGap(startButton);
    addWidgetWithGap(settingsButton);
    addWidgetWithGap(logButton);
    addWidgetWithGap(statusLabel); // Add the status label in the same style

    // Connect button signals
    connect(settingsButton, &QPushButton::clicked,
            this, &MainWindow::toggleSettingsWindow);
    connect(logButton, &QPushButton::clicked,
            this, &MainWindow::toggleLogDisplayWindow);
    connect(startButton, &QPushButton::clicked,
            this, &MainWindow::startMonitoring);
    connect(logMonitor, &LogMonitor::logLineMatched, this, &MainWindow::handleLogLine);
    connect(logMonitor, &LogMonitor::monitoringStopped, this, [this](const QString& reason) {
        // Only stop monitoring if it's active and we're not already stopping
        if (isMonitoring && !isStoppingMonitoring) {
            updateStatusLabel(reason);
            stopMonitoring();
        } else {
            updateStatusLabel(reason);
        }
    });
    connect(logMonitor, &LogMonitor::monitoringStarted, this, [this]() {
        updateStatusLabel(tr("Monitoring started successfully.")); // Updated to use tr()
    });
        
    debugPaths();
    // Load regex rules
    loadRegexRules();

    // Apply the complete theme at startup (stylesheet already applied globally in main.cpp)
    currentTheme.backgroundImage = settings.value("currentBackgroundImage", "").toString();
    ThemeManager::instance().applyCurrentThemeToAllWindows();


    // Add queue processing timer
    QTimer* queueProcessTimer = new QTimer(this);
    connect(queueProcessTimer, &QTimer::timeout, this, [this]() {
        if (!transmitter.logQueue.isEmpty()) {
            QString apiKey = settings.value("apiKey", "").toString();
            if (!apiKey.isEmpty()) {
                qDebug() << "Processing queue with" << transmitter.logQueue.size() << "items";
                transmitter.processQueueInThread(apiKey);
            }
        }
    });
    queueProcessTimer->start(5000); // Check every 5 seconds

    // Connect Transmitter error signals
    connect(&transmitter, &Transmitter::apiError, this, [this](Transmitter::ApiErrorType errorType, const QString& errorMessage) {
        // Update status bar with error message
        updateStatusLabel(errorMessage);
        
        // Stop monitoring for specific errors or server errors
        if (errorType == Transmitter::ApiErrorType::PanelClosed || 
            errorType == Transmitter::ApiErrorType::InvalidApiKey ||
            errorType == Transmitter::ApiErrorType::ServerError) {  // Add ServerError type
            if (isMonitoring) {
                qDebug() << "Stopping monitoring due to API error:" << errorMessage;
                stopMonitoring();
            }
        }
    });

    // Connect the game mode detection signals
    connect(logMonitor, &LogMonitor::gameModeSwitched, this, 
    [this](const QString& gameMode, const QString& subGameMode) {
        // Update UI with the new game mode
        if (gameMode == "PU") {
            updateStatusLabel(tr("Game mode: Persistent Universe"));
        } else if (gameMode == "AC") {
            updateStatusLabel(tr("Game mode: Arena Commander: %1").arg(subGameMode));
        }
        
        // If LogDisplayWindow is open, update its status directly
        if (logDisplayWindow) {
            if (gameMode == "PU") {
                logDisplayWindow->updateStatusLabel(tr("Game mode changed to Persistent Universe"));
            } else if (gameMode == "AC") {
                logDisplayWindow->updateStatusLabel(tr("Game mode changed to Arena Commander: %1").arg(subGameMode));
            }
            
            // Clear the log since game mode changed
            logDisplayWindow->resetLogDisplay();
        }
        
        // If monitoring is active, restart it
        if (isMonitoring) {
            stopMonitoring();
            QTimer::singleShot(1000, this, &MainWindow::startMonitoring);
        }
    });
    
    // Add player info connection
    connect(logMonitor, &LogMonitor::playerInfoDetected,
            &transmitter, &Transmitter::updatePlayerInfo);
    
    // Initialize app with game mode tracking
    QTimer::singleShot(0, this, &MainWindow::initializeApp);

    // Create connection ping timer (60 second interval)
    connectionPingTimer = new QTimer(this);
    connectionPingTimer->setInterval(60000);  // 60 seconds
    connect(connectionPingTimer, &QTimer::timeout, this, &MainWindow::sendConnectionPing);
    
    // STANDBY MODE: Create timer for checking standby-to-active transitions
    standbyCheckTimer = new QTimer(this);
    standbyCheckTimer->setInterval(5000);  // 5 seconds
    connect(standbyCheckTimer, &QTimer::timeout, this, &MainWindow::performStandbyCheck);

    // Connect LogMonitor signals to Transmitter slots
    connect(logMonitor, &LogMonitor::gameModeSwitched, 
        &transmitter, [this](const QString& gameMode, const QString& subGameMode) {
            transmitter.updateGameMode(gameMode, subGameMode, true);
        });

    connect(logMonitor, &LogMonitor::playerInfoDetected,
        &transmitter, [this](const QString& playerName, const QString& playerGEID) {
            transmitter.updatePlayerInfo(playerName, playerGEID);
        });
        
    // Add after other connect statements in the constructor
    connect(&LanguageManager::instance(), &LanguageManager::languageChanged, 
        this, &MainWindow::retranslateUi);
        
    // Create system tray icon if available (with delay to ensure system is ready)
    QTimer::singleShot(500, this, [this]() {
        if (QSystemTrayIcon::isSystemTrayAvailable()) {
            createSystemTrayIcon();
        } else {
            qWarning() << "System tray not available - minimize to tray will not work";
        }
    });

    // Connect debug mode change signal to update Transmitter's API URL and UI elements
    connect(settingsWindow, &SettingsWindow::debugModeChanged, this, [this](bool enabled) {
        // The global ISDEBUG flag is already set in SettingsWindow::activateDebugMode()
        
        qDebug() << "MainWindow: Debug mode changed to" << enabled;
        
        // Forward debug mode change to LogDisplayWindow if it exists
        if (logDisplayWindow) {
            logDisplayWindow->setDebugModeEnabled(enabled);
        }

        // Log the change
        QString message = enabled ? "Debug mode activated via settings" : "Debug mode deactivated via settings";
        if (logDisplayWindow) {
            logDisplayWindow->updateStatusLabel(tr(message.toUtf8().constData()));
        }
        
        // The Transmitter already uses ISDEBUG to determine which API URL to use
        // so we don't need to set it directly, but we can force a debug ping to test the connection
        if (enabled) {
            QString apiKey = settings.value("apiKey", "").toString();
            if (!apiKey.isEmpty()) {
                transmitter.sendDebugPing(apiKey);
            }
        }
    });
}

void MainWindow::handleLogLine(const QString& jsonPayload) {
    qDebug() << "MainWindow: Received log line from LogMonitor:" << jsonPayload;

    // Verify the payload contains valid JSON
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(jsonPayload.toUtf8(), &error);
    if (error.error != QJsonParseError::NoError) {
        qWarning() << "MainWindow::handleLogLine - Received invalid JSON:" << error.errorString();
    } else {
        // Debug some key fields if available
        QJsonObject obj = doc.object();
        if (obj.contains("identifier")) {
            QString identifier = obj["identifier"].toString();
            qDebug() << "MainWindow::handleLogLine - Event identifier:" << identifier;
            
            if (identifier == "kill_log") {
                bool victimIsNPC = obj["victim_is_npc"].toBool();
                bool killerIsNPC = obj["killer_is_npc"].toBool();
                QString victim = obj["victim"].toString();
                QString killer = obj["killer"].toString();
                
                qDebug() << "MainWindow::handleLogLine - Kill event details:"
                         << "victim=" << victim << "(NPC:" << victimIsNPC << ")"
                         << "killer=" << killer << "(NPC:" << killerIsNPC << ")";
            }
        }
    }

    // The filtering is already done in LogMonitor, just process the event
    transmitter.enqueueLog(jsonPayload);
    
    QString apiKey = settings.value("apiKey", "").toString();
    if (!apiKey.isEmpty()) {
        transmitter.processQueueInThread(apiKey);
    } else {
        qWarning() << "Cannot process queue: API key is empty";
    }

    // If LogDisplayWindow is open, display the log
    if (logDisplayWindow) {
        logDisplayWindow->addEvent(jsonPayload);
    }
}

// Add this method to perform the API connection verification before starting monitoring
bool MainWindow::verifyApiConnectionAndStartMonitoring(QString apiKey) {

    
    if (apiKey.isEmpty()) {
        updateStatusLabel(tr("Error: API key not configured. Please set up in Settings."));
        return false;
    }

    qDebug() << "Verifying API connection with debug ping before starting monitoring...";
    updateStatusLabel(tr("Verifying API connection..."));

    // Force the debug ping to always send for verification
    bool pingSuccess = transmitter.sendDebugPing(apiKey, true);

    if (!pingSuccess) {
        qWarning() << "API connection verification failed. Cannot start monitoring.";
        updateStatusLabel(tr("Error: Could not connect to Gunhead server. Monitoring not started."));

        if (logDisplayWindow) {
            logDisplayWindow->updateStatusLabel(tr("ERROR: Failed to connect to Gunhead server. Please check your API key and internet connection."));
        }
        return false;
    }

    qDebug() << "API connection verified successfully. Starting monitoring...";

    bool connectionSuccess = transmitter.sendConnectionSuccess(gameLogFilePath, apiKey);
    if (!connectionSuccess) {
        qWarning() << "Failed to send connection success event, but will continue with monitoring.";
    } else {
        if (logDisplayWindow) {
            logDisplayWindow->updateStatusLabel(tr("Connected to Gunhead server successfully. Monitoring started."));
        }
        qDebug() << "Connection success event sent to API server.";
    }

    return true;
}

// Modify the startMonitoring method to use the verification step
void MainWindow::startMonitoring() {
    qDebug() << "MainWindow: Starting log monitoring.";
    
    // Disable both monitoring buttons and update text
    startButton->setEnabled(false);
    startButton->setText(tr("Connecting..."));
    
    // Also update LogDisplayWindow button if it exists
    if (logDisplayWindow) {
        logDisplayWindow->disableMonitoringButton(tr("Connecting..."));
    }
    
    // Check if auto-launch is enabled and Star Citizen is not running
    bool autoLaunchGame = settings.value("autoLaunchGame", false).toBool();
    qDebug() << "MainWindow: Auto-launch game setting:" << autoLaunchGame;
    if (autoLaunchGame) {
        bool isRunning = gameLauncherInstance->isStarCitizenRunning();
        qDebug() << "MainWindow: Star Citizen running status:" << isRunning;
        
        if (!isRunning) {
            updateStatusLabel(tr("Star Citizen not running. Attempting to launch..."));
            qDebug() << "MainWindow: Auto-launch enabled and Star Citizen not running. Attempting to launch...";
            
            QString launcherPath = settings.value("launcherPath", "C:/Program Files/Roberts Space Industries/RSI Launcher/RSI Launcher.exe").toString();
            qDebug() << "MainWindow: Launcher path from settings:" << launcherPath;
            
            // Check if launcher path is still empty or invalid
            if (launcherPath.isEmpty()) {
                updateStatusLabel(tr("Failed to launch: RSI Launcher path not configured. Please set it in Settings."));
                qWarning() << "MainWindow: RSI Launcher path is empty - user needs to configure it in Settings";
                continueStartMonitoring();
                return;
            }
            
            QFileInfo launcherInfo(launcherPath);
            if (!launcherInfo.exists()) {
                updateStatusLabel(tr("Failed to launch: RSI Launcher not found at configured path. Please check Settings."));
                qWarning() << "MainWindow: RSI Launcher not found at:" << launcherPath;
                continueStartMonitoring();
                return;
            }
            
            if (gameLauncherInstance->launchStarCitizen(launcherPath)) {
                updateStatusLabel(tr("Star Citizen launched. Waiting for game to start..."));
                qDebug() << "MainWindow: Star Citizen launch command sent successfully";
                
                // Give the game some time to start before proceeding
                QTimer::singleShot(5000, this, [this]() {
                    continueStartMonitoring();
                });
                return;
            } else {
                updateStatusLabel(tr("Failed to launch Star Citizen. Continuing with monitoring..."));
                qWarning() << "MainWindow: Failed to launch Star Citizen";
                // Continue with monitoring anyway
            }
        } else {
            qDebug() << "MainWindow: Star Citizen is already running, skipping auto-launch";
        }
    } else {
        qDebug() << "MainWindow: Auto-launch disabled, skipping game launch check";
    }
    
    continueStartMonitoring();
}

void MainWindow::continueStartMonitoring() {
    // Using class member settings directly
    QString apiKey = settings.value("apiKey", "").toString();

    if (!QFile::exists(gameLogFilePath)) {
        qWarning() << "Game log file does not exist:" << gameLogFilePath;
        QString errorMsg = tr("Error: Game log file not found.");
        QString details = tr("The game log file was not found at the expected location:\n%1\nPlease check your game folder settings.").arg(gameLogFilePath);
        updateStatusLabel(errorMsg);
        showSystemTrayMessage(tr("Monitoring Error"), errorMsg, details);
        startButton->setEnabled(true);
        startButton->setText(tr("Start Monitoring"));
        if (logDisplayWindow) {
            logDisplayWindow->enableMonitoringButton(tr("Start Monitoring"));
        }
        return;
    }
    
    // STRATEGY 1 & 3: Force Game Mode Detection Before Monitoring Starts
    qDebug() << "Forcing game mode detection before starting monitoring...";
    updateStatusLabel(tr("Detecting game mode..."));
    
    auto [mode, subMode, playerName, playerGEID] = detectGameModeAndPlayerFast(gameLogFilePath.toStdString());
    currentGameMode = mode;
    currentSubGameMode = subMode;
    
    qDebug() << "Game mode detection result:" << QString::fromStdString(currentGameMode)
             << "Sub-mode:" << QString::fromStdString(currentSubGameMode);
    
    // Block monitoring if game mode is empty or unknown
    if (currentGameMode.empty() || currentGameMode == "Unknown") {
        qWarning() << "Game mode detection failed - blocking monitoring start";
        QString errorMsg = tr("Error: Unable to detect game mode.");
        QString details = tr("The application could not determine the current game mode from the log file.\n"
                           "This usually means:\n"
                           "• The game is not running\n"
                           "• No game session has been started yet\n"
                           "• The log file doesn't contain game mode information\n\n"
                           "Please start a game session (PU or Arena Commander) and try again.");
        updateStatusLabel(errorMsg);
        showSystemTrayMessage(tr("Game Mode Detection Failed"), errorMsg, details);
        startButton->setEnabled(true);
        startButton->setText(tr("Start Monitoring"));
        if (logDisplayWindow) {
            logDisplayWindow->enableMonitoringButton(tr("Start Monitoring"));
            logDisplayWindow->updateStatusLabel(tr("ERROR: Game mode detection failed. Please start a game session and try again."));
        }
        return;
    }
    
    // Update transmitter with detected game mode and player info
    transmitter.setGameLogFilePath(gameLogFilePath); // STRATEGY 2: Set log file path for rescans
    transmitter.updateGameMode(QString::fromStdString(mode), 
                              QString::fromStdString(subMode), 
                              false);
    
    if (!playerName.empty() && !playerGEID.empty()) {
        qDebug() << "Updating transmitter with player info:" 
                 << QString::fromStdString(playerName)
                 << "GEID:" << QString::fromStdString(playerGEID);
        transmitter.updatePlayerInfo(QString::fromStdString(playerName), 
                                    QString::fromStdString(playerGEID));
    }
    
    // Update UI with detected game mode
    if (currentGameMode == "PU") {
        updateStatusLabel(tr("Game mode detected: Persistent Universe"));
    } else if (currentGameMode == "AC") {
        updateStatusLabel(tr("Game mode detected: Arena Commander: %1").arg(QString::fromStdString(currentSubGameMode)));
    }
    
    // Verify API connection before starting monitoring
    if (!verifyApiConnectionAndStartMonitoring(apiKey)) {
        // Check if we should use standby mode instead of failing completely
        updateAutoStartSetting(); // Refresh auto-start setting
        if (autoStartEnabled && transmitter.isGameModeValid()) {
            qDebug() << "API connection failed, but auto-start is enabled and game mode is valid - starting standby mode";
            startStandbyMode();
            return;
        }
        
        QString errorMsg = tr("Error: Could not connect to Gunhead server. Monitoring not started.");
        QString details = tr("API connection failed. Please check your API key and internet connection.\nAPI Key: %1\n\nTIP: Enable 'Start monitoring automatically' to use standby mode while connection issues are resolved.").arg(apiKey);
        updateStatusLabel(errorMsg);
        showSystemTrayMessage(tr("Monitoring Error"), errorMsg, details);
        startButton->setEnabled(true);
        startButton->setText(tr("Start Monitoring"));
        if (logDisplayWindow) {
            logDisplayWindow->enableMonitoringButton(tr("Start Monitoring"));
        }
        return; // Don't start monitoring if verification failed
    }
    
    // STRATEGY 1 & 3: Final validation - ensure game mode is still valid before starting monitoring
    if (!validateGameModeForMonitoring()) {
        // Check if we should use standby mode instead of failing completely
        updateAutoStartSetting(); // Refresh auto-start setting
        if (autoStartEnabled) {
            qDebug() << "Game mode validation failed, but auto-start is enabled - starting standby mode";
            startStandbyMode();
            return;
        }
        
        // Traditional failure handling when auto-start is disabled
        QString errorMsg = tr("Error: Game mode validation failed.");
        QString details = tr("Cannot start monitoring because the game mode could not be determined.\n"
                           "This typically means the game is not running or no session has been started.\n\n"
                           "Please ensure:\n"
                           "• Star Citizen is running\n"
                           "• You have started a game session (PU or Arena Commander)\n"
                           "• The game has generated log data\n\n"
                           "Then try starting monitoring again.\n\n"
                           "TIP: Enable 'Start monitoring automatically' in settings to use standby mode.");
        updateStatusLabel(errorMsg);
        showSystemTrayMessage(tr("Game Mode Validation Failed"), errorMsg, details);
        startButton->setEnabled(true);
        startButton->setText(tr("Start Monitoring"));
        if (logDisplayWindow) {
            logDisplayWindow->enableMonitoringButton(tr("Start Monitoring"));
            logDisplayWindow->updateStatusLabel(tr("ERROR: Game mode validation failed. Enable auto-start for standby mode."));
        }
        return; // Don't start monitoring if game mode validation failed
    }

    // Declare QSettings before using it
    bool showPvP = settings.value("LogDisplay/ShowPvP", false).toBool();
    bool showPvE = settings.value("LogDisplay/ShowPvE", true).toBool();
    bool showNPCNames = settings.value("LogDisplay/ShowNPCNames", true).toBool();
    qDebug() << "FILTER STATE AT MONITORING START:"
             << "showPvP=" << showPvP
             << "showPvE=" << showPvE
             << "showNPCNames=" << showNPCNames;
    
    qDebug() << "Game log file exists, starting monitoring.";
    logMonitor->startMonitoring(gameLogFilePath);
    qDebug() << "Started monitoring game log file:" << gameLogFilePath;

    // After successful verification and starting monitoring
    startButton->setEnabled(true); 
    startButton->setText(tr("Stop Monitoring"));
    disconnect(startButton, &QPushButton::clicked, this, &MainWindow::startMonitoring);
    connect(startButton, &QPushButton::clicked, this, &MainWindow::stopMonitoring);

    // Update LogDisplayWindow button if it exists
    isMonitoring = true;
    if (logDisplayWindow) {
        logDisplayWindow->updateMonitoringButtonText(true);
    }
    
    // Update system tray action text
    if (startStopAction) {
        startStopAction->setText(tr("Stop Monitoring"));
    }
    
    // Start the connection ping timer
    connectionPingTimer->start();
    qDebug() << "Started connection ping timer - will check connection every 60 seconds";
    
    // Update status label
    updateStatusLabel(tr("Monitoring started."));
}

void MainWindow::handleMonitoringToggleRequest() {
    qDebug() << "Monitoring toggle requested from LogDisplayWindow";
    if (isMonitoring) {
        stopMonitoring();
    } else {
        startMonitoring();
    }
    if (logDisplayWindow) {
        logDisplayWindow->updateMonitoringButtonText(isMonitoring);
    }
}

void MainWindow::stopMonitoring() {
    // Guard against recursive calls
    if (isStoppingMonitoring) {
        return;
    }
    isStoppingMonitoring = true;
    
    // Stop the connection ping timer
    connectionPingTimer->stop();
    qDebug() << "Stopped connection ping timer";
    
    // STANDBY MODE: Stop standby check timer and exit standby mode
    if (isInStandbyMode) {
        standbyCheckTimer->stop();
        isInStandbyMode = false;
        transmitter.setStandbyMode(false);
        qDebug() << "Exited standby mode";
    }
    
    qDebug() << "MainWindow::stopMonitoring - Stopping log monitoring.";
    if (logMonitor) {
        logMonitor->stopMonitoring();
    }

    // Update the button label and reconnect it to startMonitoring
    startButton->setText(tr("Start Monitoring"));
    startButton->setStyleSheet(""); // Remove any standby styling
    disconnect(startButton, &QPushButton::clicked, this, &MainWindow::stopMonitoring);
    connect(startButton, &QPushButton::clicked, this, &MainWindow::startMonitoring);

    // Update LogDisplayWindow button if it exists
    isMonitoring = false;
    if (logDisplayWindow) {
        logDisplayWindow->updateMonitoringButtonText(false);
    }
    
    // Update system tray action text
    if (startStopAction) {
        startStopAction->setText(tr("Start Monitoring"));
    }

    updateStatusLabel(tr("Monitoring stopped."));
    isStoppingMonitoring = false;
}

// STRATEGY 1 & 3: Helper method to validate game mode before monitoring
bool MainWindow::validateGameModeForMonitoring() {
    qDebug() << "Validating game mode for monitoring...";
    
    // Check if current game mode is valid
    if (!transmitter.isGameModeValid()) {
        qWarning() << "Current game mode is invalid:" << QString::fromStdString(currentGameMode);
        
        // Attempt to re-detect game mode
        qDebug() << "Attempting game mode re-detection...";
        auto [mode, subMode, playerName, playerGEID] = detectGameModeAndPlayerFast(gameLogFilePath.toStdString());
        
        if (!mode.empty() && mode != "Unknown") {
            qDebug() << "Game mode re-detection successful:" << QString::fromStdString(mode);
            currentGameMode = mode;
            currentSubGameMode = subMode;
            transmitter.updateGameMode(QString::fromStdString(mode), 
                                      QString::fromStdString(subMode), 
                                      false);
            return true;
        } else {
            qWarning() << "Game mode re-detection failed - monitoring cannot start safely";
            return false;
        }
    }
    
    qDebug() << "Game mode validation successful:" << QString::fromStdString(currentGameMode);
    return true;
}

// STANDBY MODE: Start monitoring in standby mode (logs processed locally, not transmitted)
void MainWindow::startStandbyMode() {
    qDebug() << "Starting standby monitoring mode...";
    isInStandbyMode = true;
    
    // Start local log monitoring without API transmission
    logMonitor->startMonitoring(gameLogFilePath);
    transmitter.setStandbyMode(true);
    
    // Start the standby check timer
    standbyCheckTimer->start();
    
    // Update UI to reflect standby state
    startButton->setEnabled(true);
    startButton->setText(tr("Stop Standby"));
    startButton->setStyleSheet("QPushButton { background-color: orange; color: white; }");
    
    // Reconnect button to stop standby mode
    disconnect(startButton, &QPushButton::clicked, this, &MainWindow::startMonitoring);
    connect(startButton, &QPushButton::clicked, this, &MainWindow::stopMonitoring);
    
    if (logDisplayWindow) {
        logDisplayWindow->updateMonitoringButtonText(false); // Show as stopped for API transmission
        logDisplayWindow->updateStatusLabel(tr("STANDBY: Monitoring logs locally, waiting for game mode detection..."));
    }
    
    updateStatusLabel(tr("Standby: Monitoring logs, waiting for valid game mode..."));
    
    qDebug() << "Standby mode started - monitoring logs locally until game mode is detected";
}

// STANDBY MODE: Exit standby mode and start full monitoring
void MainWindow::exitStandbyMode() {
    qDebug() << "Exiting standby mode and starting full monitoring...";
    isInStandbyMode = false;
    transmitter.setStandbyMode(false);
    
    // Remove standby styling
    startButton->setStyleSheet("");
    
    // Continue with normal monitoring startup
    continueStartMonitoring();
}

// STANDBY MODE: Check if auto-start conditions are met
bool MainWindow::shouldAttemptAutoStart() const {
    return autoStartEnabled && !isMonitoring && !isInStandbyMode;
}

// STANDBY MODE: Check conditions and attempt auto-start if appropriate
void MainWindow::checkForAutoStartConditions() {
    qDebug() << "Checking auto-start conditions...";
    qDebug() << "Auto-start enabled:" << autoStartEnabled;
    qDebug() << "Is monitoring:" << isMonitoring;
    qDebug() << "Is in standby:" << isInStandbyMode;
    qDebug() << "Game mode valid:" << transmitter.isGameModeValid();
    
    if (!shouldAttemptAutoStart()) {
        qDebug() << "Auto-start conditions not met, skipping";
        return;
    }
    
    // Check if we have valid game mode now
    if (transmitter.isGameModeValid()) {
        QString apiKey = settings.value("apiKey", "").toString();
        if (!apiKey.isEmpty()) {
            qDebug() << "Auto-start conditions met - attempting to start full monitoring";
            updateStatusLabel(tr("Auto-starting monitoring (game mode detected)..."));
            QTimer::singleShot(1000, this, &MainWindow::startMonitoring);
        } else {
            qDebug() << "Auto-start: Game mode valid but no API key - starting standby mode";
            startStandbyMode();
        }
    } else {
        qDebug() << "Auto-start: Game mode not valid yet - starting standby mode";
        startStandbyMode();
    }
}

// STANDBY MODE: Update cached auto-start setting
void MainWindow::updateAutoStartSetting() {
    bool previousSetting = autoStartEnabled;
    autoStartEnabled = settings.value("autoStartMonitoring", false).toBool();
    
    qDebug() << "Auto-start setting updated from" << previousSetting << "to" << autoStartEnabled;
    
    // If auto-start was just enabled and we're not monitoring, check conditions
    if (autoStartEnabled && !previousSetting) {
        QTimer::singleShot(500, this, &MainWindow::checkForAutoStartConditions);
    }
}

// STANDBY MODE: Periodic check for standby-to-active transitions
void MainWindow::performStandbyCheck() {
    if (!isInStandbyMode) {
        return; // Not in standby mode, nothing to check
    }
    
    qDebug() << "Performing standby check...";
    
    // Check if conditions are now met for full monitoring
    QString apiKey = settings.value("apiKey", "").toString();
    bool gameModeBecameValid = transmitter.isGameModeValid();
    bool hasValidApiKey = !apiKey.isEmpty();
    
    qDebug() << "Standby check - Game mode valid:" << gameModeBecameValid 
             << "API key available:" << hasValidApiKey;
    
    if (gameModeBecameValid && hasValidApiKey) {
        qDebug() << "STANDBY TRANSITION: All conditions met, transitioning to full monitoring";
        updateStatusLabel(tr("Standby complete: Game mode and API key ready. Starting full monitoring..."));
        
        // Stop the standby check timer
        standbyCheckTimer->stop();
        
        // Transition to full monitoring
        QTimer::singleShot(1000, this, &MainWindow::exitStandbyMode);
    } else if (gameModeBecameValid && !hasValidApiKey) {
        updateStatusLabel(tr("Standby: Game mode detected, but API key required for transmission"));
    } else {
        // Still waiting for game mode
        updateStatusLabel(tr("Standby: Monitoring logs, waiting for game mode..."));
    }
}

void MainWindow::toggleLogDisplayWindow(bool forceNotVisible) {
    qDebug() << "Toggling LogDisplayWindow visibility.";
    if (forceNotVisible || logDisplayWindow) {
        qDebug() << "LogDisplayWindow is open, closing it.";
        settings.setValue("LogDisplay/Visible", false); // Use class member
        logDisplayVisible = false;
        if (logDisplayWindow) {
            logDisplayWindow->close();
            logDisplayWindow = nullptr; // Reset the pointer
        }
        logButton->setText(tr("Show Log Window"));
        // Update tray menu action text
        if (showLogDisplayAction) {
            showLogDisplayAction->setText(tr("Show Log Window"));
        }
    } else {
        qDebug() << "LogDisplayWindow is closed, opening it.";
        logDisplayWindow = new LogDisplayWindow(transmitter);
        logDisplayWindow->setAttribute(Qt::WA_DeleteOnClose);
        logDisplayWindow->setWindowFlag(Qt::Window);
        // Ensure object name is set for theme application
        logDisplayWindow->setObjectName("LogDisplayWindow");
        logDisplayWindow->show();
        // Apply the theme after the window is shown
        ThemeManager::instance().applyCurrentThemeToAllWindows();

        // Connect LogDisplayWindow signals
        connect(logDisplayWindow, &LogDisplayWindow::windowClosed, this, [this]() {
            qDebug() << "LogDisplayWindow closed signal received.";
            settings.setValue("LogDisplay/Visible", false);
            logDisplayVisible = false;
            logDisplayWindow = nullptr;
            logButton->setText(tr("Show Log Window"));
            // Update tray menu action text
            if (showLogDisplayAction) {
                showLogDisplayAction->setText(tr("Show Log Window"));
            }
        });

        connect(logDisplayWindow, &LogDisplayWindow::toggleMonitoringRequested, 
                this, &MainWindow::handleMonitoringToggleRequest);

        connect(logDisplayWindow, &LogDisplayWindow::filterPvPChanged, 
                this, [this](bool show) { setShowPvP(show); });

        connect(logDisplayWindow, &LogDisplayWindow::filterPvEChanged, 
                this, [this](bool show) { setShowPvE(show); });

        connect(logDisplayWindow, &LogDisplayWindow::filterShipsChanged,
                this, [this](bool show) { setShowShips(show); });

        connect(logDisplayWindow, &LogDisplayWindow::filterOtherChanged,
                this, [this](bool show) { setShowOther(show); });

        connect(logDisplayWindow, &LogDisplayWindow::filterNPCNamesChanged, 
                this, [this](bool show) { setShowNPCNames(show); });

        // Update monitoring button state
        logDisplayWindow->updateMonitoringButtonText(
            startButton->text() == "Stop Monitoring");

        logDisplayWindow->show();
        logButton->setText(tr("Hide Log Window"));
        // Update tray menu action text
        if (showLogDisplayAction) {
            showLogDisplayAction->setText(tr("Hide Log Window"));
        }
    }
}

void MainWindow::toggleSettingsWindow() {
    if (settingsWindow) {
        if (settingsWindow->isMinimized()) {
            settingsWindow->setWindowState(settingsWindow->windowState() & ~Qt::WindowMinimized);
            settingsWindow->raise();
            settingsWindow->activateWindow();
        } 
        else if (settingsWindow->isVisible()) {
            settingsWindow->close();
        }
        else {
            settingsWindow->show();
            settingsWindow->raise();
            settingsWindow->activateWindow();
        }
    } else {
        settingsWindow = new SettingsWindow(this);
        connect(settingsWindow, &SettingsWindow::settingsChanged,
                this, &MainWindow::applyTheme);
        connect(settingsWindow, &SettingsWindow::gameFolderChanged, 
                this, &MainWindow::onGameFolderChanged);
        connect(settingsWindow, &SettingsWindow::minimizeToTrayChanged,
                this, &MainWindow::onMinimizeToTrayChanged);
        
        // STANDBY MODE: Connect to settings changes to monitor API key changes
        connect(settingsWindow, &SettingsWindow::settingsChanged, this, [this]() {
            // Check if API key was updated and we should attempt auto-restart
            updateAutoStartSetting();
            
            static QString lastApiKey = settings.value("apiKey", "").toString();
            QString currentApiKey = settings.value("apiKey", "").toString();
            
            if (currentApiKey != lastApiKey && !currentApiKey.isEmpty()) {
                qDebug() << "API key changed - checking for auto-restart conditions";
                lastApiKey = currentApiKey;
                
                // If we're in standby mode and now have a valid API key, attempt transition
                if (isInStandbyMode && transmitter.isGameModeValid()) {
                    qDebug() << "API key updated during standby - transitioning to full monitoring";
                    updateStatusLabel(tr("API key updated! Transitioning to full monitoring..."));
                    QTimer::singleShot(1500, this, &MainWindow::exitStandbyMode);
                }
                // If auto-start is enabled and we're not monitoring, start now
                else if (autoStartEnabled && !isMonitoring && !isInStandbyMode) {
                    qDebug() << "API key updated with auto-start enabled - attempting to start monitoring";
                    QTimer::singleShot(1000, this, &MainWindow::checkForAutoStartConditions);
                }
            }
        });

        WindowUtils::positionWindowOnScreen(settingsWindow, this);
        settingsWindow->show();
    }
}

void MainWindow::applyTheme(Theme themeData)
{
    qDebug() << "MainWindow resizing to:" << themeData.mainWindowPreferredSize << " and button right spacing:" << themeData.mainButtonRightSpace;
    setFixedSize(themeData.mainWindowPreferredSize); // Resize MainWindow
        
    // Update the spacing without checking QSettings
    qDebug() << "Applying button spacing to:" << themeData.mainButtonRightSpace;
    
    // Debug buttonLayouts
    qDebug() << "buttonLayouts contains" << buttonLayouts.size() << "layouts";
    
    // Update all layouts
    for (QHBoxLayout* layout : buttonLayouts) {
        // Remove the old spacing item
        if (layout->count() > 1) {
            QLayoutItem* spacerItem = layout->takeAt(1);
            delete spacerItem;
            qDebug() << "Removed old spacing item";
        } else {
            qWarning() << "Layout has fewer than 2 items - cannot remove spacing!";
        }
        
        // Add new spacing
        layout->addSpacing(themeData.mainButtonRightSpace);
        qDebug() << "Added new spacing:" << themeData.mainButtonRightSpace;
    }
    
    // Save only the button spacing, not window sizes
    settings.setValue("mainButtonRightSpace", themeData.mainButtonRightSpace);
    centralWidget()->updateGeometry();
    centralWidget()->layout()->invalidate();
    centralWidget()->layout()->activate();
    centralWidget()->adjustSize();
    
    qDebug() << "Layout update forced with new spacing:" << themeData.mainButtonRightSpace;
    

    // Apply status label size from theme
    if (statusLabel) {
        qDebug() << "Status label resizing to:" << themeData.statusLabelPreferredSize;
        statusLabel->setFixedSize(themeData.statusLabelPreferredSize);
        statusLabel->setWordWrap(true); // Ensure word wrap is enabled
        statusLabel->setAlignment(Qt::AlignCenter);
    }
    
    qDebug() << "Applying background image:" << themeData.backgroundImage;
    const QString imagePath = themeData.backgroundImage;
    if (imagePath.isEmpty()) {
        centralWidget()->setStyleSheet("");
        qDebug() << "Cleared background image.";
        return;
    }

    // Validate resource exists
    if (!QFile::exists(imagePath)) {
        qWarning() << "Background image not found:" << imagePath;
        return;
    }
    //Opening the file to check if it is a valid image
    QFile file(imagePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to open background image:" << imagePath;
        return;
    }
    qDebug() << "Opened background image file:" << imagePath;
    // Apply only to the central widget using objectName selector
    QString sheet = QString(
        "#mainContainer {"
        " background-image: url(\"%1\");"
        " background-repeat: no-repeat;"
        " background-position: top left;"
        " }").arg(imagePath);
    qDebug() << "Background image stylesheet:" << sheet;
    centralWidget()->setStyleSheet(sheet);
    qDebug() << "Applied central widget stylesheet:" << sheet;
}

void MainWindow::closeEvent(QCloseEvent* event) {
    // Check if minimize to tray is enabled and system tray is available
    bool minimizeToTray = settings.value("minimizeToTray", false).toBool();
    bool logDisplayShouldBeVisible = settings.value("LogDisplay/Visible", false).toBool();

    if (minimizeToTray && systemTrayIcon && systemTrayIcon->isVisible() && !isShuttingDown) {
        // Hide the main window instead of closing
        hide();
        event->ignore();

        // ADDED: Update tray menu label to "Show Main Window"
        if (showHideAction) {
            showHideAction->setText(tr("Show Main Window"));
        }

        // Close all sub-windows except LogDisplayWindow
        if (settingsWindow) {
            settingsWindow->closeSubWindows();
            settingsWindow->close();
            settingsWindow = nullptr;
        }
        
        // Keep LogDisplayWindow visible if setting is true
        if (logDisplayShouldBeVisible && logDisplayWindow) {
            logDisplayWindow->show();
            logDisplayWindow->raise();
            logDisplayWindow->activateWindow();
        } else if (logDisplayWindow) {
            logDisplayWindow->hide();
        }

        // Show tray message the first time
        static bool firstTimeMinimized = true;
        if (firstTimeMinimized) {
            showSystemTrayMessage(tr("Gunhead Connect"), tr("Application was minimized to tray"), QString());
            firstTimeMinimized = false;
        }

        qDebug() << "Window minimized to system tray, all sub-windows except LogDisplayWindow closed";
        return;
    } else if (minimizeToTray && (!systemTrayIcon || !systemTrayIcon->isVisible())) {
        qWarning() << "Minimize to tray enabled but system tray icon not available - closing normally";
    }

    // Normal close behavior
    isShuttingDown = true;

    // Hide the system tray icon immediately to prevent ghost icons
    if (systemTrayIcon) {
        systemTrayIcon->hide();
        systemTrayIcon->deleteLater();
        systemTrayIcon = nullptr;
    }

    if (logMonitor) {
        logMonitor->stopMonitoring(); // Gracefully stop monitoring
        updateStatusLabel(tr("Monitoring stopped."));
    }
    qDebug() << "Final state of LogDisplayWindow:" << logDisplayVisible << " or " << settings.value("LogDisplay/Visible", false).toBool();
    updateStatusLabel(tr("MainWindow closed, stopping monitoring and closing LogDisplayWindow."));
    if (logDisplayWindow) {
        logDisplayWindow->setApplicationShuttingDown(true); // Pass the flag
        logDisplayWindow->close(); // Close the LogDisplayWindow
        logDisplayWindow = nullptr; // Reset the pointer
    }
    qDebug() << "MainWindow closed, stopping monitoring and closing LogDisplayWindow.";
    QMainWindow::closeEvent(event);
    qDebug() << "Actual final state of LogDisplayWindow:" << logDisplayVisible << " or " << settings.value("LogDisplay/Visible", false).toBool();
}

void MainWindow::loadRegexRules() {
    QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/Gunhead-Connect/data";
    QString jsonFilePath = dataDir + "/logfile_regex_rules.json";
    QFile writableFile(jsonFilePath);
    if (!writableFile.exists()) {
        QString originalPath = QCoreApplication::applicationDirPath() + "/data/logfile_regex_rules.json";
        QFile originalFile(originalPath);
        if (originalFile.exists()) {
            QDir dir = QFileInfo(jsonFilePath).absoluteDir();
            if (!dir.exists()) dir.mkpath(".");
            originalFile.copy(jsonFilePath);
        }
    }
    QString rulesFilePath = settings.value("regexRulesFile", jsonFilePath).toString();

    if (!QFile::exists(rulesFilePath)) {
        qWarning() << "Regex rules file does not exist:" << rulesFilePath;
        return;
    }

    QString parsedRules = QString::fromStdString(load_regex_rules(rulesFilePath.toStdString()));

    if (parsedRules.isEmpty() || parsedRules == "{}") {
        qWarning() << "Failed to load regex rules from file:" << rulesFilePath;
    } else {
        qDebug() << "Regex rules successfully loaded from:" << rulesFilePath;
    }
    qDebug() << "Loaded regex rules:" << parsedRules;
}

void MainWindow::debugPaths() {
    QString defaultPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/Gunhead-Connect/data/logfile_regex_rules.json";
    QString regexRulesFile = settings.value("regexRulesFile", defaultPath).toString();
    qDebug() << "Regex Rules File Path:" << regexRulesFile;

    QString backgroundImage = settings.value("currentBackgroundImage", "").toString();
    qDebug() << "Background Image Path:" << backgroundImage;

    qDebug() << "Game Log File Path:" << gameLogFilePath;

    QString apiKey = settings.value("apiKey", "").toString();
    QString gameFolder = settings.value("gameFolder", "").toString();

    bool hasGameFolder = !gameFolder.isEmpty();
    bool hasApiKey = !apiKey.isEmpty();
    bool hasGameLog = !gameLogFilePath.isEmpty() && QFile::exists(gameLogFilePath);

    if (!hasGameFolder && !hasApiKey) {
        updateStatusLabel(tr("Please configure the game directory and API key."));
    } else if (!hasGameFolder) {
        updateStatusLabel(tr("Please configure the game directory."));
    } else if (!hasApiKey) {
        updateStatusLabel(tr("Please configure your API key."));
    } else if (!hasGameLog) {
        updateStatusLabel(tr("Game log file not found in the selected directory."));
    } else {
        updateStatusLabel(tr("Ready to start monitoring."));
    }
}

void MainWindow::updateStatusLabel(const QString& status) {
    if (statusLabel) {
        lastStatusKey = status; // Store the untranslated key
        statusLabel->setText(tr(status.toUtf8().constData())); // Translate now
        qDebug() << "Status updated to:" << status;
    }
}

void MainWindow::focusInEvent(QFocusEvent* event) {
    QMainWindow::focusInEvent(event);
    
    // If LogDisplayWindow exists and is visible, bring it to the front
    if (logDisplayWindow && logDisplayWindow->isVisible()) {
        qDebug() << "MainWindow::focusInEvent - Attempting to bring LogDisplayWindow to foreground";
        
        // Get window handles
        HWND mainHwnd = (HWND)this->winId();
        HWND logHwnd = (HWND)logDisplayWindow->winId();
        
        // Ensure LogDisplayWindow is not minimized
        logDisplayWindow->setWindowState(logDisplayWindow->windowState() & ~Qt::WindowMinimized);
        logDisplayWindow->show();
        
        // Method 1: More aggressive z-order manipulation
        // Get the foreground window's thread
        DWORD foreThreadId = GetWindowThreadProcessId(GetForegroundWindow(), NULL);
        DWORD currentThreadId = GetCurrentThreadId();
        
        // Attach to the foreground window's thread to allow focus changes
        AttachThreadInput(currentThreadId, foreThreadId, TRUE);
        
        // Force LogDisplayWindow to the very top, even above active windows
        SetWindowPos(logHwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
        
        // Wait a small amount of time for window manager to process
        Sleep(50);
        
        // Remove topmost flag but keep the window visible
        SetWindowPos(logHwnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
        
        // Make MainWindow the foreground window
        SetForegroundWindow(mainHwnd);
        
        // Detach from the thread
        AttachThreadInput(currentThreadId, foreThreadId, FALSE);
        
        // Method 2: Use a timer to re-apply the topmost flag after a delay
        QTimer::singleShot(200, this, [logHwnd]() {
            // Second attempt after a short delay
            SetWindowPos(logHwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
            Sleep(50);
            SetWindowPos(logHwnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        });
        
        qDebug() << "MainWindow::focusInEvent - Enhanced window ordering applied using Windows API";
    }
}

void MainWindow::setShowPvP(bool show) {
    qDebug() << "MainWindow::setShowPvP - Value changing from" << showPvP << "to" << show;
    if (showPvP != show) {
        showPvP = show;
        settings.setValue("LogDisplay/ShowPvP", showPvP);
        qDebug() << "MainWindow::setShowPvP - Saved new value to QSettings";
        emit filterSettingsChanged();
        qDebug() << "MainWindow::setShowPvP - Emitted filterSettingsChanged signal";
        
        // Automatically restart monitoring if active
        if (isMonitoring) {
            qDebug() << "MainWindow::setShowPvP - Automatically restarting monitoring to apply changes";
            updateStatusLabel(tr("Filter changed - automatically applying changes"));
            restartMonitoring();
        }
    } else {
        qDebug() << "MainWindow::setShowPvP - Value unchanged, no action taken";
    }
}

void MainWindow::setShowPvE(bool show) {
    qDebug() << "MainWindow::setShowPvE - Value changing from" << showPvE << "to" << show;
    if (showPvE != show) {
        showPvE = show;
        settings.setValue("LogDisplay/ShowPvE", showPvE);
        qDebug() << "MainWindow::setShowPvE - Saved new value to QSettings";
        emit filterSettingsChanged();
        qDebug() << "MainWindow::setShowPvE - Emitted filterSettingsChanged signal";
        
        // Automatically restart monitoring if active
        if (isMonitoring) {
            qDebug() << "MainWindow::setShowPvE - Automatically restarting monitoring to apply changes";
            updateStatusLabel(tr("Filter changed - automatically applying changes"));
            restartMonitoring();
        }
    } else {
        qDebug() << "MainWindow::setShowPvE - Value unchanged, no action taken";
    }
}

void MainWindow::setShowShips(bool show) {
    qDebug() << "MainWindow::setShowShips - Value changing from" << showShips << "to" << show;
    if (showShips != show) {
        showShips = show;
        settings.setValue("LogDisplay/ShowShips", showShips);
        qDebug() << "MainWindow::setShowShips - Saved new value to QSettings";
        emit filterSettingsChanged();
        qDebug() << "MainWindow::setShowShips - Emitted filterSettingsChanged signal";
        
        // Automatically restart monitoring if active
        if (isMonitoring) {
            qDebug() << "MainWindow::setShowShips - Automatically restarting monitoring to apply changes";
            updateStatusLabel(tr("Filter changed - automatically applying changes"));
            restartMonitoring();
        }
    } else {
        qDebug() << "MainWindow::setShowShips - Value unchanged, no action taken";
    }
}

void MainWindow::setShowOther(bool show) {
    qDebug() << "MainWindow::setShowOther - Value changing from" << showOther << "to" << show;
    if (showOther != show) {
        showOther = show;
        settings.setValue("LogDisplay/ShowOther", showOther);
        qDebug() << "MainWindow::setShowOther - Saved new value to QSettings";
        emit filterSettingsChanged();
        qDebug() << "MainWindow::setShowOther - Emitted filterSettingsChanged signal";
        
        // Automatically restart monitoring if active
        if (isMonitoring) {
            qDebug() << "MainWindow::setShowOther - Automatically restarting monitoring to apply changes";
            updateStatusLabel(tr("Filter changed - automatically applying changes"));
            restartMonitoring();
        }
    } else {
        qDebug() << "MainWindow::setShowOther - Value unchanged, no action taken";
    }
}

void MainWindow::setShowNPCNames(bool show) {
    qDebug() << "MainWindow::setShowNPCNames - Value changing from" << showNPCNames << "to" << show;
    if (showNPCNames != show) {
        showNPCNames = show;
        settings.setValue("LogDisplay/ShowNPCNames", showNPCNames);
        qDebug() << "MainWindow::setShowNPCNames - Saved new value to QSettings";
        emit filterSettingsChanged();
        qDebug() << "MainWindow::setShowNPCNames - Emitted filterSettingsChanged signal";
        
        if (isMonitoring) {
            qDebug() << "MainWindow::setShowNPCNames - Automatically restarting monitoring to apply changes";
            updateStatusLabel(tr("Filter changed - automatically applying changes"));
            restartMonitoring();
        }
    }
}

// Add a helper method to restart monitoring
void MainWindow::restartMonitoring() {
    if (isMonitoring) {
        stopMonitoring();
        // Use a short delay to ensure everything is cleaned up before restarting
        QTimer::singleShot(100, this, &MainWindow::startMonitoring);
    }
}

void MainWindow::onGameFolderChanged(const QString& newFolder) {
    qDebug() << "MainWindow::onGameFolderChanged - New game folder:" << newFolder;

    // Update the game folder path
    gameLogFilePath = newFolder + "/game.log";
    qDebug() << "Game log file path updated to:" << gameLogFilePath;

    // Check if the file exists
    if (QFile::exists(gameLogFilePath)) {
        qDebug() << "Game log file found, detecting game mode and player info...";

        // Perform detection and unpack the tuple
        auto [mode, subMode, playerName, playerGEID] = detectGameModeAndPlayerFast(gameLogFilePath.toStdString());
        currentGameMode = mode;
        currentSubGameMode = subMode;

        qDebug() << "Detected game mode:" << QString::fromStdString(currentGameMode)
                 << "Sub-mode:" << QString::fromStdString(currentSubGameMode)
                 << "Player name:" << QString::fromStdString(playerName)
                 << "Player GEID:" << QString::fromStdString(playerGEID);

        // Update transmitter with player info if available
        transmitter.setGameLogFilePath(gameLogFilePath); // STRATEGY 2: Set log file path for rescans
        if (!playerName.empty() && !playerGEID.empty()) {
            transmitter.updatePlayerInfo(QString::fromStdString(playerName), QString::fromStdString(playerGEID));
        }

        // Update UI with detected game mode
        if (currentGameMode == "PU") {
            updateStatusLabel(tr("Game mode: Persistent Universe"));
        } else if (currentGameMode == "AC") {
            updateStatusLabel(tr("Game mode: Arena Commander: %1").arg(QString::fromStdString(currentSubGameMode)));
        } else {
            updateStatusLabel(tr("Game mode: Unknown"));
        }
    } else {
        qDebug() << "Game log file not found, cannot detect game mode:" << gameLogFilePath;
    }

    // If monitoring is active, restart it
    if (isMonitoring) {
        qDebug() << "MainWindow::onGameFolderChanged - Restarting monitoring with new game folder.";
        stopMonitoring();
        startMonitoring();
    } else {
        qDebug() << "MainWindow::onGameFolderChanged - Monitoring is not active, ready to new folder.";
        updateStatusLabel(tr("Game folder updated. Ready to start monitoring."));
    }

    // Notify LogDisplayWindow if it exists
    if (logDisplayWindow) {
        logDisplayWindow->updateGameFolder(newFolder);
    }
}

void MainWindow::initializeGameMode() {
    if (!gameLogFilePath.isEmpty() && QFile::exists(gameLogFilePath)) {
        qDebug() << "Detecting game mode from log file:" << gameLogFilePath;
        
        // Use detectGameModeAndPlayerFast instead of detectLastGameMode
        auto [mode, subMode, playerName, playerGEID] = detectGameModeAndPlayerFast(gameLogFilePath.toStdString());
        
        // Fix: Assign std::string values directly without conversion
        currentGameMode = mode;
        currentSubGameMode = subMode;
        
        // Update transmitter with game mode info
        transmitter.setGameLogFilePath(gameLogFilePath); // STRATEGY 2: Set log file path for rescans
        transmitter.updateGameMode(QString::fromStdString(mode), 
                                  QString::fromStdString(subMode), 
                                  false);
        
        // Update transmitter with player info if available - ADD THIS DEBUG INFO
        if (!playerName.empty() && !playerGEID.empty()) {
            qDebug() << "PLAYER INFO FOUND - Updating Transmitter with:" 
                     << QString::fromStdString(playerName)
                     << "GEID:" << QString::fromStdString(playerGEID);
                     
            
            // Force the player info update with explicit debug output
            transmitter.updatePlayerInfo(QString::fromStdString(playerName), 
                                        QString::fromStdString(playerGEID));
                                        
        
            // Verify the player info was updated
            qDebug() << "After update, Transmitter player name is:" 
                     << transmitter.getCurrentPlayerName();
        } else {
            qDebug() << "WARNING: No player info found in log file!";
        }
        
        // Rest of the method remains the same...
    }
}

void MainWindow::initializeApp() {
    qDebug() << "Initializing application...";

    // Load regex rules (needed for game mode detection)
    loadRegexRules();
    
    // STRATEGY 2: Set log file path in transmitter for game mode rescans
    transmitter.setGameLogFilePath(gameLogFilePath);

    // Perform initial game mode and player info detection
    logMonitor->initializeGameMode();

    // Start continuous log monitoring for future updates
    logMonitor->startUnifiedMonitoring(gameLogFilePath);

    // Connect signals for game mode and player info changes
    connect(logMonitor, &LogMonitor::gameModeSwitched, this, &MainWindow::handleGameModeChange);
    connect(logMonitor, &LogMonitor::playerInfoDetected, &transmitter, &Transmitter::updatePlayerInfo);

    qDebug() << "Application initialization complete.";
}

void MainWindow::handleGameModeChange(const QString& gameMode, const QString& subGameMode) {
    // Add a static variable to track the last game mode update
    static QString lastGameMode = "";
    static QString lastSubGameMode = "";
    
    // Only process if this is a different game mode than the last one processed
    if (gameMode != lastGameMode || subGameMode != lastSubGameMode) {
        // Remember this game mode to prevent duplicates
        lastGameMode = gameMode;
        lastSubGameMode = subGameMode;  // This should be the 'subGameMode' variable passed to the method as an argument.
        
        // Only send the game mode update if monitoring is active
        if (isMonitoring) {
            QString apiKey = settings.value("apiKey", "").toString();
            if (!apiKey.isEmpty()) {
                transmitter.sendGameMode(apiKey);
            }
        }
        
        // Update the game mode in the transmitter
        transmitter.setGameLogFilePath(gameLogFilePath); // STRATEGY 2: Ensure log file path is current
        transmitter.updateGameMode(gameMode, subGameMode, isMonitoring);
        
        // STANDBY MODE: Check if we should transition from standby to active monitoring
        if (isInStandbyMode && transmitter.isGameModeValid()) {
            QString apiKey = settings.value("apiKey", "").toString();
            if (!apiKey.isEmpty()) {
                qDebug() << "STANDBY TRANSITION: Valid game mode detected, attempting to start full monitoring";
                updateStatusLabel(tr("Game mode detected! Transitioning to full monitoring..."));
                QTimer::singleShot(1000, this, &MainWindow::exitStandbyMode);
            } else {
                qDebug() << "STANDBY: Game mode valid but no API key available";
                updateStatusLabel(tr("Standby: Game mode detected, but API key needed for full monitoring"));
            }
        }
        
        // If LogDisplayWindow is open, update its status directly
        if (logDisplayWindow) {
            if (gameMode == "PU") {
                logDisplayWindow->updateStatusLabel(tr("Game mode changed to Persistent Universe"));
            } else if (gameMode == "AC") {
                logDisplayWindow->updateStatusLabel(tr("Game mode changed to Arena Commander: %1").arg(subGameMode));
            }
        }
    } else {
        qDebug() << "Ignoring duplicate game mode change notification for" << gameMode << subGameMode;
    }
}

// Add the connection ping method
void MainWindow::sendConnectionPing() {
    if (!isMonitoring) {
        return;  // Safety check
    }

    qDebug() << "Sending connection heartbeat ping";
    QString apiKey = settings.value("apiKey", "").toString();

    if (apiKey.isEmpty()) {
        qWarning() << "Cannot send connection ping: API key is empty";
        return;
    }

    // Check if we are still in the cool-off period
    QDateTime now = QDateTime::currentDateTimeUtc();
    QDateTime nextAllowed = getNextAllowedPingTime();
    if (nextAllowed.isValid() && now < nextAllowed) {
        qDebug() << "Skipping connection ping: still in cool-off period until" << nextAllowed.toString(Qt::ISODate);
        return;
    }

    bool pingSuccess = transmitter.sendDebugPing(apiKey);

    // Only treat as a failure if the ping was actually attempted and failed due to network/server error
    // If pingSuccess is false but we are still in the cooldown, do not log or update status
    if (!pingSuccess) {
        // Check again if we are still in cooldown (defensive, in case transmitter logic changes)
        QDateTime nextAllowedAfter = getNextAllowedPingTime();
        if (nextAllowedAfter.isValid() && QDateTime::currentDateTimeUtc() < nextAllowedAfter) {
            qDebug() << "Debug ping was skipped due to cooldown, not a real failure.";
            return;
        }
        qWarning() << "Connection ping failed - API server may be unreachable";
        QString errorMessage = tr("API connection error detected during routine check");
        transmitter.handleNetworkError(QNetworkReply::ConnectionRefusedError, errorMessage);
    } else {
        qDebug() << "Connection ping successful - API connection is healthy";
    }
}

// Add this method to create the LogDisplayWindow after initialization
void MainWindow::createLogDisplayWindowIfNeeded() {
    bool shouldBeVisible = settings.value("LogDisplay/Visible", false).toBool();
    
    if (shouldBeVisible && !logDisplayWindow) {
        qDebug() << "Creating LogDisplayWindow after initialization complete";
        logDisplayWindow = new LogDisplayWindow(transmitter);
        logDisplayWindow->setAttribute(Qt::WA_DeleteOnClose);
        logDisplayWindow->setWindowFlag(Qt::Window);

        // Apply the current theme to all windows (including the new LogDisplayWindow)
        ThemeManager::instance().applyCurrentThemeToAllWindows();

        // Connect LogDisplayWindow signals
        connect(logDisplayWindow, &LogDisplayWindow::windowClosed, this, [this]() {
            qDebug() << "LogDisplayWindow closed signal received.";
            
            // Save the closed state using class member
            settings.setValue("LogDisplay/Visible", false);
            logDisplayVisible = false;
            
            logDisplayWindow = nullptr;
            logButton->setText(tr("Show Log Window"));
        });
        
        connect(logDisplayWindow, &LogDisplayWindow::toggleMonitoringRequested, 
                this, &MainWindow::handleMonitoringToggleRequest);
                
        connect(logDisplayWindow, &LogDisplayWindow::filterPvPChanged, 
                this, [this](bool show) {
                    setShowPvP(show);
                });
                
        connect(logDisplayWindow, &LogDisplayWindow::filterPvEChanged, 
                this, [this](bool show) {
                    setShowPvE(show);
                });
                
        connect(logDisplayWindow, &LogDisplayWindow::filterShipsChanged,
                this, [this](bool show) {
                    setShowShips(show);
                });

        connect(logDisplayWindow, &LogDisplayWindow::filterOtherChanged,
                this, [this](bool show) {
                    setShowOther(show);
                });
                
        connect(logDisplayWindow, &LogDisplayWindow::filterNPCNamesChanged, 
                this, [this](bool show) {
                    setShowNPCNames(show);
                });

        // Update monitoring button state
        logDisplayWindow->updateMonitoringButtonText(isMonitoring);

        logDisplayWindow->show();
        logButton->setText(tr("Hide Log Window"));

        // Force an immediate title update with current values
        QString gameMode = transmitter.getCurrentGameMode();
        QString subGameMode = transmitter.getCurrentSubGameMode();
        QString playerName = transmitter.getCurrentPlayerName();
        
        qDebug() << "Forcing initial title update with data:"
                 << "Game Mode:" << gameMode
                 << "Sub-game Mode:" << subGameMode
                 << "Player Name:" << playerName;
                 
        QTimer::singleShot(500, this, [this, gameMode, subGameMode]() {
            if (logDisplayWindow) {
                logDisplayWindow->updateWindowTitle(gameMode, subGameMode);
            }
        });
    } else {
        logButton->setText(tr("Show Log Window"));
    }
}

// Add this method to MainWindow class:
void MainWindow::emitInitializationComplete() {
    // Create and show LogDisplayWindow if it was previously visible
    createLogDisplayWindowIfNeeded();
    
    // Then emit the signal
    emit initializationComplete();
}

// Add this method to MainWindow class:
void MainWindow::startBackgroundInitialization() {
    // Create worker thread for initialization
    QThread* initThread = new QThread();
    QObject* worker = new QObject();
    worker->moveToThread(initThread);
    
    // Connect to perform work when thread starts
    connect(initThread, &QThread::started, worker, [this, worker, initThread]() {
        // Load regex rules (10% progress)
        emit initializationProgress(40, "Loading regex rules...");
        loadRegexRules();
        
        // Initialize game mode (30% progress)
        emit initializationProgress(50, "Detecting game mode...");
        initializeGameMode();
        
        // Set up log monitor (20% progress)
        emit initializationProgress(70, "Setting up log monitoring...");
        logMonitor->startGameModeTracking(gameLogFilePath);
        
        // Connect remaining signals (10% progress)
        emit initializationProgress(90, "Finalizing setup...");
        connect(logMonitor, &LogMonitor::gameModeSwitched, this, &MainWindow::handleGameModeChange);
        connect(logMonitor, &LogMonitor::playerInfoDetected, &transmitter, &Transmitter::updatePlayerInfo);
        
        // Final setup steps
        emit initializationProgress(95, "Ready!");
        
        // Clean up worker and thread
        worker->deleteLater();
        initThread->quit();
        
        // Signal completion on the main thread
        QMetaObject::invokeMethod(this, "emitInitializationComplete", Qt::QueuedConnection);
    });
    


    // Clean up thread when done
    connect(initThread, &QThread::finished, initThread, &QThread::deleteLater);
    
    // Start the worker thread
    initThread->start();
}

// Add this method to the MainWindow class
void MainWindow::retranslateUi() {
    // Update window title
    setWindowTitle(tr("Gunhead Connect"));
    
    // Update button texts
    startButton->setText(isMonitoring ? tr("Stop Monitoring") : tr("Start Monitoring"));
    settingsButton->setText(tr("Settings"));
    
    // Fix the log button text based on the current state
    if (logDisplayWindow) {
        logButton->setText(tr("Hide Log Window"));
    } else {
        logButton->setText(tr("Show Log Window"));
    }
    
    // Update system tray menu if it exists
    if (trayIconMenu) {
        if (showHideAction) showHideAction->setText(tr(isVisible() ? "Hide Main Window" : "Show Main Window"));
        if (showLogDisplayAction) showLogDisplayAction->setText(tr(logDisplayWindow && logDisplayWindow->isVisible() ? "Hide Log Window" : "Show Log Window"));
        if (startStopAction) startStopAction->setText(isMonitoring ? tr("Stop Monitoring") : tr("Start Monitoring"));
        if (exitAction) exitAction->setText(tr("Exit"));
        
        if (systemTrayIcon) {
            systemTrayIcon->setToolTip(tr("Gunhead Connect"));
        }
    }
    
    // Retranslate the current status message if we have one
    if (!lastStatusKey.isEmpty() && statusLabel) {
        statusLabel->setText(tr(lastStatusKey.toUtf8().constData()));
        qDebug() << "Status label retranslated to:" << statusLabel->text() 
                 << "from key:" << lastStatusKey;
    }
    
    qDebug() << "MainWindow UI retranslated";
}

void MainWindow::updateLogDisplayFilterWidth() {
    if (logDisplayWindow) {
        QTimer::singleShot(100, logDisplayWindow, &LogDisplayWindow::updateFilterDropdownWidth);
    }
}

// System tray functionality
// filepath: src/MainWindow.cpp
// ADDED: Robust system tray message function with details support
void MainWindow::showSystemTrayMessage(const QString& title, const QString& message, const QString& details) {
    if (systemTrayIcon && systemTrayIcon->isVisible()) {
        QString fullMessage = message;
        if (!details.isEmpty()) {
            fullMessage += "\n" + details;
        }
        systemTrayIcon->showMessage(title, fullMessage, QSystemTrayIcon::Information, 5000);
        lastGrowlDetails = details;
    }
}
void MainWindow::createSystemTrayIcon() {
    // Check if icon already exists to prevent duplicates
    if (systemTrayIcon) {
        qDebug() << "System tray icon already exists, skipping creation";
        return;
    }

    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        qWarning() << "System tray unavailable";
        return;
    }

    // Create the tray menu with no parent to avoid inheriting QSS
    trayIconMenu = new QMenu(nullptr);
    trayIconMenu->setObjectName("TrayMenu"); // ADDED: for QSS targeting

    // Apply ONLY the "originalsleek" theme to the tray menu
    QFile sleekQss(":/themes/originalsleek.qss");
    if (sleekQss.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QString sleekStyle = QString::fromUtf8(sleekQss.readAll());
        trayIconMenu->setStyleSheet(sleekStyle);
        qDebug() << "Applied 'originalsleek' theme to tray menu";
    } else {
        trayIconMenu->setStyleSheet("");
        qWarning() << "Could not load 'originalsleek.qss', tray menu will use default style";
    }

    // Start/Stop monitoring action
    startStopAction = new QAction(tr("Start Monitoring"), this);
    connect(startStopAction, &QAction::triggered, this, &MainWindow::onSystemTrayStartStopClicked);
    trayIconMenu->addAction(startStopAction);

    trayIconMenu->addSeparator();
    // Show/Hide action
    showHideAction = new QAction(tr(isVisible() ? "Hide Main Window" : "Show Main Window"), this);
    connect(showHideAction, &QAction::triggered, this, &MainWindow::onSystemTrayShowHideClicked);
    trayIconMenu->addAction(showHideAction);
    
    // ADDED: Show/Hide LogDisplayWindow action
    bool logDisplayIsVisible = logDisplayWindow && logDisplayWindow->isVisible();
    showLogDisplayAction = new QAction(tr(logDisplayIsVisible ? "Hide Log Window" : "Show Log Window"), this);
    connect(showLogDisplayAction, &QAction::triggered, this, &MainWindow::onSystemTrayShowLogDisplayClicked);
    trayIconMenu->addAction(showLogDisplayAction);

    trayIconMenu->addSeparator();

    // Exit action
    exitAction = new QAction(tr("Exit"), this);
    connect(exitAction, &QAction::triggered, this, &MainWindow::onSystemTrayExitClicked);
    trayIconMenu->addAction(exitAction);

    // Create system tray icon
    systemTrayIcon = new QSystemTrayIcon(this);
    systemTrayIcon->setContextMenu(trayIconMenu);

    // Try to load the dedicated tray PNG first
    const QString trayPngPath = ":/icons/Gunhead-tray.png";
    QIcon trayPngIcon(trayPngPath);
    qDebug() << "Attempting to load tray icon from" << trayPngPath
             << "isNull=" << trayPngIcon.isNull()
             << "availableSizes=" << trayPngIcon.availableSizes();

    if (!trayPngIcon.isNull()) {
        systemTrayIcon->setIcon(trayPngIcon);
        qDebug() << "System tray icon set from Gunhead-tray.png";
    } else {
        // Fallback: draw a branded 'K' on transparent 16×16
        qWarning() << "Failed to load" << trayPngPath << "- falling back to letter icon";
        QPixmap pixmap(16, 16);
        pixmap.fill(Qt::transparent);

        QPainter p(&pixmap);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(QColor("#cefc19")); // brand color
        p.setFont(QFont("Consolas", 10, QFont::Bold));
        p.drawText(pixmap.rect(), Qt::AlignCenter, "K");
        p.end();

        systemTrayIcon->setIcon(QIcon(pixmap));
        qDebug() << "Fallback 'K' icon set with brand color #cefc19";
    }

    systemTrayIcon->setToolTip(tr("Gunhead Connect"));
    connect(systemTrayIcon, &QSystemTrayIcon::activated,
            this, &MainWindow::onSystemTrayActivated);

    // ADDED: Connect growl (balloon) message click to handler
    connect(systemTrayIcon, &QSystemTrayIcon::messageClicked,
            this, &MainWindow::onSystemTrayMessageClicked);

    systemTrayIcon->show();
    qDebug() << "Tray icon show() called; isVisible() =" << systemTrayIcon->isVisible();
}


void MainWindow::toggleMonitoring() {
    if (isMonitoring) {
        stopMonitoring();
    } else if (isInStandbyMode) {
        // If we're in standby mode, stop standby and don't start full monitoring
        stopMonitoring();
    } else {
        startMonitoring();
    }
}

bool MainWindow::getMonitoringState() const {
    return isMonitoring;
}

bool MainWindow::getStandbyState() const {
    return isInStandbyMode;
}

void MainWindow::onSystemTrayActivated(QSystemTrayIcon::ActivationReason reason) {
    if (reason == QSystemTrayIcon::DoubleClick || reason == QSystemTrayIcon::Trigger) {
        // Robustly restore and focus the app window
        if (isMinimized()) {
            showNormal();
        }
        if (!isVisible()) {
            show();
        }
        raise();
        activateWindow();
        #ifdef Q_OS_WIN
        #include <windows.h>
        SetForegroundWindow((HWND)winId());
        #endif
        setWindowState(windowState() & ~Qt::WindowMinimized);
        if (!lastGrowlDetails.isEmpty()) {
            updateStatusLabel(lastGrowlDetails);
        }
    }
}

// MODIFIED: Connect messageClicked to a handler that restores the main window
void MainWindow::onSystemTrayMessageClicked() {
    // Robustly restore and focus the app window
    if (isMinimized()) {
        showNormal();
    }
    if (!isVisible()) {
        show();
    }
    raise();
    activateWindow();
#ifdef Q_OS_WIN
    SetForegroundWindow((HWND)winId());
#endif
    setWindowState(windowState() & ~Qt::WindowMinimized);
    if (!lastGrowlDetails.isEmpty()) {
        updateStatusLabel(lastGrowlDetails);
    }
}

void MainWindow::onMinimizeToTrayChanged(bool enabled) {
    // Store the setting for use in closeEvent
    QSettings settings("KillApiConnect", "KillApiConnectPlus");
    settings.setValue("minimizeToTray", enabled);
    
    qDebug() << "Minimize to tray setting changed to:" << enabled;
}

void MainWindow::onSystemTrayStartStopClicked() {
    toggleMonitoring();
    // Note: Action text is already updated by startMonitoring()/stopMonitoring() methods
}

void MainWindow::onSystemTrayShowHideClicked() {
    if (isVisible()) {
        hide();
        showHideAction->setText(tr("Show Main Window"));
    } else {
        show();
        raise();
        activateWindow();
        showHideAction->setText(tr("Hide Main Window"));
    }
}

void MainWindow::onSystemTrayShowLogDisplayClicked() {
    // Use the existing toggleLogDisplayWindow method to ensure consistency
    toggleLogDisplayWindow();
    
    // Update the tray menu action text to match the current state
    if (showLogDisplayAction) {
        if (logDisplayWindow && logDisplayWindow->isVisible()) {
            showLogDisplayAction->setText(tr("Hide Log Window"));
        } else {
            showLogDisplayAction->setText(tr("Show Log Window"));
        }
    }
}

void MainWindow::onSystemTrayExitClicked() {
    isShuttingDown = true;
    
    // Hide and clean up the tray icon before quitting
    if (systemTrayIcon) {
        systemTrayIcon->hide();
        systemTrayIcon->deleteLater();
        systemTrayIcon = nullptr;
    }
    
    QApplication::quit();
}

void MainWindow::activateFromAnotherInstance() 
{
    // If window is visible but minimized, restore it
    if (isVisible() && isMinimized()) {
        showNormal();
    }
    // If hidden (in system tray), show it
    else if (!isVisible()) {
        show();
    }
    
    // Bring to front
    raise();
    activateWindow();
    
    // If we're in system tray mode, also update the tray icon
    if (systemTrayIcon) {
        systemTrayIcon->setToolTip(tr("Gunhead Connect (Active)"));
    }
}

// Helper method to get the next allowed debug ping time from the Transmitter
QDateTime MainWindow::getNextAllowedPingTime() const {
    return transmitter.getNextAllowedPingTime();
}