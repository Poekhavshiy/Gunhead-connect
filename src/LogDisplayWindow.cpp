#include "globals.h"
#include "LogDisplayWindow.h"
#include "log_parser.h"
#include <QScrollBar>
#include <QDebug>
#include <QSettings>
#include <QColorDialog>
#include <QFile>
#include <QTextStream>
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QRandomGenerator>
#include <QTimer>
#include "FilterUtils.h"

LogDisplayWindow::LogDisplayWindow(Transmitter& transmitter, QWidget* parent)
    : QMainWindow(parent), transmitter(transmitter), logFontSize(12), logBgColor("#000000"), logFgColor("#FFFFFF") {
    // Load filter settings from QSettings
    loadFilterSettings();
    
    setupUI();

    // Load settings from QSettings
    QSettings settings("KillApiConnect", "KillApiConnectPlus");
    logFontSize = settings.value("LogDisplay/FontSize", 12).toInt();
    logBgColor = settings.value("LogDisplay/BackgroundColor", "#000000").toString();
    logFgColor = settings.value("LogDisplay/TextColor", "#FFFFFF").toString();
    QPoint windowPos = settings.value("LogDisplay/WindowPosition", QPoint(100, 100)).toPoint();
    QSize windowSize = settings.value("LogDisplay/WindowSize", QSize(600, 400)).toSize();

    move(windowPos);
    resize(windowSize); // Restore the saved size

    applyColors();
    addEvent("Killfeed Log Display Initialized");
}

void LogDisplayWindow::resizeEvent(QResizeEvent* event) {
    QMainWindow::resizeEvent(event);

    QSettings settings("KillApiConnect", "KillApiConnectPlus");
    settings.setValue("LogDisplay/WindowSize", size());
    qDebug() << "Window size saved:" << size();
}

void LogDisplayWindow::moveEvent(QMoveEvent* event) {
    QMainWindow::moveEvent(event);

    QSettings settings("KillApiConnect", "KillApiConnectPlus");
    settings.setValue("LogDisplay/WindowPosition", pos());
    qDebug() << "Window position saved:" << pos();
}

void LogDisplayWindow::loadFilterSettings() {
    QSettings settings("KillApiConnect", "KillApiConnectPlus");
    showPvP = settings.value("LogDisplay/ShowPvP", false).toBool();
    showPvE = settings.value("LogDisplay/ShowPvE", true).toBool();
    showNPCNames = settings.value("LogDisplay/ShowNPCNames", true).toBool();
    playSound = settings.value("LogDisplay/PlaySound", false).toBool();
}

void LogDisplayWindow::setupUI() {
    setWindowTitle("Killfeed Log Display");
    setWindowIcon(QIcon(":/app_icon"));

    // Main layout
    QWidget* container = new QWidget(this);
    QVBoxLayout* mainLayout = new QVBoxLayout(container);

    // Control bar
    QHBoxLayout* controlBarLayout = new QHBoxLayout();

    QSettings settings("KillApiConnect", "KillApiConnectPlus");

    // Show PvP Checkbox
    showPvPCheckbox = new QCheckBox("Show PvP", this);
    showPvPCheckbox->setChecked(showPvP);
    showPvPCheckbox->setToolTip("Show PvP events (e.g., player kills)");
    connect(showPvPCheckbox, &QCheckBox::toggled, this, &LogDisplayWindow::onShowPvPToggled);
    controlBarLayout->addWidget(showPvPCheckbox);

    // Show PvE Checkbox
    showPvECheckbox = new QCheckBox("Show PvE", this);
    showPvECheckbox->setChecked(showPvE);
    showPvECheckbox->setToolTip("Show PvE events (e.g., NPC kills)");
    connect(showPvECheckbox, &QCheckBox::toggled, this, &LogDisplayWindow::onShowPvEToggled);
    controlBarLayout->addWidget(showPvECheckbox);

    // Show NPC Names Checkbox
    showNPCNamesCheckbox = new QCheckBox("Show NPC Names", this);
    showNPCNamesCheckbox->setChecked(showNPCNames);
    showNPCNamesCheckbox->setToolTip("Show actual NPC names in events or just 'NPC'");
    connect(showNPCNamesCheckbox, &QCheckBox::toggled, this, &LogDisplayWindow::onShowNPCNamesToggled);
    controlBarLayout->addWidget(showNPCNamesCheckbox);

    // Play Sound Checkbox
    playSoundCheckbox = new QCheckBox("Play Sound", this);
    playSoundCheckbox->setChecked(playSound);
    connect(playSoundCheckbox, &QCheckBox::toggled, this, [this](bool checked) {
        playSound = checked;
        QSettings settings("KillApiConnect", "KillApiConnectPlus");
        settings.setValue("LogDisplay/PlaySound", playSound); // Save immediately
    });
    controlBarLayout->addWidget(playSoundCheckbox);

    // Test Button
    testButton = new QPushButton("Test", this);
    testButton->setVisible(ISDEBUG); // Use ISDEBUG to control visibility
    connect(testButton, &QPushButton::clicked, this, &LogDisplayWindow::handleTestButton);
    controlBarLayout->addWidget(testButton);

    // Monitoring Button
    monitoringButton = new QPushButton("Start Monitoring", this);
    connect(monitoringButton, &QPushButton::clicked, this, &LogDisplayWindow::handleMonitoringButton);
    controlBarLayout->addWidget(monitoringButton);

    mainLayout->addLayout(controlBarLayout);

    // Log display
    logDisplay = new QTextEdit(this);
    logDisplay->setReadOnly(true);
    mainLayout->addWidget(logDisplay);

// Buttons layout
    QHBoxLayout* buttonLayout = new QHBoxLayout();

    // Clear button
    QPushButton* clearButton = new QPushButton("Clear", this);
    connect(clearButton, &QPushButton::clicked, this, &LogDisplayWindow::clearLog);
    buttonLayout->addWidget(clearButton);

    // Text color button
    QPushButton* textColorButton = new QPushButton("Text Color", this);
    connect(textColorButton, &QPushButton::clicked, this, &LogDisplayWindow::changeTextColor);
    buttonLayout->addWidget(textColorButton);

    // Background color button
    QPushButton* bgColorButton = new QPushButton("Background Color", this);
    connect(bgColorButton, &QPushButton::clicked, this, &LogDisplayWindow::changeBackgroundColor);
    buttonLayout->addWidget(bgColorButton);

    // Font size increase button
    QPushButton* increaseFontButton = new QPushButton("+", this);
    connect(increaseFontButton, &QPushButton::clicked, this, &LogDisplayWindow::increaseFontSize);
    increaseFontButton->setToolTip("Increase Font Size\nCTRL + +");
    increaseFontButton->setEnabled(logFontSize < 72); // Disable if font size is already at maximum
    increaseFontButton->setFixedWidth(50); // Set fixed size for increase button
    // Set keyboard shortcut to CTRL + +
    increaseFontButton->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Plus));
    buttonLayout->addWidget(increaseFontButton);

    // Font size decrease button
    QPushButton* decreaseFontButton = new QPushButton("-", this);
    connect(decreaseFontButton, &QPushButton::clicked, this, &LogDisplayWindow::decreaseFontSize);
    decreaseFontButton->setToolTip("Decrease Font Size\nCTRL + -");
    decreaseFontButton->setEnabled(logFontSize > 1); // Disable if font size is already at minimum
    decreaseFontButton->setFixedWidth(50); // Set fixed size for decrease button
    // Set keyboard shortcut to CTRL + -
    decreaseFontButton->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Minus));
    buttonLayout->addWidget(decreaseFontButton);

    mainLayout->addLayout(buttonLayout);
    container->setLayout(mainLayout);
    setCentralWidget(container);
}

void LogDisplayWindow::onShowPvPToggled(bool checked) {
    qDebug() << "LogDisplayWindow::onShowPvPToggled - PvP filter changed from" 
             << showPvP << "to" << checked;
    showPvP = checked;
    QSettings settings("KillApiConnect", "KillApiConnectPlus");
    settings.setValue("LogDisplay/ShowPvP", showPvP);
    qDebug() << "LogDisplayWindow::onShowPvPToggled - Saved setting to QSettings"; 
    emit filterPvPChanged(showPvP);
    qDebug() << "LogDisplayWindow::onShowPvPToggled - Emitted filterPvPChanged signal";
    
    // This change now requires restarting monitoring to apply the new filter
    updateStatusLabel("Filter changed: Restart monitoring to apply");
    qDebug() << "LogDisplayWindow::onShowPvPToggled - Notified user to restart monitoring";
}

void LogDisplayWindow::onShowPvEToggled(bool checked) {
    qDebug() << "LogDisplayWindow::onShowPvEToggled - PvE filter changed from" 
             << showPvE << "to" << checked;
    showPvE = checked;
    QSettings settings("KillApiConnect", "KillApiConnectPlus");
    settings.setValue("LogDisplay/ShowPvE", showPvE);
    qDebug() << "LogDisplayWindow::onShowPvEToggled - Saved setting to QSettings";
    emit filterPvEChanged(showPvE);
    qDebug() << "LogDisplayWindow::onShowPvEToggled - Emitted filterPvEChanged signal";
    
    // This change now requires restarting monitoring to apply the new filter
    updateStatusLabel("Filter changed: Restart monitoring to apply");
    qDebug() << "LogDisplayWindow::onShowPvEToggled - Notified user to restart monitoring";
}

void LogDisplayWindow::onShowNPCNamesToggled(bool checked) {
    qDebug() << "LogDisplayWindow::onShowNPCNamesToggled - NPC names filter changed from" 
             << showNPCNames << "to" << checked;
    showNPCNames = checked;
    QSettings settings("KillApiConnect", "KillApiConnectPlus");
    settings.setValue("LogDisplay/ShowNPCNames", showNPCNames);
    qDebug() << "LogDisplayWindow::onShowNPCNamesToggled - Saved setting to QSettings";
    emit filterNPCNamesChanged(showNPCNames);
    qDebug() << "LogDisplayWindow::onShowNPCNamesToggled - Emitted filterNPCNamesChanged signal";
    
    // This change now requires restarting monitoring to apply the new filter
    updateStatusLabel("Filter changed: Restart monitoring to apply");
    qDebug() << "LogDisplayWindow::onShowNPCNamesToggled - Notified user to restart monitoring";
}

void LogDisplayWindow::updateFilterSettings(bool newShowPvP, bool newShowPvE, bool newShowNPCNames) {
    bool changed = false;
    
    if (showPvP != newShowPvP) {
        showPvP = newShowPvP;
        showPvPCheckbox->setChecked(showPvP);
        changed = true;
    }
    
    if (showPvE != newShowPvE) {
        showPvE = newShowPvE;
        showPvECheckbox->setChecked(showPvE);
        changed = true;
    }
    
    if (showNPCNames != newShowNPCNames) {
        showNPCNames = newShowNPCNames;
        showNPCNamesCheckbox->setChecked(showNPCNames);
        changed = true;
    }
    
    if (changed) {
        filterAndDisplayLogs();
    }
}

void LogDisplayWindow::addEvent(const QString& eventText) {
    qDebug() << "LogDisplayWindow::addEvent received:" << eventText;
    
    // Add to buffer
    eventBuffer.append(eventText);
    if (eventBuffer.size() > 100) {
        eventBuffer.removeFirst();
    }

    try {
        // Check if this is valid JSON
        auto parsed = nlohmann::json::parse(eventText.toStdString(), nullptr, false);
        if (parsed.is_discarded() || parsed.empty()) {
            qWarning() << "Invalid JSON in addEvent, displaying raw text:" << eventText;
            logDisplay->append(eventText);
            return;
        }
        
        // Events are already filtered at the LogMonitor level, just prettify and display
        QString prettifiedLog = prettifyLog(eventText);
        if (!prettifiedLog.isEmpty()) {
            logDisplay->append(prettifiedLog);
            logDisplay->verticalScrollBar()->setValue(logDisplay->verticalScrollBar()->maximum());
        } else {
            // Fallback to raw JSON if prettifying failed
            logDisplay->append(eventText);
        }
    } catch (const std::exception& e) {
        qWarning() << "Exception in addEvent:" << e.what();
        logDisplay->append(eventText); // Fallback to raw text
    }
    
    // Play sound if enabled
    if (playSound) {
        QSettings settings("KillApiConnect", "KillApiConnectPlus");
        QString soundFile = settings.value("LogDisplay/SoundFile", ":/sounds/default.mp3").toString();
        QMediaPlayer* player = new QMediaPlayer(this);
        QAudioOutput* audioOutput = new QAudioOutput(this);
        player->setSource(QUrl::fromLocalFile(soundFile));
        player->setAudioOutput(audioOutput);
        audioOutput->setVolume(0.5);
        player->play();
    }
}

QStringList LogDisplayWindow::filterLogs(const QStringList& logs) const {
    QStringList filteredLogs;

    for (const QString& event : logs) {
        auto parsed = nlohmann::json::parse(event.toStdString(), nullptr, false);
        qDebug() << "LogDisplayWindow: Parsed JSON:" << QString::fromStdString(parsed.dump());


        QString prettifiedLog = prettifyLog(QString::fromStdString(parsed.dump()));
        filteredLogs.append(prettifiedLog);
    }

    return filteredLogs;
}

QString LogDisplayWindow::prettifyLog(const QString& log) const {
    auto parsed = nlohmann::json::parse(log.toStdString(), nullptr, false);
    if (parsed.is_discarded()) {
        return log; // Return the raw log if parsing fails
    }

    QString timestamp;
    if (parsed.contains("timestamp") && parsed["timestamp"].is_number()) {
        qint64 timestampValue = parsed["timestamp"].get<qint64>();
        QDateTime dateTime = QDateTime::fromSecsSinceEpoch(timestampValue);
        timestamp = dateTime.toString("HH:mm:ss");
    }

    QString identifier = QString::fromStdString(parsed.value("identifier", ""));
    QString message = formatEvent(identifier, parsed);

    // Combine timestamp and message
    return timestamp.isEmpty() ? message : QString("[%1] %2").arg(timestamp, message);
}

QString LogDisplayWindow::formatEvent(const QString& identifier, const nlohmann::json& parsed) const {
    QString message;

    if (identifier == "kill_log") {
        if (parsed.value("killer", "") == parsed.value("victim", "")) {
            message = QString("%1 killed themselves").arg(QString::fromStdString(parsed.value("killer", "")));
        } else {
            QString killer = QString::fromStdString(parsed.value("killer", "Unknown"));
            QString victim = QString::fromStdString(parsed.value("victim", "Unknown"));
            message = QString("%1 killed %2").arg(killer, victim);
        }
        QString killer = QString::fromStdString(parsed.value("killer", "Unknown"));
        QString victim = QString::fromStdString(parsed.value("victim", "Unknown"));
        message = QString("%1 killed %2").arg(killer, victim);
    } else if (identifier == "vehicle_destruction") {
        QString vehicle = QString::fromStdString(parsed.value("vehicle", "Unknown"));
        QString cause = QString::fromStdString(parsed.value("cause", "Unknown"));
        message = QString("%1 was destroyed by %2").arg(vehicle, cause);
    } else if (identifier == "vehicle_soft_death") {
        QString vehicle = QString::fromStdString(parsed.value("vehicle", "Unknown"));
        QString driver = QString::fromStdString(parsed.value("driver", "Unknown"));
        QString cause = QString::fromStdString(parsed.value("cause", "Unknown"));
        message = QString("%1 put vehicle %2 of %3 in soft death state").arg(cause, vehicle, driver);
    } else if (identifier == "player_connect") {
        QString player = QString::fromStdString(parsed.value("player", "Unknown"));
        message = QString("%1 connected").arg(player);
    } else if (identifier == "player_disconnect") {
        message = QString("Player disconnected");
    } else {
        if (ISDEBUG && false) {
            qDebug() << "Unknown identifier:" << identifier;
                  message = QString::fromStdString(parsed.dump()); // Fallback to raw JSON
        } else {
            message = "";
            qDebug() << "Unknown identifier:" << identifier << "in debug mode, not displaying.";
        }
    }

    return message;
}

void LogDisplayWindow::filterAndDisplayLogs() {
    QStringList filteredLogs = filterLogs(eventBuffer);
    logDisplay->setPlainText(filteredLogs.join("\n"));
    logDisplay->verticalScrollBar()->setValue(logDisplay->verticalScrollBar()->maximum());
}

void LogDisplayWindow::clearLog() {
    eventBuffer.clear();
    logDisplay->clear();
    addEvent("Killfeed Log Display Cleared");
}

void LogDisplayWindow::changeTextColor() {
    QColor color = QColorDialog::getColor(Qt::white, this, "Select Text Color");
    if (color.isValid()) {
        logFgColor = color.name();
        applyColors();

        // Save the new text color to QSettings
        QSettings settings("KillApiConnect", "KillApiConnectPlus");
        settings.setValue("LogDisplay/TextColor", logFgColor);
    }
}

void LogDisplayWindow::changeBackgroundColor() {
    QColor color = QColorDialog::getColor(Qt::black, this, "Select Background Color");
    if (color.isValid()) {
        logBgColor = color.name();
        applyColors();

        // Save the new background color to QSettings
        QSettings settings("KillApiConnect", "KillApiConnectPlus");
        settings.setValue("LogDisplay/BackgroundColor", logBgColor);
    }
}

void LogDisplayWindow::increaseFontSize() {
    logFontSize += 1;
    applyColors();

    // Save the new font size to QSettings
    QSettings settings("KillApiConnect", "KillApiConnectPlus");
    settings.setValue("LogDisplay/FontSize", logFontSize);
}

void LogDisplayWindow::decreaseFontSize() {
    if (logFontSize > 1) {
        logFontSize -= 1;
        applyColors();

        // Save the new font size to QSettings
        QSettings settings("KillApiConnect", "KillApiConnectPlus");
        settings.setValue("LogDisplay/FontSize", logFontSize);
    }
}

void LogDisplayWindow::applyColors() {
    logDisplay->setStyleSheet(QString(
        "background-color: %1; color: %2; font-family: Consolas; font-size: %3px;")
        .arg(logBgColor)
        .arg(logFgColor)
        .arg(logFontSize));
}

void LogDisplayWindow::setColors(const QString& bgColor, const QString& fgColor) {
    logBgColor = bgColor;
    logFgColor = fgColor;
    applyColors();
}

void LogDisplayWindow::setFontSize(int fontSize) {
    logFontSize = fontSize;
    applyColors();
}

void LogDisplayWindow::closeEvent(QCloseEvent* event) {
    // Only emit the windowClosed signal if the application is not shutting down
    if (!isShuttingDown) {
        emit windowClosed(); // Notify MainWindow that the window was manually closed
    }

    qDebug() << "LogDisplayWindow closed.";
    QMainWindow::closeEvent(event);
}

void LogDisplayWindow::handleTestButton() {
    qDebug() << "Test button clicked.";
    
    // Debug current filter settings
    QSettings settings("KillApiConnect", "KillApiConnectPlus");
    bool showPvP = settings.value("LogDisplay/ShowPvP", false).toBool();
    bool showPvE = settings.value("LogDisplay/ShowPvE", true).toBool();
    bool showNPCNames = settings.value("LogDisplay/ShowNPCNames", true).toBool();
    qDebug() << "CURRENT TEST FILTER SETTINGS:"
             << "showPvP=" << showPvP
             << "showPvE=" << showPvE
             << "showNPCNames=" << showNPCNames;
             
    QFile file(":/tests/game.log");
    qDebug() << "Opening test log file: " << file.fileName();
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "Failed to open test log file: " << file.errorString();
        return;
    }

    QTextStream in(&file);
    QStringList lines;
    while (!in.atEnd()) {
        lines.append(in.readLine());
    }
    file.close();


    QString apiKey = settings.value("apiKey", "").toString();
    if (apiKey.isEmpty()) {
        qWarning() << "Cannot process test events: API key is empty";
        logDisplay->append("Error: API key not configured. Please set up in Settings.");
        return;
    }

    // Send a debug ping start notification
    logDisplay->append("Starting test data processing...");
    QString debugPingStart = R"({"identifier": "debug_ping", "message": "Start of test data"})";
    transmitter.enqueueLog(debugPingStart);
    
    // Process and send the test log lines with proper filtering
    for (int i = 0; i < lines.size(); i++) {
        const QString& line = lines[i];
        int delay = (i+1) * 1500; // Fixed delay of 1.5 seconds per line for more predictable results
        
        QTimer::singleShot(delay, this, [this, line, apiKey]() {
            qDebug() << "Processing test line after delay:" << line;
            
            // Parse the log line
            std::string parsedJson = parse_log_line(line.toStdString());
            if (parsedJson != "{}" && !parsedJson.empty()) {
                QString jsonStr = QString::fromStdString(parsedJson);
                qDebug() << "Test line parsed successfully:" << jsonStr;
                
                // Apply the same filtering logic as LogMonitor
                if (FilterUtils::shouldDisplayEvent(jsonStr)) {
                    QString filteredPayload = FilterUtils::applyNPCNameFilter(jsonStr);
                    
                    // Add to display and queue
                    addEvent(filteredPayload);
                    transmitter.enqueueLog(filteredPayload);
transmitter.processQueueInThread(apiKey);
                } else {
                    qDebug() << "Filtered out test event:" << jsonStr;
                }
            } else {
                qWarning() << "Failed to parse test line:" << line;
            }
        });
    }

    // Force processing the queue
    transmitter.processQueueInThread(apiKey);
    
    // Add completion message
    int totalDelay = (lines.size() + 1) * 1500;
    QTimer::singleShot(totalDelay, this, [this]() {
        logDisplay->append("Test data processing complete.");
    });
}

void LogDisplayWindow::processLogQueue(const QString& log) {
    qDebug() << "LogDisplayWindow::processLogQueue received:" << log;
    
    // Verify this is valid JSON
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(log.toUtf8(), &error);
    
    if (error.error != QJsonParseError::NoError || doc.isEmpty()) {
        qWarning() << "Invalid JSON received in processLogQueue:" << log;
        return;
    }
    
    // No need to filter again - already filtered at LogMonitor level
    QString prettifiedLog = prettifyLog(log);
    qDebug() << "Prettified log:" << prettifiedLog;
    
    if (!prettifiedLog.isEmpty()) {
        logDisplay->append(prettifiedLog);
        logDisplay->verticalScrollBar()->setValue(logDisplay->verticalScrollBar()->maximum());
        
        // Play sound if enabled
        if (playSound) {
            QSettings settings("KillApiConnect", "KillApiConnectPlus");
            QString soundFile = settings.value("LogDisplay/SoundFile", ":/sounds/default.mp3").toString();
            QMediaPlayer* player = new QMediaPlayer(this);
            QAudioOutput* audioOutput = new QAudioOutput(this);
            player->setSource(QUrl::fromLocalFile(soundFile));
            player->setAudioOutput(audioOutput);
            audioOutput->setVolume(0.5);
            player->play();
        }
    } else {
        qWarning() << "Skipping empty prettified log for:" << log;
    }
}

// Add a helper method to update status in the log display
void LogDisplayWindow::updateStatusLabel(const QString& message) {
    // Add a visual indication that this is a status message, not an event
    QString formattedMessage = QString("--- %1 ---").arg(message);
    logDisplay->append(formattedMessage);
    logDisplay->verticalScrollBar()->setValue(logDisplay->verticalScrollBar()->maximum());
}

void LogDisplayWindow::setApplicationShuttingDown(bool shuttingDown) {
    isShuttingDown = shuttingDown;
}

void LogDisplayWindow::handleMonitoringButton() {
    emit toggleMonitoringRequested();
}

void LogDisplayWindow::updateMonitoringButtonText(bool isMonitoring) {
    if (monitoringButton) {
        monitoringButton->setText(isMonitoring ? "Stop Monitoring" : "Start Monitoring");
    }
}