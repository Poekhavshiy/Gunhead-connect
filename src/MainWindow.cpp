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
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

MainWindow::MainWindow(QWidget* parent)
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
    setWindowTitle("KillAPI Connect Plus");
    setWindowIcon(QIcon(":/icons/KillApi.ico"));
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
    
    // When creating the status label
    statusLabel = new QLabel(tr("Ready"), central);
    statusLabel->setObjectName("statusLabel"); // Add this line
    statusLabel->setFixedSize(statusLabelSize);
    statusLabel->setWordWrap(true); // Enable word wrapping for multi-line display
    statusLabel->setAlignment(Qt::AlignCenter);

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

    // Restore LogDisplayWindow visibility
    if (logDisplayVisible) {
        // Previous session had LogDisplayWindow open, restore it
        qDebug() << "Restoring LogDisplayWindow from previous session.";
        logDisplayWindow = new LogDisplayWindow(transmitter); // Pass the Transmitter instance
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

        logDisplayWindow->show(); // Show the window
        qDebug() << "LogDisplayWindow restored and shown.";
        logButton->setText("Hide Log"); // Update button label
    } else {
        // Previous session had LogDisplayWindow closed, keep it closed
        qDebug() << "LogDisplayWindow was closed in the previous session, keeping it closed.";
        logButton->setText("View Log"); // Default label
    }

    // Connect button signals
    connect(settingsButton, &QPushButton::clicked,
            this, &MainWindow::toggleSettingsWindow);
    connect(logButton, &QPushButton::clicked,
            this, &MainWindow::toggleLogDisplayWindow);
    connect(startButton, &QPushButton::clicked,
            this, &MainWindow::startMonitoring);
    connect(logMonitor, &LogMonitor::logLineMatched, this, &MainWindow::handleLogLine);
    connect(logMonitor, &LogMonitor::monitoringStopped, this, [this](const QString& reason) {
        updateStatusLabel(tr("Monitoring stopped:\n") + reason);
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

void MainWindow::startMonitoring() {
    qDebug() << "MainWindow: Starting log monitoring.";
    if (!QFile::exists(gameLogFilePath)) {
        qWarning() << "Game log file does not exist:" << gameLogFilePath;
        qDebug() << "Please check the game folder path in settings.";
        updateStatusLabel(tr("Error: Game log file not found."));
        return;
    }

    // Debug output current filter settings before starting monitoring
    QSettings settings("KillApiConnect", "KillApiConnectPlus");
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

    // Update the button label and connect it to stopMonitoring
    startButton->setText("Stop Monitoring");
    disconnect(startButton, &QPushButton::clicked, this, &MainWindow::startMonitoring);
    connect(startButton, &QPushButton::clicked, this, &MainWindow::stopMonitoring);

    // Update LogDisplayWindow button if it exists
    isMonitoring = true;
    if (logDisplayWindow) {
        logDisplayWindow->updateMonitoringButtonText(true);
    }

    updateStatusLabel(tr("Monitoring started."));
}

void MainWindow::stopMonitoring() {
    qDebug() << "MainWindow: Stopping log monitoring.";
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

void MainWindow::toggleSettingsWindow()
{
    if (settingsWindow && settingsWindow->isVisible()) {
        settingsWindow->close();
    } else {
        if (!settingsWindow) {
            settingsWindow = new SettingsWindow(this);
            connect(settingsWindow, &SettingsWindow::settingsChanged,
                    this, &MainWindow::applyTheme);
        }
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
    
    // Bring MainWindow to the front
    raise();
    activateWindow();
    
    // If LogDisplayWindow exists, make it visible again
    if (logDisplayWindow && logDisplayWindow->isVisible()) {
        // First activate LogDisplayWindow to bring it forward
        logDisplayWindow->setWindowState(logDisplayWindow->windowState() & ~Qt::WindowMinimized);
        logDisplayWindow->raise();
        logDisplayWindow->activateWindow();
        
        // Then reactivate MainWindow to get focus back
        QTimer::singleShot(100, this, [this]() {
            this->raise();
            this->activateWindow();
        });
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
        
        // Check if we need to restart monitoring for changes to take effect
        if (isMonitoring) {
            qDebug() << "MainWindow::setShowPvP - Monitoring is active, require restart to apply";
            updateStatusLabel(tr("Filter changed: Restart monitoring to apply"));
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
        
        // Check if we need to restart monitoring for changes to take effect
        if (isMonitoring) {
            qDebug() << "MainWindow::setShowPvE - Monitoring is active, require restart to apply";
            updateStatusLabel(tr("Filter changed: Restart monitoring to apply"));
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
        
        // Check if we need to restart monitoring for changes to take effect
        if (isMonitoring) {
            qDebug() << "MainWindow::setShowNPCNames - Monitoring is active, require restart to apply";
            updateStatusLabel(tr("Filter changed: Restart monitoring to apply"));
        }
    } else {
        qDebug() << "MainWindow::setShowNPCNames - Value unchanged, no action taken";
    }
}
