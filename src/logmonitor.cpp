#include "globals.h"
#include "logmonitor.h"
#include "log_parser.h"
#include "logger.h"
#include "FilterUtils.h"
#include <QTimer>
#include <QTextStream>

LogMonitor::LogMonitor(QObject *parent) : QObject(parent), m_regex(".*"), m_lastReadPosition(0) {
    log_debug("LogMonitor", "LogMonitor instance created.");
}

void LogMonitor::startMonitoring(const QString &filePath) {
    log_debug("LogMonitor", "startMonitoring called with filePath: " + filePath.toStdString());

    m_logFile.setFileName(filePath);
    if (m_logFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        log_info("LogMonitor", "Log monitoring started on: " + filePath.toStdString());
        log_debug("LogMonitor", "Log file opened successfully: " + filePath.toStdString());

        if (ISDEBUG) {
            // Read the last 10 lines of the file
            QTextStream in(&m_logFile);
            QStringList lastLines;
            m_logFile.seek(0); // Start from the beginning
            while (!in.atEnd()) {
                lastLines.append(in.readLine());
                if (lastLines.size() > DEBUGLINES) {
                    lastLines.removeFirst(); // Keep only the last 10 lines
                }
            }

            log_debug("LogMonitor", "Last 10 lines of the log file:");
            for (const QString &line : lastLines) {
                log_debug("LogMonitor", line.toStdString());
                if (m_regex.match(line).hasMatch()) {
                    std::string parsedJson = parse_log_line(line.toStdString());
                    if (parsedJson != "{}" && !parsedJson.empty()) {
                        log_debug("LogMonitor", "Parsed JSON (debug): " + parsedJson);
                        QString jsonPayload = QString::fromStdString(parsedJson);

                        // --- Apply filtering here ---
                        bool shouldDisplay = FilterUtils::shouldDisplayEvent(jsonPayload);
                        log_debug("LogMonitor", "FilterUtils::shouldDisplayEvent returned: " + std::to_string(shouldDisplay));
                        if (!shouldDisplay) {
                            log_info("LogMonitor", "Filtered out event (debug lines) - not emitting signal");
                            continue;
                        }
                        QString filteredPayload = FilterUtils::applyNPCNameFilter(jsonPayload);
                        log_debug("LogMonitor", "Emitting filtered event (debug lines): " + filteredPayload.toStdString());
                        emit logLineMatched(filteredPayload);
                    }
                }
            }
        }

        // Seek to the end of the file to start monitoring new lines
        m_lastReadPosition = m_logFile.size();
        log_debug("LogMonitor", "Starting monitoring from end of file at position: " + QString::number(m_lastReadPosition).toStdString());

        emit monitoringStarted();

        // Trigger the first check
        QTimer::singleShot(1000, this, SLOT(checkLogFile()));
        log_debug("LogMonitor", "Initial checkLogFile scheduled.");
    } else {
        log_warn("LogMonitor", "Failed to open log file: " + filePath.toStdString());
        emit monitoringStopped("Failed to open log file.");
    }
}

void LogMonitor::stopMonitoring() {
    log_debug("LogMonitor", "Stopping log monitoring.");

    if (m_logFile.isOpen()) {
        m_logFile.close(); // Close the log file
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
}
