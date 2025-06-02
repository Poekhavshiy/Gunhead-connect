#pragma once

#include <QObject>
#include <QFile>
#include <QRegularExpression>
#include <QStringList>
#include <QDateTime>

class LogMonitor : public QObject {
    Q_OBJECT
    
public:
    explicit LogMonitor(QObject *parent = nullptr);
    
    void startMonitoring(const QString &filePath);
    void stopMonitoring();
    void startGameModeTracking(const QString &filePath);
    void startUnifiedMonitoring(const QString &filePath);
    void updateGameMode(const QString& gameMode, const QString& subGameMode);
    void initializeGameMode(); // Add this method declaration
    void processLogLine(const QString& line);
    
signals:
    void logLineMatched(const QString &jsonPayload);
    void monitoringStarted();
    void monitoringStopped(const QString &reason);
    void gameModeSwitched(const QString& gameMode, const QString& subGameMode);
    void playerInfoDetected(const QString& playerName, const QString& playerGEID);
    
private slots:
    void checkLogFile();
    void checkGameModeChanges();
    void checkLogFileUnified();
    
private:
    void processLine(const QString& line);
    void checkForGameModeChanges(const QString& line);

    QFile m_logFile;
    QFile m_gameLogFile;
    QRegularExpression m_regex;
    qint64 m_lastReadPosition;
    qint64 m_gameModeLastPosition;
    bool m_isMonitoringEvents;
    QDateTime m_lastCheckTime;
    QString gameLogFilePath; // Add this member variable
    
    // Add these method declarations
    QString getGameModePattern(const QString& identifier) const;
    QString getDefaultPattern(const QString& identifier) const;
    QRegularExpression getGameModeRegex(const QString& identifier) const;
};
