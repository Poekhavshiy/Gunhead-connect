#include "globals.h"
#include "LogDisplayWindow.h"
#include "log_parser.h"
#include "ThemeManager.h"
#include "ResizeHelper.h"
#include <QScrollBar>
#include <QDebug>
#include <QSettings>
#include <QColorDialog>
#include <QPointer>
#include <QFile>
#include <QTextStream>
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QFileInfo>
#include <QRandomGenerator>
#include <QApplication> 
#include <QTimer>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include "FilterUtils.h"
#include <QTextBrowser> // ADDED: For QTextBrowser
#include <QDesktopServices> // ADDED: For QDesktopServices::openUrl
#include <QStandardPaths>
#include <QShowEvent>


static QStringList s_logDisplayCache;

LogDisplayWindow::LogDisplayWindow(Transmitter& transmitter, QWidget* parent)
    : QMainWindow(parent), transmitter(transmitter), titleBar(nullptr), logFontSize(12), logBgColor("#000000"), logFgColor("#FFFFFF") 
{
    // Set frameless window hint for custom title bar
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground, false);
    
    // Load JSON rules for dynamic event formatting
    loadJsonRules();
    
    // Get initial game mode for later use
    QString initialGameMode = transmitter.getCurrentGameMode();
    QString initialSubGameMode = transmitter.getCurrentSubGameMode();
    
    // Connect to gameModeChanged signal
    connect(&transmitter, &Transmitter::gameModeChanged, this, 
        [this](const QString& gameMode, const QString& subGameMode) {
            qDebug() << "Received gameModeChanged signal:" << gameMode << subGameMode;
            updateWindowTitle(gameMode, subGameMode);
        });

    // Connect to playerInfoChanged signal
    connect(&transmitter, &Transmitter::playerInfoChanged, this, 
        [this, &transmitter](const QString& playerName, const QString&) {
            qDebug() << "Received playerInfoChanged signal:" << playerName;
            QString gameMode = transmitter.getCurrentGameMode();
            QString subGameMode = transmitter.getCurrentSubGameMode();
            updateWindowTitle(gameMode, subGameMode);
        });

    // Create sound player
    soundPlayer = new SoundPlayer(this);
    
    // Load filter settings from QSettings
    loadFilterSettings();
    
    // Setup UI FIRST (this creates titleBar)
    setupUI();
    
    // NOW update window title after titleBar exists
    updateWindowTitle(initialGameMode, initialSubGameMode);

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

    // Set window title based on current game mode at initialization
    // Get current game mode from the transmitter
    QString apiKey = settings.value("apiKey", "").toString();

    // Check if game mode is available from the transmitter and update status
    if (initialGameMode != "Unknown") {
        if (initialGameMode == "PU") {
            updateStatusLabel(tr("Game mode: Persistent Universe"));
        } else if (initialGameMode == "AC") {
            updateStatusLabel(tr("Game mode: Arena Commander") + 
                             (initialSubGameMode.isEmpty() ? "" : ": " + initialSubGameMode));
        }
    }

    // Restore cached log if available
    if (!s_logDisplayCache.isEmpty()) {
        eventBuffer = s_logDisplayCache;
        filterAndDisplayLogs();
    } else {
        // If no cached log, initialize with a default JSON message
        QString initialLog = R"({"identifier":"status","message":")" + tr("Gunhead Log Display Initialized") + R"("})";
        addEvent(initialLog);
    }

    // Connect to transmitter errors
    connect(&transmitter, &Transmitter::apiError, this, [this](Transmitter::ApiErrorType errorType, const QString& errorMessage) {
        // Add error message to log display with special formatting
        QString errorDisplay = QString("<span style='color:red;'>ERROR: %1</span>").arg(errorMessage);
        logDisplay->append(errorDisplay);
        
        // Also update the status label with a different message
        updateStatusLabel(tr("API connection error detected. Monitoring stopped."));
    });

    // Add this connection to respond to language changes
    connect(&LanguageManager::instance(), &LanguageManager::languageChanged,
            this, &LogDisplayWindow::retranslateUi);

    // Connect to theme changes
    connect(&ThemeManager::instance(), &ThemeManager::themeApplied,
            this, &LogDisplayWindow::onThemeChanged);
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
}

void LogDisplayWindow::loadFilterSettings() {
    QSettings settings("KillApiConnect", "KillApiConnectPlus");
    
    showPvP = settings.value("LogDisplay/ShowPvP", false).toBool();
    showPvE = settings.value("LogDisplay/ShowPvE", true).toBool();
    showShips = settings.value("LogDisplay/ShowShips", true).toBool();
    showOther = settings.value("LogDisplay/ShowOther", false).toBool();
    showNPCNames = settings.value("LogDisplay/ShowNPCNames", true).toBool();
    playSound = settings.value("LogDisplay/PlaySound", false).toBool();
    
    // Load filter mode (0 = All Events, 1 = Custom)
    int filterMode = settings.value("LogDisplay/FilterMode", 0).toInt();
    if (filterMode == 0) { // All Events mode
        showPvP = true;
        showPvE = true;
        showShips = true;
        showOther = true;
    }
    
    qDebug() << "Filter settings loaded - PvP:" << showPvP << "PvE:" << showPvE << "Ships:" << showShips << "Other:" << showOther << "NPC Names:" << showNPCNames << "Filter Mode:" << filterMode;
}
void LogDisplayWindow::resetLogDisplay() {
    clearLog();
}

void LogDisplayWindow::setupUI() {
    setObjectName("LogDisplayWindow"); // This is crucial - defines the window's identity for QSS
    setWindowTitle(tr("Gunhead Log Display"));
    setWindowIcon(QIcon(":/icons/Gunhead.ico"));
    setMinimumSize(500, 400); // Set minimum window size for resize

    // Create custom title bar
    titleBar = new CustomTitleBar(this, true); // Include maximize button for log window
    titleBar->setTitle(tr("Gunhead Log Display"));
    titleBar->setIcon(QIcon(":/icons/Gunhead.png")); // Set logo icon for LogDisplayWindow title bar
    
    // Connect title bar signals
    connect(titleBar, &CustomTitleBar::minimizeClicked, this, &LogDisplayWindow::showMinimized);
    connect(titleBar, &CustomTitleBar::maximizeClicked, this, [this]() {
        // Safety check to ensure titleBar still exists
        if (!titleBar) return;
        
        if (isMaximized()) {
            showNormal();
            titleBar->updateMaximizeButton(false);
        } else {
            showMaximized();
            titleBar->updateMaximizeButton(true);
        }
    });
    connect(titleBar, &CustomTitleBar::closeClicked, this, &LogDisplayWindow::close);

    // Create container widget to hold title bar and content
    QWidget* outerContainer = new QWidget(this);
    outerContainer->setObjectName("logWindowContainer");
    QVBoxLayout* outerLayout = new QVBoxLayout(outerContainer);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);
    outerLayout->addWidget(titleBar);
    
    // Main layout
    QWidget* container = new QWidget(outerContainer);
    container->setObjectName("logDisplayContainer"); // Add container object name for QSS
    container->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    QVBoxLayout* mainLayout = new QVBoxLayout(container);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    outerLayout->addWidget(container, 1);
    setCentralWidget(outerContainer);

    // Apply window effects (rounded corners and shadow)
    // Use QPointer for safe access in case window is destroyed before timer fires
    QPointer<LogDisplayWindow> safeThis(this);
    QTimer::singleShot(100, this, [safeThis]() {
        if (safeThis) {
            CustomTitleBar::applyWindowEffects(safeThis);
        }
    });
    
    // Enable window resizing for frameless window
    resizeHelper = new ResizeHelper(this, titleBar->height());

    // Control bar
    QHBoxLayout* controlBarLayout = new QHBoxLayout();

    // Create a button that looks like a dropdown
    filterDropdown = new QPushButton(tr("Select Filters   ▼"), this);
    filterDropdown->setObjectName("filterDropdown"); // Add object name for QSS targeting
    
    // Remove inline stylesheet that might override theme
    // filterDropdown->setStyleSheet(""); // Clear any previous styling
    
    // Calculate initial width based on current font
    QTimer::singleShot(0, this, &LogDisplayWindow::updateFilterDropdownWidth);
    
    // Create the filter widget (hidden by default)
    filterWidget = new FilterDropdownWidget(this);
    filterWidget->setShowPvP(showPvP);
    filterWidget->setShowPvE(showPvE);
    filterWidget->setShowShips(showShips);
    filterWidget->setShowOther(showOther);
    filterWidget->setShowNPCNames(showNPCNames);
    filterWidget->hide(); // Initially hidden
    
    // Connect button click to show popup
    connect(filterDropdown, &QPushButton::clicked, this, [this]() {
        // Check if popup is already open and close it
        QWidget* existingPopup = this->findChild<QWidget*>("filterPopup");
        if (existingPopup) {
            existingPopup->close();
            return;
        }
        
        // Show custom filter widget as a popup
        QPoint globalPos = filterDropdown->mapToGlobal(QPoint(0, filterDropdown->height()));
        
        // Create a popup widget to hold the filter options
        QWidget* popup = new QWidget(nullptr, Qt::Popup);
        popup->setObjectName("filterPopup"); // Set object name to find it later
        popup->setWindowFlags(Qt::Popup | Qt::FramelessWindowHint);
        popup->setAttribute(Qt::WA_DeleteOnClose);
        
        // Apply current theme to popup widget manually since it doesn't inherit from parent
        Theme currentTheme = ThemeManager::instance().loadCurrentTheme();
        QFile themeFile(currentTheme.stylePath);
        if (themeFile.open(QFile::ReadOnly | QFile::Text)) {
            QString themeStyle = themeFile.readAll();
            popup->setStyleSheet(themeStyle);
        }
        
        QVBoxLayout* popupLayout = new QVBoxLayout(popup);
        popupLayout->setContentsMargins(5, 5, 5, 5);
        
        // Create a new filter widget for the popup
        FilterDropdownWidget* popupFilterWidget = new FilterDropdownWidget(popup);
        popupFilterWidget->setShowPvP(showPvP);
        popupFilterWidget->setShowPvE(showPvE);
        popupFilterWidget->setShowShips(showShips);
        popupFilterWidget->setShowOther(showOther);
        popupFilterWidget->setShowNPCNames(showNPCNames);
        
        popupLayout->addWidget(popupFilterWidget);
        
        // Connect the popup filter widget changes
        connect(popupFilterWidget, &FilterDropdownWidget::filterChanged, this, [this, popup, popupFilterWidget]() {
            showPvP = popupFilterWidget->getShowPvP();
            showPvE = popupFilterWidget->getShowPvE();
            showShips = popupFilterWidget->getShowShips();
            showOther = popupFilterWidget->getShowOther();
            showNPCNames = popupFilterWidget->getShowNPCNames();
            
            // Update the main filter widget
            filterWidget->setShowPvP(showPvP);
            filterWidget->setShowPvE(showPvE);
            filterWidget->setShowShips(showShips);
            filterWidget->setShowOther(showOther);
            filterWidget->setShowNPCNames(showNPCNames);
            
            // Save settings
            QSettings settings("KillApiConnect", "KillApiConnectPlus");
            settings.setValue("LogDisplay/ShowPvP", showPvP);
            settings.setValue("LogDisplay/ShowPvE", showPvE);
            settings.setValue("LogDisplay/ShowShips", showShips);
            settings.setValue("LogDisplay/ShowOther", showOther);
            settings.setValue("LogDisplay/ShowNPCNames", showNPCNames);
            
            // Emit signals for MainWindow
            emit filterPvPChanged(showPvP);
            emit filterPvEChanged(showPvE);
            emit filterShipsChanged(showShips);
            emit filterOtherChanged(showOther);
            emit filterNPCNamesChanged(showNPCNames);
            
            // Refresh display
            filterAndDisplayLogs();
            
            // Keep the dropdown open - don't close automatically
        });
        
        popup->move(globalPos);
        popup->show();
        popup->raise();
        popup->activateWindow();
    });
    
    controlBarLayout->addWidget(filterDropdown);

    // Play Sound Checkbox
    playSoundCheckbox = new QCheckBox(tr("Play Sound"), this);
    playSoundCheckbox->setToolTip(tr("Coming soon: Play sound on events"));
    playSoundCheckbox->setChecked(playSound);
    connect(playSoundCheckbox, &QCheckBox::toggled, this, [this](bool checked) {
        playSound = checked;
        QSettings settings("KillApiConnect", "KillApiConnectPlus");
        settings.setValue("LogDisplay/PlaySound", playSound);
    });
    controlBarLayout->addWidget(playSoundCheckbox);

    // Test Button
    testButton = new QPushButton(tr("Test"), this);
    testButton->setVisible(ISDEBUG);
    connect(testButton, &QPushButton::clicked, this, &LogDisplayWindow::handleTestButton);
    controlBarLayout->addWidget(testButton);

    // Monitoring Button
    monitoringButton = new QPushButton(tr("Start Monitoring"), this);
    connect(monitoringButton, &QPushButton::clicked, this, &LogDisplayWindow::handleMonitoringButton);
    controlBarLayout->addWidget(monitoringButton);

    mainLayout->addLayout(controlBarLayout);

    // Log display
    logDisplay = new QTextBrowser(this);
    logDisplay->setReadOnly(true);
    logDisplay->setTextInteractionFlags(Qt::TextBrowserInteraction | Qt::LinksAccessibleByMouse | Qt::LinksAccessibleByKeyboard);
    logDisplay->setOpenExternalLinks(true);
    mainLayout->addWidget(logDisplay);

    // Buttons layout
    QHBoxLayout* buttonLayout = new QHBoxLayout();

    // Clear button
    QPushButton* clearButton = new QPushButton(tr("Clear"), this);
    clearButton->setObjectName("clearButton"); // Add this line
    connect(clearButton, &QPushButton::clicked, this, &LogDisplayWindow::clearLog);
    buttonLayout->addWidget(clearButton);

    // Text color button
    QPushButton* textColorButton = new QPushButton(tr("Text Color"), this);
    textColorButton->setObjectName("textColorButton"); // Add this line
    connect(textColorButton, &QPushButton::clicked, this, &LogDisplayWindow::changeTextColor);
    buttonLayout->addWidget(textColorButton);

    // Background color button
    QPushButton* bgColorButton = new QPushButton(tr("Background Color"), this);
    bgColorButton->setObjectName("bgColorButton"); // Add this line
    connect(bgColorButton, &QPushButton::clicked, this, &LogDisplayWindow::changeBackgroundColor);
    buttonLayout->addWidget(bgColorButton);

    // Font size increase button
    QPushButton* increaseFontButton = new QPushButton("+", this);
    connect(increaseFontButton, &QPushButton::clicked, this, &LogDisplayWindow::increaseFontSize);
    increaseFontButton->setToolTip(tr("Increase Font Size\nCTRL + +"));
    increaseFontButton->setEnabled(logFontSize < 72); // Disable if font size is already at maximum
    increaseFontButton->setFixedWidth(50); // Set fixed size for increase button
    // Set keyboard shortcut to CTRL + +
    increaseFontButton->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Plus));
    buttonLayout->addWidget(increaseFontButton);

    // Font size decrease button
    QPushButton* decreaseFontButton = new QPushButton(tr("-"), this);
    connect(decreaseFontButton, &QPushButton::clicked, this, &LogDisplayWindow::decreaseFontSize);
    decreaseFontButton->setToolTip(tr("Decrease Font Size\nCTRL + -"));
    decreaseFontButton->setEnabled(logFontSize > 1); // Disable if font size is already at minimum
    decreaseFontButton->setFixedWidth(50); // Set fixed size for decrease button
    // Set keyboard shortcut to CTRL + -
    decreaseFontButton->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Minus));
    buttonLayout->addWidget(decreaseFontButton);

    // Sound test button (for debugging)
    QPushButton* testSoundButton = new QPushButton(tr("Test Sound"), this);
    testSoundButton->setObjectName("testSoundButton"); // Add this line
    testSoundButton->setVisible(ISDEBUG);
    connect(testSoundButton, &QPushButton::clicked, this, [this]() {
        qDebug() << "Testing sound playback...";
        
        // Try multiple playback methods
        playSystemSound();
    });
    buttonLayout->addWidget(testSoundButton);

    mainLayout->addLayout(buttonLayout);
}

void LogDisplayWindow::onFilterDropdownChanged(int index) {
    // Remove the old checkbox visibility code since we're using FilterDropdownWidget now
    // The FilterDropdownWidget handles its own visibility and state
    
    // Just update the filter settings based on the widget state
    showPvP = filterWidget->getShowPvP();
    showPvE = filterWidget->getShowPvE();
    showShips = filterWidget->getShowShips();
    showOther = filterWidget->getShowOther();
    showNPCNames = filterWidget->getShowNPCNames();
    
    // Save settings
    QSettings settings("KillApiConnect", "KillApiConnectPlus");
    settings.setValue("LogDisplay/ShowPvP", showPvP);
    settings.setValue("LogDisplay/ShowPvE", showPvE);
    settings.setValue("LogDisplay/ShowShips", showShips);
    settings.setValue("LogDisplay/ShowOther", showOther);
    settings.setValue("LogDisplay/ShowNPCNames", showNPCNames);
    
    // Refresh display
    filterAndDisplayLogs();
}

void LogDisplayWindow::onShowPvPToggled(bool checked) {
    qDebug() << "LogDisplayWindow::onShowPvPToggled - PvP filter changed from" 
             << showPvP << "to" << checked;
    showPvP = checked;
    QSettings settings("KillApiConnect", "KillApiConnectPlus");
    settings.setValue("LogDisplay/ShowPvP", showPvP);
    qDebug() << "LogDisplayWindow::onShowPvPToggled - Saved setting to QSettings"; 
    emit filterPvPChanged(checked);
    qDebug() << "LogDisplayWindow::onShowPvPToggled - Emitted filterPvPChanged signal";
    
    filterAndDisplayLogs();
}

void LogDisplayWindow::onShowPvEToggled(bool checked) {
    qDebug() << "LogDisplayWindow::onShowPvEToggled - PvE filter changed from" 
             << showPvE << "to" << checked;
    showPvE = checked;
    QSettings settings("KillApiConnect", "KillApiConnectPlus");
    settings.setValue("LogDisplay/ShowPvE", showPvE);
    qDebug() << "LogDisplayWindow::onShowPvEToggled - Saved setting to QSettings";
    emit filterPvEChanged(checked);
    qDebug() << "LogDisplayWindow::onShowPvEToggled - Emitted filterPvEChanged signal";
    
    filterAndDisplayLogs();
}

void LogDisplayWindow::onShowShipsToggled(bool checked) {
    qDebug() << "LogDisplayWindow::onShowShipsToggled - Ships filter changed from" 
             << showShips << "to" << checked;
    showShips = checked;
    QSettings settings("KillApiConnect", "KillApiConnectPlus");
    settings.setValue("LogDisplay/ShowShips", showShips);
    qDebug() << "LogDisplayWindow::onShowShipsToggled - Saved setting to QSettings";
    emit filterShipsChanged(checked);
    qDebug() << "LogDisplayWindow::onShowShipsToggled - Emitted filterShipsChanged signal";
    
    filterAndDisplayLogs();
}

void LogDisplayWindow::onShowOtherToggled(bool checked) {
    qDebug() << "LogDisplayWindow::onShowOtherToggled - Other filter changed from" 
             << showOther << "to" << checked;
    showOther = checked;
    QSettings settings("KillApiConnect", "KillApiConnectPlus");
    settings.setValue("LogDisplay/ShowOther", showOther);
    qDebug() << "LogDisplayWindow::onShowOtherToggled - Saved setting to QSettings";
    emit filterOtherChanged(checked);
    qDebug() << "LogDisplayWindow::onShowOtherToggled - Emitted filterOtherChanged signal";
    
    filterAndDisplayLogs();
}

void LogDisplayWindow::onShowNPCNamesToggled(bool checked) {
    qDebug() << "LogDisplayWindow::onShowNPCNamesToggled - NPC names filter changed from" 
             << showNPCNames << "to" << checked;
    showNPCNames = checked;
    QSettings settings("KillApiConnect", "KillApiConnectPlus");
    settings.setValue("LogDisplay/ShowNPCNames", showNPCNames);
    qDebug() << "LogDisplayWindow::onShowNPCNamesToggled - Saved setting to QSettings";
    emit filterNPCNamesChanged(checked);
    qDebug() << "LogDisplayWindow::onShowNPCNamesToggled - Emitted filterNPCNamesChanged signal";
    
    filterAndDisplayLogs();
}

void LogDisplayWindow::updateFilterSettings(bool pvp, bool e, bool npcNames) {
    showPvP = pvp;
    showPvE = e;
    showNPCNames = npcNames;
    
    // Update the filter widget instead of individual checkboxes
    if (filterWidget) {
        filterWidget->setShowPvP(showPvP);
        filterWidget->setShowPvE(showPvE);
        filterWidget->setShowNPCNames(showNPCNames);
    }
    
    // Save settings
    QSettings settings("KillApiConnect", "KillApiConnectPlus");
    settings.setValue("LogDisplay/ShowPvP", showPvP);
    settings.setValue("LogDisplay/ShowPvE", showPvE);
    settings.setValue("LogDisplay/ShowNPCNames", showNPCNames);
    
    // Refresh display
    filterAndDisplayLogs();
}

// Update the addEvent method to apply filters
void LogDisplayWindow::addEvent(const QString& eventText) {
    // Skip empty JSON objects
    if (eventText == "{}" || eventText.trimmed().isEmpty()) {
        return;
    }
    
    qDebug() << "LogDisplayWindow::addEvent received:" << eventText;

    // Add to buffer - store all events for filtering later
    eventBuffer.append(eventText);
    if (eventBuffer.size() > 100) {
        eventBuffer.removeFirst();
    }
    s_logDisplayCache = eventBuffer; // Cache the buffer    // Additional pre-check for game mode messages and player character info
    if (eventText.contains("game_mode_pu") || 
        eventText.contains("game_mode_ac") || 
        eventText.contains("{Join PU}") ||
        eventText.contains("EALobby") ||
        eventText.contains("\"identifier\":\"player_character_info\"")) {
        qDebug() << "LogDisplayWindow::addEvent - Filtered out game mode/system message";
        return; // Skip display but keep in buffer
    }

    // Check if this event should be displayed based on current filter settings
    if (!FilterUtils::shouldDisplayEvent(eventText)) {
        qDebug() << "LogDisplayWindow::addEvent - Filtered out event based on settings";
        return; // Skip display but keep in buffer
    }

    QString timePrefix;

    try {
        // Check if this is valid JSON
        auto parsed = nlohmann::json::parse(eventText.toStdString(), nullptr, false);
        if (parsed.is_discarded() || parsed.empty()) {
            qWarning() << "Invalid JSON in addEvent, displaying raw text:" << eventText;
            logDisplay->append(eventText);
            return;
        }

        // Try to extract timestamp from parsed data
        if (parsed.contains("timestamp") && parsed["timestamp"].is_number()) {
            qint64 timestampValue = parsed["timestamp"].get<qint64>();
            QDateTime dateTime = QDateTime::fromSecsSinceEpoch(timestampValue);
            timePrefix = dateTime.toString("HH:mm:ss");
        }        // Events are already filtered at the LogMonitor level, just prettify and display
        QString prettifiedLog = prettifyLog(eventText);
        if (!prettifiedLog.isEmpty()) {
            logDisplay->append(prettifiedLog);
            logDisplay->verticalScrollBar()->setValue(logDisplay->verticalScrollBar()->maximum());
        } else {
            qDebug() << "LogDisplayWindow::addEvent - prettifyLog returned empty, not displaying event";
            // Don't display anything if prettifyLog returned empty (filtered out)
        }
    } catch (const std::exception& e) {
        qWarning() << "Exception in addEvent:" << e.what();
        logDisplay->append(eventText); // Fallback to raw text
    }

    // Play sound if enabled
    if (playSound) {
        soundPlayer->playConfiguredSound(nullptr);
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
        return log;
    }

    QString timestamp;
    if (parsed.contains("timestamp") && parsed["timestamp"].is_number()) {
        qint64 timestampValue = parsed["timestamp"].get<qint64>();
        QDateTime dateTime = QDateTime::fromSecsSinceEpoch(timestampValue);
        timestamp = dateTime.toString("HH:mm:ss");
    }

    QString identifier = QString::fromStdString(parsed.value("identifier", ""));

    // Apply NPC name masking when "Show NPC Names" is unchecked
    if (!showNPCNames && identifier == "kill_log") {
        bool victimIsNPC = parsed.value("victim_is_npc", false);
        bool killerIsNPC = parsed.value("killer_is_npc", false);

        if (victimIsNPC) {
            parsed["victim"] = "an NPC";
        }
        if (killerIsNPC) {
            parsed["killer"] = "An NPC";
        }
    }

    if (identifier == "kill_log") {
        if (parsed.contains("victim") && parsed.contains("victim_is_npc") && !parsed.value("victim_is_npc", false)) {
            QString victim = QString::fromStdString(parsed.value("victim", ""));
            QString styledVictim = QString("<a href=\"https://robertsspaceindustries.com/citizens/%1\"><b><u>%1</u></b></a>").arg(victim.toHtmlEscaped());
            parsed["victim"] = styledVictim.toStdString();
        }
        if (parsed.contains("killer") && parsed.contains("killer_is_npc") && !parsed.value("killer_is_npc", false)) {
            QString killer = QString::fromStdString(parsed.value("killer", ""));
            QString styledKiller = QString("<a href=\"https://robertsspaceindustries.com/citizens/%1\"><b><u>%1</u></b></a>").arg(killer.toHtmlEscaped());
            parsed["killer"] = styledKiller.toStdString();
        }
    }

    // ADDED: Style player names for vehicle destruction, softdeath, and instant destruction events
    if (
        identifier == "vehicle_destruction" ||
        identifier == "vehicle_soft_death" ||
        identifier == "vehicle_instant_destruction"
    ) {
        // driver
        if (parsed.contains("driver") && (!parsed.contains("driver_is_npc") || !parsed.value("driver_is_npc", false))) {
            QString driver = QString::fromStdString(parsed.value("driver", ""));
            if (!driver.isEmpty()) {
                QString styledDriver = QString("<a href=\"https://robertsspaceindustries.com/citizens/%1\"><b><u>%1</u></b></a>").arg(driver.toHtmlEscaped());
                parsed["driver"] = styledDriver.toStdString();
            }
        }
        // cause (if present and not NPC)
        if (parsed.contains("cause") && (!parsed.contains("cause_is_npc") || !parsed.value("cause_is_npc", false))) {
            QString cause = QString::fromStdString(parsed.value("cause", ""));
            if (!cause.isEmpty()) {
                QString styledCause = QString("<a href=\"https://robertsspaceindustries.com/citizens/%1\"><b><u>%1</u></b></a>").arg(cause.toHtmlEscaped());
                parsed["cause"] = styledCause.toStdString();
            }
        }
    }

    QString message = formatEvent(identifier, parsed);

    // Don't return entries with empty messages
    if (message.isEmpty()) {
        qDebug() << "LogDisplayWindow::prettifyLog - Filtered out message for identifier:" << identifier;
        return ""; // Skip this entry entirely
    }

    // Combine timestamp and message
    return timestamp.isEmpty() ? message : QString("[%1] %2").arg(timestamp, message);
}

QString LogDisplayWindow::formatEvent(const QString& identifier, const nlohmann::json& parsed) const {
    qDebug() << "LogDisplayWindow::formatEvent - Processing identifier:" << identifier;

    // Filter out all game mode and system messages
    if (identifier == "game_mode_pu" || 
        identifier == "game_mode_ac" || 
        identifier == "game_mode_change" ||
        identifier == "game_mode_update" ||
        identifier.startsWith("game_mode_") ||  // Catch any other game mode messages
        identifier == "debug_ping" ||
        identifier == "system_message" ||
        identifier == "player_character_info") {
        qDebug() << "LogDisplayWindow::formatEvent - Filtering out identifier:" << identifier;
        return ""; // Return empty string to filter out these messages
    }

    // Look up rule configuration from JSON
    QJsonArray rules = rulesData["rules"].toArray();
    QString filterType;
    QString messageTemplate;
    QString messageTemplateSuicide;
    
    for (const QJsonValue& ruleValue : rules) {
        QJsonObject rule = ruleValue.toObject();
        if (rule["identifier"].toString() == identifier) {
            filterType = rule["filter_type"].toString();
            messageTemplate = rule["message_template"].toString();
            messageTemplateSuicide = rule["message_template_suicide"].toString();
            break;
        }
    }
    
    // Check if this event type should be displayed based on filter settings
    if (!filterType.isEmpty()) {
        if (filterType == "kill") {
            // For kill events, check PvP/PvE filtering
            bool killerIsNPC = parsed.value("killer_is_npc", false);
            bool victimIsNPC = parsed.value("victim_is_npc", false);
            
            if (!killerIsNPC && !victimIsNPC) {
                // Both are players - this is PvP
                if (!showPvP) {
                    qDebug() << "LogDisplayWindow::formatEvent - PvP event filtered out for identifier:" << identifier;
                    return "";
                }
            } else if (killerIsNPC || victimIsNPC) {
                // At least one is NPC - this is PvE
                if (!showPvE) {
                    qDebug() << "LogDisplayWindow::formatEvent - PvE event filtered out for identifier:" << identifier;
                    return "";
                }
            }
        } else if (!shouldShowEventType(filterType)) {
            qDebug() << "LogDisplayWindow::formatEvent - Event type" << filterType << "filtered out for identifier:" << identifier;
            return "";
        }
    }
    
    QString message;
    
    // Use message template if available
    if (!messageTemplate.isEmpty()) {
        // Special case for suicide detection in kill events
        if (identifier == "kill_log" && 
            !messageTemplateSuicide.isEmpty() &&
            parsed.value("killer", "") == parsed.value("victim", "")) {
            message = formatEventFromTemplate(messageTemplateSuicide, parsed);
        } else {
            message = formatEventFromTemplate(messageTemplate, parsed);
        }
        
        qDebug() << "LogDisplayWindow::formatEvent - Using template for" << identifier << ":" << message;
        return message;
    }
    
    // Fallback to hardcoded formatting for events without templates
    if (identifier == "status") {
        // Only show important status messages
        QString statusMsg = QString::fromStdString(parsed.value("message", ""));
        if (statusMsg.contains("error", Qt::CaseInsensitive) || 
            statusMsg.contains("warning", Qt::CaseInsensitive) ||
            statusMsg.contains("initialized", Qt::CaseInsensitive)) {
            message = statusMsg;
        } else {
            return ""; // Filter out routine status messages
        }
    } else {
        // For events without templates, show a basic format
        qDebug() << "LogDisplayWindow::formatEvent - No template found for identifier:" << identifier;
        message = QString("Event: %1").arg(identifier); // Basic fallback
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
    s_logDisplayCache.clear(); // Clear the cache
    logDisplay->clear();
    updateStatusLabel(tr("Gunhead Log Display Cleared"));
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
        logDisplay->append(tr("Error: API key not configured. Please set up in Settings."));
        return;
    }

    // Send a debug ping start notification
    logDisplay->append(tr("Starting test data processing..."));
    QString debugPingStart = R"({"identifier": "debug_ping", "message": "Start of test data"})";
    transmitter.enqueueLog(debugPingStart);
    
    // Process and send the test log lines with proper filtering
    for (int i = 0; i < lines.size(); i++) {
        const QString& line = lines[i];
        int delay = (i+1) * 5000; // Fixed delay of 5 seconds per line for more predictable results

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
        logDisplay->append(tr("Test data processing complete."));
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
            soundPlayer->playConfiguredSound(nullptr);
        }
    } else {
        logDisplay->append(log);
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
    qDebug() << "LogDisplayWindow: Monitoring button clicked.";
    
    // Disable button and show connecting message if starting monitoring
    if (monitoringButton->text() == "Start Monitoring") {
        monitoringButton->setEnabled(false);
        monitoringButton->setText("Connecting...");
    }
    
    emit toggleMonitoringRequested();
}

void LogDisplayWindow::disableMonitoringButton(const QString& text) {
    if (monitoringButton) {
        monitoringButton->setEnabled(false);
        monitoringButton->setText(text);
    }
}

void LogDisplayWindow::enableMonitoringButton(const QString& text) {
    if (monitoringButton) {
        monitoringButton->setEnabled(true);
        monitoringButton->setText(text);
    }
}

// Update the existing updateMonitoringButtonText method to also enable the button
void LogDisplayWindow::updateMonitoringButtonText(bool isMonitoring) {
    if (monitoringButton) {
        monitoringButton->setEnabled(true);
        monitoringButton->setText(isMonitoring ? "Stop Monitoring" : "Start Monitoring");
    }
}

void LogDisplayWindow::updateGameFolder(const QString& newFolder) {
    qDebug() << "LogDisplayWindow::updateGameFolder - New game folder:" << newFolder;

    // If monitoring is active, restart it
    if (monitoringButton->text() == "Stop Monitoring") {
        qDebug() << "LogDisplayWindow::updateGameFolder - Restarting monitoring with new game folder.";
        emit toggleMonitoringRequested(); // Stop monitoring
        emit toggleMonitoringRequested(); // Start monitoring
    } else {
        qDebug() << "LogDisplayWindow::updateGameFolder - Monitoring is not active, ready for new folder.";
        updateStatusLabel(tr("Game folder updated. Ready to start monitoring."));
    }
}

// Add this new method to your LogDisplayWindow class
void LogDisplayWindow::playSystemSound() {
    soundPlayer->playConfiguredSound(logDisplay);
}

// Add this function to LogDisplayWindow class
QString LogDisplayWindow::getFriendlyGameModeName(const QString& rawMode) const {
    // Use the same transform function that's used in log parsing
    // This applies the exact same transforms defined in logfile_regex_rules.json
    std::string friendlyName = process_transforms(
        rawMode.toStdString(),
        get_transform_steps("friendly_game_mode_name")
    );
    
    return QString::fromStdString(friendlyName);
}

void LogDisplayWindow::updateWindowTitle(const QString& gameMode, const QString& subGameMode) {
    QString title = tr("Gunhead Log Display");

    // Get player name from transmitter
    QString playerName = transmitter.getCurrentPlayerName();
    
    // Debug logging to verify data
    qDebug() << "Title Update - Game Mode:" << gameMode 
             << "Sub-Game Mode:" << subGameMode
             << "Player Name:" << playerName;

    // Add player name to title
    if (!playerName.isEmpty() && playerName != "Unknown") {
        title += tr(" - Player: %1").arg(playerName);
    } else {
        title += tr(" - Player: [Unknown]");
    }

    // Add game mode information
    if (gameMode == "PU") {
        title += tr(" - Game Mode: Persistent Universe");
    } else if (gameMode == "AC") {
        title += tr(" - Game Mode: Arena Commander");
        if (!subGameMode.isEmpty()) {
            title += tr(" (%1)").arg(subGameMode);
        }
    } else if (!gameMode.isEmpty() && gameMode != "Unknown") {
        title += tr(" - Game Mode: %1").arg(gameMode);
    }

    // Set the window title
    setWindowTitle(title);
    if (titleBar) {
        titleBar->setTitle(title);
    }
    qDebug() << "Window title set to:" << title;
}

void LogDisplayWindow::retranslateUi() {
    // Update window title
    updateWindowTitle(transmitter.getCurrentGameMode(), transmitter.getCurrentSubGameMode());
    
    // Update filter dropdown text
    if (filterDropdown) {
        filterDropdown->setText(tr("Select Filters     ▼"));
        // Recalculate width after text and potentially font changes
        QTimer::singleShot(0, this, &LogDisplayWindow::updateFilterDropdownWidth);
    }
    
    // Update other UI elements
    if (playSoundCheckbox) {
        playSoundCheckbox->setText(tr("Play Sound"));
        playSoundCheckbox->setToolTip(tr("Coming soon: Play sound on events"));
    }
    
    // Update buttons
    if (testButton) testButton->setText(tr("Test"));
    
    // Check the current text to determine if monitoring is active
    if (monitoringButton) {
        bool isCurrentlyMonitoring = monitoringButton->text() == "Stop Monitoring";
        monitoringButton->setText(isCurrentlyMonitoring ? tr("Stop Monitoring") : tr("Start Monitoring"));
    }
    
    // Find and update buttons by object name
    QPushButton* clearBtn = findChild<QPushButton*>("clearButton");
    if (clearBtn) clearBtn->setText(tr("Clear"));
    
    QPushButton* textColorBtn = findChild<QPushButton*>("textColorButton");
    if (textColorBtn) textColorBtn->setText(tr("Text Color"));
    
    QPushButton* bgColorBtn = findChild<QPushButton*>("bgColorButton");
    if (bgColorBtn) bgColorBtn->setText(tr("Background Color"));
    
    QPushButton* testSoundBtn = findChild<QPushButton*>("testSoundButton");
    if (testSoundBtn) testSoundBtn->setText(tr("Test Sound"));
    
    qDebug() << "LogDisplayWindow UI retranslated";
}

void LogDisplayWindow::loadJsonRules() {
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
    QFile file(jsonFilePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Could not open JSON rules file:" << file.errorString();
        return;
    }
    
    QByteArray data = file.readAll();
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    
    if (parseError.error != QJsonParseError::NoError) {
        qWarning() << "Failed to parse JSON rules file:" << parseError.errorString();
        return;
    }
    
    rulesData = doc.object();
    qDebug() << "JSON rules loaded successfully";
}

bool LogDisplayWindow::shouldShowEventType(const QString& filterType) const {
    if (filterType == "kill") {
        // For kill events, we need to determine if it's PvP or PvE
        // This will be handled in the calling context since we need parsed data
        return true; // Let the caller handle PvP/PvE filtering
    } else if (filterType == "ship") {
        return showShips;
    } else if (filterType == "other") {
        return showOther;
    }
    
    // For events without filter_type, default to showing them
    return true;
}

// MODIFIED: formatEventFromTemplate - Only allow <a> tags, escape all other tags (including <b>, <u>, etc.)

QString LogDisplayWindow::formatEventFromTemplate(const QString& messageTemplate, const nlohmann::json& parsed) const {
    QString message = messageTemplate;

    for (auto it = parsed.begin(); it != parsed.end(); ++it) {
        QString placeholder = QString("{%1}").arg(QString::fromStdString(it.key()));
        QString value;

        if (it.value().is_string()) {
            value = QString::fromStdString(it.value().get<std::string>());
            
            // For player name fields, preserve hyperlinks but ensure they are properly contained
            if (it.key() == "victim" || it.key() == "killer" || it.key() == "driver" || it.key() == "cause") {
                // If the value contains HTML tags, ensure it's a complete, valid hyperlink structure
                if (value.contains("<a ") && value.contains("</a>")) {
                    // Value is already formatted as hyperlink, use as-is but validate structure
                    if (!value.startsWith("<a ") || !value.endsWith("</a>")) {
                        // Malformed hyperlink, escape it
                        value = value.toHtmlEscaped();
                    }
                } else {
                    // Not a hyperlink, escape any HTML
                    value = value.toHtmlEscaped();
                }
            } else {
                // For non-player fields, always escape HTML
                value = value.toHtmlEscaped();
            }
        } else if (it.value().is_number_integer()) {
            value = QString::number(it.value().get<int64_t>());
        } else if (it.value().is_number_float()) {
            value = QString::number(it.value().get<double>());
        } else if (it.value().is_boolean()) {
            value = it.value().get<bool>() ? "true" : "false";
        } else {
            value = QString::fromStdString(it.value().dump()).toHtmlEscaped();
        }

        message.replace(placeholder, value);
    }

    // Final safety check: ensure only complete hyperlink tags are preserved
    QRegularExpression completeHyperlinks(R"(<a\s+[^>]*href="[^"]*"[^>]*>.*?</a>)");
    QRegularExpression incompleteOrDanglingTags(R"(<(?!/?)(?:a\b|/a\b)[^>]*>|<[^>]*(?<!/)>(?![^<]*</[^>]*>))");
    
    // First pass: identify and temporarily replace complete hyperlinks
    QStringList hyperlinks;
    QString tempMessage = message;
    QRegularExpressionMatchIterator hyperlinkIt = completeHyperlinks.globalMatch(tempMessage);
    int placeholderIndex = 0;
    while (hyperlinkIt.hasNext()) {
        QRegularExpressionMatch match = hyperlinkIt.next();
        QString placeholder = QString("__HYPERLINK_%1__").arg(placeholderIndex++);
        hyperlinks.append(match.captured(0));
        tempMessage.replace(match.captured(0), placeholder);
    }
    
    // Second pass: escape any remaining HTML tags
    tempMessage.replace("<", "&lt;").replace(">", "&gt;");
    
    // Third pass: restore the complete hyperlinks
    for (int i = 0; i < hyperlinks.size(); ++i) {
        QString placeholder = QString("__HYPERLINK_%1__").arg(i);
        tempMessage.replace(placeholder, hyperlinks[i]);
    }

    return tr(tempMessage.toUtf8().constData());
}

FilterDropdownWidget::FilterDropdownWidget(QWidget* parent) 
    : QWidget(parent), updatingSelectAll(false) {
    
    // Set object name for theming
    setObjectName("filterDropdownWidget");
    
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(4);
    
    // Select All checkbox - remove hardcoded styling
    selectAllCheckbox = new QCheckBox(tr("Select All"), this);
    selectAllCheckbox->setObjectName("selectAllCheckbox"); // Add object name for theme targeting
    connect(selectAllCheckbox, &QCheckBox::toggled, this, &FilterDropdownWidget::onSelectAllChanged);
    layout->addWidget(selectAllCheckbox);
    
    // Separator line
    QFrame* line = new QFrame(this);
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    layout->addWidget(line);
    
    // Individual filter checkboxes - let them inherit theme styling
    pvpCheckbox = new QCheckBox(tr("Show PvP"), this);
    pvpCheckbox->setObjectName("pvpCheckbox");
    pvpCheckbox->setToolTip(tr("Show player vs player kill events"));
    connect(pvpCheckbox, &QCheckBox::toggled, this, &FilterDropdownWidget::onIndividualFilterChanged);
    layout->addWidget(pvpCheckbox);
    
    pveCheckbox = new QCheckBox(tr("Show PvE"), this);
    pveCheckbox->setObjectName("pveCheckbox");
    pveCheckbox->setToolTip(tr("Show player vs environment (NPC) kill events"));
    connect(pveCheckbox, &QCheckBox::toggled, this, &FilterDropdownWidget::onIndividualFilterChanged);
    layout->addWidget(pveCheckbox);
    
    shipsCheckbox = new QCheckBox(tr("Show Ship Events"), this);
    shipsCheckbox->setObjectName("shipsCheckbox");
    shipsCheckbox->setToolTip(tr("Show ship destruction and vehicle events"));
    connect(shipsCheckbox, &QCheckBox::toggled, this, &FilterDropdownWidget::onIndividualFilterChanged);
    layout->addWidget(shipsCheckbox);
    
    otherCheckbox = new QCheckBox(tr("Show Other Events"), this);
    otherCheckbox->setObjectName("otherCheckbox");
    otherCheckbox->setToolTip(tr("Show connections, seat changes, missions, etc."));
    connect(otherCheckbox, &QCheckBox::toggled, this, &FilterDropdownWidget::onIndividualFilterChanged);
    layout->addWidget(otherCheckbox);

    // NPC Names checkbox (separate from others)
    QFrame* line2 = new QFrame(this);
    line2->setFrameShape(QFrame::HLine);
    line2->setFrameShadow(QFrame::Sunken);
    layout->addWidget(line2);
    
    npcNamesCheckbox = new QCheckBox(tr("Show NPC Names"), this);
    npcNamesCheckbox->setObjectName("npcNamesCheckbox");
    npcNamesCheckbox->setToolTip(tr("Show actual NPC names in events or just 'NPC'"));
    connect(npcNamesCheckbox, &QCheckBox::toggled, this, &FilterDropdownWidget::onIndividualFilterChanged);
    layout->addWidget(npcNamesCheckbox);
}

void FilterDropdownWidget::setShowPvP(bool show) { pvpCheckbox->setChecked(show); }
void FilterDropdownWidget::setShowPvE(bool show) { pveCheckbox->setChecked(show); }
void FilterDropdownWidget::setShowShips(bool show) { shipsCheckbox->setChecked(show); }
void FilterDropdownWidget::setShowOther(bool show) { otherCheckbox->setChecked(show); }
void FilterDropdownWidget::setShowNPCNames(bool show) { npcNamesCheckbox->setChecked(show); }

bool FilterDropdownWidget::getShowPvP() const { return pvpCheckbox->isChecked(); }
bool FilterDropdownWidget::getShowPvE() const { return pveCheckbox->isChecked(); }
bool FilterDropdownWidget::getShowShips() const { return shipsCheckbox->isChecked(); }
bool FilterDropdownWidget::getShowOther() const { return otherCheckbox->isChecked(); }
bool FilterDropdownWidget::getShowNPCNames() const { return npcNamesCheckbox->isChecked(); }

void FilterDropdownWidget::onSelectAllChanged(bool checked) {
    if (updatingSelectAll) return;
    
    updatingSelectAll = true;
    pvpCheckbox->setChecked(checked);
    pveCheckbox->setChecked(checked);
    shipsCheckbox->setChecked(checked);
    otherCheckbox->setChecked(checked);
    // Note: NPC Names is independent of "Select All"
    updatingSelectAll = false;
    
    emit filterChanged();
}

void FilterDropdownWidget::onIndividualFilterChanged() {
    if (updatingSelectAll) return;
    
    // Update Select All checkbox based on individual states
    bool allChecked = pvpCheckbox->isChecked() && 
                     pveCheckbox->isChecked() && 
                     shipsCheckbox->isChecked() && 
                     otherCheckbox->isChecked();
    
    updatingSelectAll = true;
    selectAllCheckbox->setChecked(allChecked);
    updatingSelectAll = false;
    
    emit filterChanged();
}

void LogDisplayWindow::updateFilterDropdownWidth() {
    if (!filterDropdown) return;
    
    // Save the original stylesheet
    QString originalStyle = filterDropdown->styleSheet();
    
    // Temporarily clear any text alignment that might affect measurement
    filterDropdown->setStyleSheet("");
    
    // Recalculate width based on current font metrics
    QFontMetrics fm(filterDropdown->font());
    int textWidth = fm.horizontalAdvance(tr("Select Filters   ▼"));
    int padding = 24; // Increased padding for reliable display
    int calculatedWidth = textWidth + padding;

    // Update the button width
    filterDropdown->setMinimumWidth(calculatedWidth);
    filterDropdown->setMaximumWidth(calculatedWidth + 40); // More expansion room
    
    // Restore original style with text alignment explicitly set for dropdown
    filterDropdown->setStyleSheet(originalStyle + " QPushButton#filterDropdown { text-align: left; padding-left: 8px; }");
    
    qDebug() << "Filter dropdown width updated to:" << calculatedWidth << "based on font:" << filterDropdown->font().toString();
}

// Implement the new method

void LogDisplayWindow::setDebugModeEnabled(bool enabled)
{
    qDebug() << "LogDisplayWindow: Setting debug mode to" << enabled;
    
    // Show or hide the test buttons based on debug mode
    if (testButton) {
        testButton->setVisible(enabled);
    }
    
    // Find the test sound button and update its visibility
    QPushButton* testSoundBtn = findChild<QPushButton*>("testSoundButton");
    if (testSoundBtn) {
        testSoundBtn->setVisible(enabled);
    }
    
    // Update status label with debug mode change
    QString message = enabled ? 
        "Debug mode enabled - test buttons visible" : 
        "Debug mode disabled - test buttons hidden";
    updateStatusLabel(tr(message.toUtf8().constData()));
    
    // If debug mode is enabled, add a debug message to the log
    if (enabled) {
        QString debugInfo = QString(
            "Debug Info: Qt %1, App %2, OS: %3")
            .arg(QT_VERSION_STR)
            .arg(QCoreApplication::applicationVersion())
            .arg(QSysInfo::prettyProductName());
        
        QJsonObject debugObj;
        debugObj["identifier"] = "status";
        debugObj["message"] = debugInfo;
        
        addEvent(QJsonDocument(debugObj).toJson());
    }
}

void LogDisplayWindow::recalculateLayout() {
    // Force filter dropdown to update its width
    if (filterDropdown) {
        updateFilterDropdownWidth();
        
        // Force a second update after brief delay to ensure font is fully loaded
        QTimer::singleShot(100, this, &LogDisplayWindow::updateFilterDropdownWidth);
    }
}

void LogDisplayWindow::onThemeChanged() {
    recalculateLayout();
}

void LogDisplayWindow::showEvent(QShowEvent* event) {
    QMainWindow::showEvent(event);
    
    // Reapply window effects with a small delay to ensure window is ready
    QPointer<LogDisplayWindow> safeThis(this);
    QTimer::singleShot(50, this, [safeThis]() {
        if (safeThis) {
            CustomTitleBar::applyWindowEffects(safeThis);
        }
    });
}