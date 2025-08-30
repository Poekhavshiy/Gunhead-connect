#include "globals.h"
#include "logmonitor.h"
#include "log_parser.h"
#include "logger.h"
#include "FilterUtils.h"
#include <QTimer>
#include <QTextStream>
#include <QFileInfo> 
#include <QDateTime>
#include <sstream>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QCoreApplication>
#include <QStandardPaths>
#include <QDir>

LogMonitor::LogMonitor(QObject *parent) : QObject(parent), m_regex(".*"), m_lastReadPosition(0), m_gameModeLastPosition(0), m_isMonitoringEvents(false) {
    log_debug("LogMonitor", "LogMonitor instance created.");
}

void LogMonitor::startUnifiedMonitoring(const QString &filePath) {
    m_logFile.setFileName(filePath);
    if (m_logFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        log_info("LogMonitor", "Unified log monitoring started on: " + filePath.toStdString());
        
        // Seek to the end of the file to start from current position
        m_lastReadPosition = m_logFile.size();
        
        // Schedule the first check
        QTimer::singleShot(1000, this, SLOT(checkLogFileUnified()));
        
        if (m_isMonitoringEvents) {
            emit monitoringStarted();
        }
    } else {
        log_warn("LogMonitor", "Failed to open log file for monitoring: " + filePath.toStdString());
        if (m_isMonitoringEvents) {
            emit monitoringStopped("Failed to open log file.");
        }
    }
}

void LogMonitor::startMonitoring(const QString &filePath) {
    m_isMonitoringEvents = true;
    startUnifiedMonitoring(filePath);
}

void LogMonitor::stopMonitoring() {
    m_isMonitoringEvents = false;
    
    if (m_logFile.isOpen()) {
        m_logFile.close();
        log_info("LogMonitor", "Log file closed.");
    }
    
    emit monitoringStopped("Monitoring stopped by user.");
}

void LogMonitor::checkLogFile() {
    log_debug("LogMonitor", "checkLogFile called.");

    if (!m_logFile.isOpen()) {
        log_error("LogMonitor", "Log file is not open: " + m_logFile.fileName().toStdString());
        emit monitoringStopped("Log file is not open.");
        return;
    }

    // Check if the file has been truncated
    if (m_logFile.size() < m_lastReadPosition) {
        log_warn("LogMonitor", "Log file was truncated. Resetting read position.");
        m_lastReadPosition = 0; // Reset the last read position
    }

    // Seek to the last read position
    m_logFile.seek(m_lastReadPosition);
    QTextStream in(&m_logFile);
    log_debug("LogMonitor", "Reading log file from position: " + QString::number(m_lastReadPosition).toStdString());

    // Debug output current filter settings
    QSettings settings("KillApiConnect", "KillApiConnectPlus");
    bool showPvP = settings.value("LogDisplay/ShowPvP", false).toBool();
    bool showPvE = settings.value("LogDisplay/ShowPvE", true).toBool();
    bool showNPCNames = settings.value("LogDisplay/ShowNPCNames", true).toBool();
    log_debug("LogMonitor", "Current filter settings: showPvP=" + std::to_string(showPvP) + 
              ", showPvE=" + std::to_string(showPvE) + 
              ", showNPCNames=" + std::to_string(showNPCNames));

    while (!in.atEnd()) {
        QString line = in.readLine();
        log_debug("LogMonitor", "Read line: " + line.toStdString());

        if (m_regex.match(line).hasMatch()) {
            log_info("LogMonitor", "Line matched regex: " + line.toStdString());
            
            // Parse the log line using the log_parser
            std::string parsedJson = parse_log_line(line.toStdString());
            
            // Only process if it's not an empty JSON object
            if (parsedJson != "{}" && !parsedJson.empty()) {
                log_debug("LogMonitor", "Parsed JSON: " + parsedJson);
                
                QString jsonPayload = QString::fromStdString(parsedJson);
                
                // Apply filters using FilterUtils
                bool shouldDisplay = FilterUtils::shouldDisplayEvent(jsonPayload);
                log_debug("LogMonitor", "FilterUtils::shouldDisplayEvent returned: " + std::to_string(shouldDisplay));
                
                if (!shouldDisplay) {
                    log_info("LogMonitor", "Filtered out event - not emitting signal");
                    continue; // Skip to next log line
                }
                
                // Only process and emit events that pass the filter
                QString filteredPayload = FilterUtils::applyNPCNameFilter(jsonPayload);
                log_debug("LogMonitor", "Emitting filtered event: " + filteredPayload.toStdString());
                emit logLineMatched(filteredPayload);
            } else {
                log_debug("LogMonitor", "Skipping line - no matching pattern: " + line.toStdString());
            }
        } else {
            log_debug("LogMonitor", "Line did not match regex: " + line.toStdString());
        }
    }

    // Update the last read position
    m_lastReadPosition = m_logFile.pos();
    log_debug("LogMonitor", "Updated last read position to: " + QString::number(m_lastReadPosition).toStdString());

    // Schedule the next check
    QTimer::singleShot(1000, this, SLOT(checkLogFile()));
    log_debug("LogMonitor", "Next checkLogFile scheduled.");

    // Check for game mode changes in a cleaner way
    QString line = in.readLine();
    
    // Get regex patterns from configuration
    QRegularExpression puRegex = getGameModeRegex("game_mode_pu");
    QRegularExpressionMatch puMatch = puRegex.match(line);
    
    if (puMatch.hasMatch()) {
        QString newGameMode = "PU";
        QString newSubGameMode = "";
        
        // Only emit if there's a change
        if (QString::fromStdString(currentGameMode) != newGameMode || 
            QString::fromStdString(currentSubGameMode) != newSubGameMode) {
            
            // Update global variables
            currentGameMode = newGameMode.toStdString();
            currentSubGameMode = newSubGameMode.toStdString();
            
            emit gameModeSwitched(newGameMode, newSubGameMode);
            emit monitoringStopped(tr("Game mode changed, monitoring reset"));
        }
    }
    
    QRegularExpression acRegex = getGameModeRegex("game_mode_ac");
    QRegularExpressionMatch acMatch = acRegex.match(line);
    
    if (acMatch.hasMatch()) {
        QString newGameMode = "AC";
        QString rawSubGameMode = acMatch.captured(1);
        
        // Apply the transform from logfile_regex_rules.json
        std::string transformedSubMode = process_transforms(rawSubGameMode.toStdString(), 
                                                get_transform_steps("friendly_game_mode_name"));
        QString newSubGameMode = QString::fromStdString(transformedSubMode);
        
        // Only emit if there's a change
        if (QString::fromStdString(currentGameMode) != newGameMode || 
            QString::fromStdString(currentSubGameMode) != newSubGameMode.toStdString()) {
            
            // Update global variables
            currentGameMode = newGameMode.toStdString();
            currentSubGameMode = newSubGameMode.toStdString();
            
            emit gameModeSwitched(newGameMode, newSubGameMode);
            emit monitoringStopped("Game mode changed to AC: " + newSubGameMode + ", monitoring reset");
        }
    }
}

void LogMonitor::startGameModeTracking(const QString &filePath) {
    log_debug("LogMonitor", "startGameModeTracking called with filePath: " + filePath.toStdString());

    m_gameLogFile.setFileName(filePath);
    if (m_gameLogFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        log_info("LogMonitor", "Game mode tracking started on: " + filePath.toStdString());
        
        // Seek to the end of the file to start from current position
        m_gameModeLastPosition = m_gameLogFile.size();
        
        // Schedule the first check
        QTimer::singleShot(2000, this, SLOT(checkGameModeChanges()));
    } else {
        log_warn("LogMonitor", "Failed to open log file for game mode tracking: " + filePath.toStdString());
    }
}

void LogMonitor::checkGameModeChanges() {
    if (!m_gameLogFile.isOpen()) {
        // Try to re-open the file if it was closed
        if (!m_gameLogFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            log_warn("LogMonitor", "Cannot open log file for game mode tracking, will retry...");
            QTimer::singleShot(5000, this, SLOT(checkGameModeChanges()));
            return;
        }
    }

    // Check if the file has been truncated
    if (m_gameLogFile.size() < m_gameModeLastPosition) {
        log_warn("LogMonitor", "Game log file was truncated. Resetting read position.");
        m_gameModeLastPosition = 0;
    }

    // Seek to the last position
    m_gameLogFile.seek(m_gameModeLastPosition);
    QTextStream in(&m_gameLogFile);

    // Read new lines and check for game mode changes only
    while (!in.atEnd()) {
        QString line = in.readLine();
        
        // Get regex patterns from configuration
        QRegularExpression puRegex = getGameModeRegex("game_mode_pu");
        QRegularExpressionMatch puMatch = puRegex.match(line);
        
        QRegularExpression acRegex = getGameModeRegex("game_mode_ac");
        QRegularExpressionMatch acMatch = acRegex.match(line);
        
        // Process matches
        if (puMatch.hasMatch()) {
            updateGameMode("PU", "");
        } else if (acMatch.hasMatch()) {
            QString rawSubGameMode = acMatch.captured(1);
            std::string transformedSubMode = process_transforms(
                rawSubGameMode.toStdString(), 
                get_transform_steps("friendly_game_mode_name")
            );
            updateGameMode("AC", QString::fromStdString(transformedSubMode));
        }
    }
    
    // Update position and schedule next check
    m_gameModeLastPosition = m_gameLogFile.pos();
    QTimer::singleShot(2000, this, SLOT(checkGameModeChanges()));
}

void LogMonitor::updateGameMode(const QString& gameMode, const QString& subGameMode) {
    // Only emit if there's a change
    if (QString::fromStdString(currentGameMode) != gameMode || 
        QString::fromStdString(currentSubGameMode) != subGameMode) {
        
        // Update global variables
        currentGameMode = gameMode.toStdString();
        currentSubGameMode = subGameMode.toStdString();
        
        // Emit the signal
        emit gameModeSwitched(gameMode, subGameMode);
        
        log_info("LogMonitor", "Game mode changed to: " + gameMode.toStdString() + 
                 (subGameMode.isEmpty() ? "" : " - " + subGameMode.toStdString()));
    }
}

void LogMonitor::checkLogFileUnified() {
    if (!m_logFile.isOpen()) {
        if (!m_logFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            log_warn("LogMonitor", "Cannot open log file, will retry...");
            QTimer::singleShot(5000, this, SLOT(checkLogFileUnified()));
            return;
        }
    }

    // Check if the file has been truncated
    if (m_logFile.size() < m_lastReadPosition) {
        log_warn("LogMonitor", "Log file was truncated. Resetting read position.");
        m_lastReadPosition = 0;
    }

    // Seek to the last position
    m_logFile.seek(m_lastReadPosition);
    QTextStream in(&m_logFile);

    // Read and process new lines
    while (!in.atEnd()) {
        QString line = in.readLine();
        log_debug("LogMonitor", "Read line: " + line.toStdString());

        // Process player info lines
        if (line.contains("AccountLoginCharacterStatus_Character")) {
            processLogLine(line);
        }

        // Process game mode changes
        checkForGameModeChanges(line);

        // Process other log events
        if (m_isMonitoringEvents) {
            processLine(line);
        }
    }

    // Update position and schedule next check
    m_lastReadPosition = m_logFile.pos();
    QTimer::singleShot(1000, this, SLOT(checkLogFileUnified()));
}

// Helper method to process a single line
void LogMonitor::processLine(const QString& line) {
    if (line.trimmed().isEmpty()) {
        return;
    }
    
    // Use the existing log parser for all pattern matching
    std::string result = parse_log_line(line.toStdString());
    
    // Check for player character info from the result
    if (!result.empty() && result != "{}") {
        QString jsonResult = QString::fromStdString(result);
        
        // Extract player info if present in the parsed result
        QJsonDocument doc = QJsonDocument::fromJson(jsonResult.toUtf8());
        QJsonObject obj = doc.object();
        
        if (obj["identifier"].toString() == "player_character_info") {
            QString playerName = obj["character_name"].toString();
            QString playerGEID = obj["geid"].toString();
            
            qDebug() << "Detected player login - Name:" << playerName << "GEID:" << playerGEID;
            emit playerInfoDetected(playerName, playerGEID);
        }
        
        log_debug("LogMonitor", "Matched line: " + jsonResult.toStdString());
        emit logLineMatched(jsonResult);
    }
}

// Helper method to check for game mode changes
void LogMonitor::checkForGameModeChanges(const QString& line) {
    // Get regex patterns from configuration
    QRegularExpression puRegex = getGameModeRegex("game_mode_pu");
    QRegularExpressionMatch puMatch = puRegex.match(line);
    
    QRegularExpression acRegex = getGameModeRegex("game_mode_ac");
    QRegularExpressionMatch acMatch = acRegex.match(line);
    
    if (puMatch.hasMatch()) {
        updateGameMode("PU", "");
    } else if (acMatch.hasMatch()) {
        QString rawSubGameMode = acMatch.captured(1);
        std::string transformedSubMode = process_transforms(
            rawSubGameMode.toStdString(), 
            get_transform_steps("friendly_game_mode_name")
        );
        updateGameMode("AC", QString::fromStdString(transformedSubMode));
    }
}

QRegularExpression LogMonitor::getGameModeRegex(const QString& identifier) const {
    // First get the pattern from getGameModePattern
    QString pattern = getGameModePattern(identifier);
    
    // Return a compiled regex with the pattern
    return QRegularExpression(pattern);
}

// Add the implementation of getGameModePattern method
QString LogMonitor::getGameModePattern(const QString& identifier) const {
    // Load patterns from JSON file
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
        log_warn("LogMonitor", "Failed to open logfile_regex_rules.json");
        return getDefaultPattern(identifier);
    }
    
    QByteArray jsonData = file.readAll();
    file.close();
    
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(jsonData, &parseError);
    
    if (parseError.error != QJsonParseError::NoError) {
        log_warn("LogMonitor", "JSON parse error: " + parseError.errorString().toStdString());
        return getDefaultPattern(identifier);
    }
    
    QJsonObject json = doc.object();
    
    // Find the pattern in the rules array
    if (json.contains("rules")) {
        QJsonArray rules = json["rules"].toArray();
        
        for (const QJsonValue& ruleValue : rules) {
            if (ruleValue.isObject()) {
                QJsonObject rule = ruleValue.toObject();
                if (rule["identifier"] == identifier) {
                    return rule["pattern"].toString();
                }
            }
        }
    }
    
    log_warn("LogMonitor", "Pattern not found for: " + identifier.toStdString());
    return getDefaultPattern(identifier);
}

// Add the getDefaultPattern implementation
QString LogMonitor::getDefaultPattern(const QString& identifier) const {
    // Fallback patterns
    if (identifier == "game_mode_pu") {
        return "\\[\\+\\] \\[CIG\\] \\{Join PU\\}";
    } else if (identifier == "game_mode_ac") {
        return "\\[EALobby\\]\\[CEALobby::SetGameModeId\\].+?Changing game mode from.+?to (EA_\\w+)";
    } else if (identifier == "player_character_info") {
        return "\\[\\+\\] PlayerName: ([^,]+), GEID: ([a-fA-F0-9\\-]+)";
    }
    return "";
}

// Fix initializeGameMode implementation:
void LogMonitor::initializeGameMode() {
    if (!gameLogFilePath.isEmpty() && QFile::exists(gameLogFilePath)) {
        qDebug() << "Initializing game mode and player info from log file:" << gameLogFilePath;

        auto [mode, subMode, playerName, playerGEID] = detectGameModeAndPlayerFast(gameLogFilePath.toStdString());
        currentGameMode = mode;
        currentSubGameMode = subMode;

        qDebug() << "Detected game mode:" << QString::fromStdString(currentGameMode)
                 << "Sub-mode:" << QString::fromStdString(currentSubGameMode)
                 << "Player name:" << QString::fromStdString(playerName)
                 << "Player GEID:" << QString::fromStdString(playerGEID);

        emit gameModeSwitched(QString::fromStdString(currentGameMode), QString::fromStdString(currentSubGameMode));

        if (!playerName.empty() && !playerGEID.empty()) {
            qDebug() << "Emitting playerInfoChanged signal with Name:" << QString::fromStdString(playerName)
                     << "GEID:" << QString::fromStdString(playerGEID);
            emit playerInfoDetected(QString::fromStdString(playerName), QString::fromStdString(playerGEID));
        }
    } else {
        qDebug() << "Game log file not found, cannot initialize game mode or player info:" << gameLogFilePath;
    }
}

void LogMonitor::processLogLine(const QString& line) {
    if (line.contains("AccountLoginCharacterStatus_Character")) {
        qDebug() << "Found character line:" << line;

        QString pattern = getGameModePattern("player_character_info");
        QRegularExpression regex(pattern);
        QRegularExpressionMatch match = regex.match(line);

        qDebug() << "Player regex pattern:" << pattern;
        qDebug() << "Match valid:" << match.hasMatch() << "with" << match.capturedTexts().size() << "captures";
        
        if (match.hasMatch()) {
            // Debug all captures
            for (int i = 0; i < match.capturedTexts().size(); i++) {
                qDebug() << "  Capture" << i << ":" << match.captured(i);
            }
            
            QString playerName = match.captured(3);
            QString playerGEID = match.captured(2);

            qDebug() << "Detected player info - Name:" << playerName << ", GEID:" << playerGEID;
            emit playerInfoDetected(playerName, playerGEID);
            qDebug() << "Emitted playerInfoDetected signal";
        } else {
            qDebug() << "No match found for player info pattern";
        }
    }
}
