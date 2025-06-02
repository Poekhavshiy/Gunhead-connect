#include "MainWindow.h"
#include "SettingsWindow.h"
#include "LogDisplayWindow.h"
#include "ThemeSelect.h"
#include "LogMonitor.h"
#include "Transmitter.h"
#include "log_parser.h"
#include <QVBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QSettings>
#include <QDebug>
#include <QCloseEvent>
#include <QFile>
#include <QTimer>
#include <QThread>  // Add this line
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
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
    , showNPCNames(true)
{
    // Set up the main window
    setWindowTitle("KillAPI Connect Plus");
    setWindowIcon(QIcon(":/app_icon"));
    // Load the game log file path from QSettings
    QSettings settings("KillApiConnect", "KillApiConnectPlus");
    gameLogFilePath = settings.value("gameFolder", "").toString() + "/game.log";
    logDisplayVisible = settings.value("LogDisplay/Visible", false).toBool();

    // Load filter settings from QSettings
    showPvP = settings.value("LogDisplay/ShowPvP", false).toBool();
    showPvE = settings.value("LogDisplay/ShowPvE", true).toBool();
    showNPCNames = settings.value("LogDisplay/ShowNPCNames", true).toBool();

    
    // Load the current theme directly
    ThemeSelectWindow themeSelect;
    Theme currentTheme = themeSelect.loadCurrentTheme();

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
    int buttonRightSpace = settings.value("mainButtonRightSpace", 140).toInt();
    qDebug() << "Button right spacing:" << buttonRightSpace;

    // Create buttons and set their fixed width
    startButton = new QPushButton(tr("Start Monitoring"), central);
    startButton->setFixedWidth(buttonWidth);

    settingsButton = new QPushButton(tr("Settings"), central);
    settingsButton->setFixedWidth(buttonWidth);

    logButton = new QPushButton(tr("View Log"), central);
    logButton->setFixedWidth(buttonWidth);

    QSize statusLabelSize = settings.value("statusLabelPreferredSize", QSize(280, 80)).toSize();
    
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
        
    //debugPaths();
    debugPaths();
    // Load regex rules
    loadRegexRules();

    

    // Apply the complete theme at startup
    currentTheme.backgroundImage = settings.value("currentBackgroundImage", "").toString();
    applyTheme(currentTheme);


    // Add queue processing timer
    QTimer* queueProcessTimer = new QTimer(this);
    connect(queueProcessTimer, &QTimer::timeout, this, [this]() {
        if (!transmitter.logQueue.isEmpty()) {
            QSettings settings("KillApiConnect", "KillApiConnectPlus");
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
        
        // Stop monitoring for specific errors
        if (errorType == Transmitter::ApiErrorType::PanelClosed || 
            errorType == Transmitter::ApiErrorType::InvalidApiKey) {
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

    // Connect LogMonitor signals to Transmitter slots
    connect(logMonitor, &LogMonitor::gameModeSwitched, 
        &transmitter, [this](const QString& gameMode, const QString& subGameMode) {
            transmitter.updateGameMode(gameMode, subGameMode, true);
        });

    connect(logMonitor, &LogMonitor::playerInfoDetected,
        &transmitter, [this](const QString& playerName, const QString& playerGEID) {
            transmitter.updatePlayerInfo(playerName, playerGEID);
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
    
    QSettings settings("KillApiConnect", "KillApiConnectPlus");
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
    
    // Send debug ping to verify connection
    bool pingSuccess = transmitter.sendDebugPing(apiKey);
    
    if (!pingSuccess) {
        qWarning() << "API connection verification failed. Cannot start monitoring.";
        updateStatusLabel(tr("Error: Could not connect to KillAPI server. Monitoring not started."));
        
        // Show more detailed error in LogDisplayWindow if it's open
        if (logDisplayWindow) {
            logDisplayWindow->updateStatusLabel(tr("ERROR: Failed to connect to KillAPI server. Please check your API key and internet connection."));
        }
        return false;
    }
    
    qDebug() << "API connection verified successfully. Starting monitoring...";
    
    // Send full connection success event after successful ping
    bool connectionSuccess = transmitter.sendConnectionSuccess(gameLogFilePath, apiKey);
    if (!connectionSuccess) {
        qWarning() << "Failed to send connection success event, but will continue with monitoring.";
    } else {
         if (logDisplayWindow) {
            logDisplayWindow->updateStatusLabel(tr("Connected to KillAPI server successfully. Monitoring started."));
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
    startButton->setText("Connecting...");
    
    // Also update LogDisplayWindow button if it exists
    if (logDisplayWindow) {
        logDisplayWindow->disableMonitoringButton("Connecting...");
    }
    
    QSettings settings("KillApiConnect", "KillApiConnectPlus");
    QString apiKey = settings.value("apiKey", "").toString();

    if (!QFile::exists(gameLogFilePath)) {
        qWarning() << "Game log file does not exist:" << gameLogFilePath;
        updateStatusLabel(tr("Error: Game log file not found."));
        
        // Re-enable buttons on error
        startButton->setEnabled(true);
        startButton->setText("Start Monitoring");
        if (logDisplayWindow) {
            logDisplayWindow->enableMonitoringButton("Start Monitoring");
        }
        return;
    }
    
    // Verify API connection before starting monitoring
    if (!verifyApiConnectionAndStartMonitoring(apiKey)) {
        // Re-enable buttons on failed connection
        startButton->setEnabled(true);
        startButton->setText("Start Monitoring");
        if (logDisplayWindow) {
            logDisplayWindow->enableMonitoringButton("Start Monitoring");
        }
        return; // Don't start monitoring if verification failed
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
    startButton->setText("Stop Monitoring");
    disconnect(startButton, &QPushButton::clicked, this, &MainWindow::startMonitoring);
    connect(startButton, &QPushButton::clicked, this, &MainWindow::stopMonitoring);

    // Update LogDisplayWindow button if it exists
    isMonitoring = true;
    if (logDisplayWindow) {
        logDisplayWindow->updateMonitoringButtonText(true);
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
    
    qDebug() << "MainWindow::stopMonitoring - Stopping log monitoring.";
    if (logMonitor) {
        logMonitor->stopMonitoring();
    }

    // Update the button label and reconnect it to startMonitoring
    startButton->setText("Start Monitoring");
    disconnect(startButton, &QPushButton::clicked, this, &MainWindow::stopMonitoring);
    connect(startButton, &QPushButton::clicked, this, &MainWindow::startMonitoring);

    // Update LogDisplayWindow button if it exists
    isMonitoring = false;
    if (logDisplayWindow) {
        logDisplayWindow->updateMonitoringButtonText(false);
    }

    updateStatusLabel(tr("Monitoring stopped."));
    isStoppingMonitoring = false;
}

void MainWindow::toggleLogDisplayWindow(bool forceNotVisible) {
    qDebug() << "Toggling LogDisplayWindow visibility.";
    QSettings settings("KillApiConnect", "KillApiConnectPlus");

    if (forceNotVisible || logDisplayWindow) {
        qDebug() << "LogDisplayWindow is open, closing it.";
        settings.setValue("LogDisplay/Visible", false); // Update state in QSettings
        logDisplayVisible = false;
        if (logDisplayWindow) {
            logDisplayWindow->close();
            logDisplayWindow = nullptr; // Reset the pointer
        }
        logButton->setText("View Log"); // Update button label
    } else {
        qDebug() << "LogDisplayWindow is closed, opening it.";
        logDisplayWindow = new LogDisplayWindow(transmitter);
        logDisplayWindow->setAttribute(Qt::WA_DeleteOnClose);
        logDisplayWindow->setWindowFlag(Qt::Window);

        // Connect LogDisplayWindow signals
        connect(logDisplayWindow, &LogDisplayWindow::windowClosed, this, [this]() {
            qDebug() << "LogDisplayWindow closed signal received.";
            toggleLogDisplayWindow(true);
        });
        
        connect(logDisplayWindow, &LogDisplayWindow::toggleMonitoringRequested, 
                this, &MainWindow::handleMonitoringToggleRequest);
                
        connect(logDisplayWindow, &LogDisplayWindow::filterPvPChanged, 
                this, [this](bool show) {
                    qDebug() << "MainWindow: Received filterPvPChanged signal with value:" << show;
                    setShowPvP(show);
                });
                
        connect(logDisplayWindow, &LogDisplayWindow::filterPvEChanged, 
                this, [this](bool show) {
                    qDebug() << "MainWindow: Received filterPvEChanged signal with value:" << show;
                    setShowPvE(show);
                });
                
        connect(logDisplayWindow, &LogDisplayWindow::filterNPCNamesChanged, 
                this, [this](bool show) {
                    qDebug() << "MainWindow: Received filterNPCNamesChanged signal with value:" << show;
                    setShowNPCNames(show);
                });

        // Update monitoring button state
        logDisplayWindow->updateMonitoringButtonText(
            startButton->text() == "Stop Monitoring");

        logDisplayWindow->show();
        settings.setValue("LogDisplay/Visible", true); // Update state in QSettings
        logDisplayVisible = true;
        logButton->setText("Hide Log"); // Update button label
    }
}

void MainWindow::toggleSettingsWindow() {
    if (settingsWindow) {
        // Check if window exists and is minimized
        if (settingsWindow->isMinimized()) {
            // Restore the window from minimized state
            settingsWindow->setWindowState(settingsWindow->windowState() & ~Qt::WindowMinimized);
            settingsWindow->raise();
            settingsWindow->activateWindow();
        } 
        // If visible and not minimized, close it
        else if (settingsWindow->isVisible()) {
            settingsWindow->close();
        }
        // If not visible (but exists), show it
        else {
            settingsWindow->show();
            settingsWindow->raise();
            settingsWindow->activateWindow();
        }
    } else {
        // Create new window if it doesn't exist
        settingsWindow = new SettingsWindow(this);
        connect(settingsWindow, &SettingsWindow::settingsChanged,
                this, &MainWindow::applyTheme);
        connect(settingsWindow, &SettingsWindow::gameFolderChanged, 
                this, &MainWindow::onGameFolderChanged);
        settingsWindow->show();
    }
}

void MainWindow::applyTheme(Theme themeData)
{
    qDebug() << "MainWindow resizing to:" << themeData.mainWindowPreferredSize << " and button right spacing:" << themeData.mainButtonRightSpace;
    setFixedSize(themeData.mainWindowPreferredSize); // Resize MainWindow
        
    // ALWAYS update the spacing, don't check QSettings
    qDebug() << "FORCE applying button spacing to:" << themeData.mainButtonRightSpace;
    
    // Debug buttonLayouts
    qDebug() << "buttonLayouts contains" << buttonLayouts.size() << "layouts";
    
    // Update all layouts regardless of condition
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
    
    // Force a complete layout update
    QSettings settings("KillApiConnect", "KillApiConnectPlus");
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
    if (logMonitor) {
        logMonitor->stopMonitoring(); // Gracefully stop monitoring
        updateStatusLabel(tr("Monitoring stopped."));
    }
    QSettings settings("KillApiConnect", "KillApiConnectPlus");
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
    QSettings settings("KillApiConnect", "KillApiConnectPlus");
    QString rulesFilePath = settings.value("regexRulesFile", "data/logfile_regex_rules.json").toString();

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
    QSettings settings("KillApiConnect", "KillApiConnectPlus");

    QString regexRulesFile = settings.value("regexRulesFile", "data/logfile_regex_rules.json").toString();
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
        statusLabel->setText(status);
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
        QSettings settings("KillApiConnect", "KillApiConnectPlus");
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
        QSettings settings("KillApiConnect", "KillApiConnectPlus");
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

void MainWindow::setShowNPCNames(bool show) {
    qDebug() << "MainWindow::setShowNPCNames - Value changing from" << showNPCNames << "to" << show;
    if (showNPCNames != show) {
        showNPCNames = show;
        QSettings settings("KillApiConnect", "KillApiConnectPlus");
        settings.setValue("LogDisplay/ShowNPCNames", showNPCNames);
        qDebug() << "MainWindow::setShowNPCNames - Saved new value to QSettings";
        emit filterSettingsChanged();
        qDebug() << "MainWindow::setShowNPCNames - Emitted filterSettingsChanged signal";
        
        // Automatically restart monitoring if active
        if (isMonitoring) {
            qDebug() << "MainWindow::setShowNPCNames - Automatically restarting monitoring to apply changes";
            updateStatusLabel(tr("Filter changed - automatically applying changes"));
            restartMonitoring();
        }
    } else {
        qDebug() << "MainWindow::setShowNPCNames - Value unchanged, no action taken";
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
    }
    
    // Only send the game mode update if monitoring is active
    if (isMonitoring) {
        QSettings settings("KillApiConnect", "KillApiConnectPlus");
        QString apiKey = settings.value("apiKey", "").toString();
        if (!apiKey.isEmpty()) {
            transmitter.sendGameMode(apiKey);
        }
    }
    
    // Update the game mode in the transmitter
    transmitter.updateGameMode(gameMode, subGameMode, isMonitoring);
}

// Add the connection ping method
void MainWindow::sendConnectionPing() {
    if (!isMonitoring) {
        return;  // Safety check
    }
    
    qDebug() << "Sending connection heartbeat ping";
    QSettings settings("KillApiConnect", "KillApiConnectPlus");
    QString apiKey = settings.value("apiKey", "").toString();
    
    if (apiKey.isEmpty()) {
        qWarning() << "Cannot send connection ping: API key is empty";
        return;
    }
    
    bool pingSuccess = transmitter.sendDebugPing(apiKey);
    
    if (!pingSuccess) {
        qWarning() << "Connection ping failed - API server may be unreachable";
        QString errorMessage = tr("API connection error detected during routine check");
        transmitter.handleNetworkError(QNetworkReply::ConnectionRefusedError, errorMessage);
    } else {
        qDebug() << "Connection ping successful - API connection is healthy";
    }
}

// Add this method to create the LogDisplayWindow after initialization
void MainWindow::createLogDisplayWindowIfNeeded() {
    QSettings settings("KillApiConnect", "KillApiConnectPlus");
    bool shouldBeVisible = settings.value("LogDisplay/Visible", false).toBool();
    
    if (shouldBeVisible && !logDisplayWindow) {
        qDebug() << "Creating LogDisplayWindow after initialization complete";
        logDisplayWindow = new LogDisplayWindow(transmitter);
        logDisplayWindow->setAttribute(Qt::WA_DeleteOnClose);
        logDisplayWindow->setWindowFlag(Qt::Window);

        // Connect the LogDisplayWindow's close signal to update the button label
        connect(logDisplayWindow, &LogDisplayWindow::windowClosed, this, [this]() {
            qDebug() << "LogDisplayWindow closed signal received.";
            logDisplayWindow = nullptr;
            logButton->setText("View Log");
        });

        // Connect the monitoring toggle signal
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
                
        connect(logDisplayWindow, &LogDisplayWindow::filterNPCNamesChanged, 
                this, [this](bool show) {
                    setShowNPCNames(show);
                });

        // Update monitoring button state
        logDisplayWindow->updateMonitoringButtonText(isMonitoring);

        logDisplayWindow->show();
        logButton->setText("Hide Log");

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
        logButton->setText("View Log");
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