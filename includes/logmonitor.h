#pragma once

#include <QObject>
#include <QFile>
#include <QRegularExpression>
#include <QStringList>

class LogMonitor : public QObject {
    Q_OBJECT

public:
    explicit LogMonitor(QObject *parent = nullptr);
    void startMonitoring(const QString &filePath);
    void stopMonitoring();

signals:
    void monitoringStarted();
    void monitoringStopped(const QString &reason);
    void logLineMatched(const QString& jsonPayload);

private slots:
    void checkLogFile();

private:
    QFile m_logFile;
    QRegularExpression m_regex;
    qint64 m_lastReadPosition = 0; // Track the last read position in the file
    int DEBUGLINES = 15;
};
