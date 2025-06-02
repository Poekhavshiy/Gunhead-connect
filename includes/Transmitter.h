#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QJsonObject>
#include <QJsonDocument>
#include <QQueue>
#include <QMutex>
#include <QNetworkReply>
#include <QJsonArray>
#include "log_parser.h"
#include "globals.h"

class Transmitter : public QObject {
    Q_OBJECT

public:
    static Transmitter& getInstance();

    explicit Transmitter(QObject* parent = nullptr);

    bool sendDebugPing(const QString& apiKey);
    bool sendConnectionSuccess(const QString& logFilePath, const QString& apiKey);
    bool sendGameMode(const QString& apiKey); // New function for sending game mode
    void enqueueLog(const QString& log);
    void processQueueInThread(const QString& apiKey);
    
    void handleNetworkError(QNetworkReply::NetworkError error, const QString& errorString);
    
    // Error types enum for specific error handling
    enum class ApiErrorType {
        None,
        PanelClosed,
        InvalidApiKey,
        NetworkError,
        Other
    };

    QQueue<QString> logQueue;

public slots:
    void sendEvent(const QJsonObject& event, const QString& apiKey);

signals:
    void logQueueUpdated();
    void logProcessed(const QString& log);
    void apiError(Transmitter::ApiErrorType errorType, const QString& errorMessage);
    void panelClosedError();
    void invalidApiKeyError();
    void monitoringStopped(const QString& reason); // New signal for monitoring stopped
    void apiConnectionError(const QString& errorMessage); // New signal for API connection errors
    void gameModeChanged(const QString& gameMode, const QString& subGameMode); // Add this line
    void playerInfoChanged(const QString& playerName, const QString& playerGEID);

private:
    QNetworkAccessManager* networkManager;
    const QString apiServerUrl = "https://gunhead.sparked.network/api/interaction";
    const QString debugApiServerUrl = "https://bagman.sparked.network/api/interaction";

    QMutex queueMutex;

    QString getApiServerUrl() const;
    void processDirectJson(const QString& jsonStr, const QString& apiKey);

    // Helper method to parse API errors
    ApiErrorType identifyApiError(QNetworkReply* reply, const QByteArray& responseData);

    // Settings change tracking
    struct SettingsChange {
        QString type;       // "theme", "language", or "sound"
        QString value;      // The new setting value
        QDateTime timestamp; // When the change was made
    };
    
    QList<SettingsChange> settingsChanges; // Queue of settings changes
    QMutex settingsChangesMutex;           // Mutex to protect the queue

public:
    // Add to the public section
    void addSettingsChange(const QString& type, const QString& value);
    void clearSettingsChanges();
    void updateGameMode(const QString& identifier, const QString& logFilePath, bool isMonitoring);
    void updatePlayerInfo(const QString& playerName, const QString& playerGEID);

    // Getter methods for player and game mode information
    QString getCurrentPlayerName() const;
    QString getCurrentPlayerGEID() const;
    QString getCurrentGameMode() const;
    QString getCurrentSubGameMode() const;

private:
    QString currentPlayerName;
    QString currentPlayerGEID;
    QString currentGameMode;
    QString currentSubGameMode;
};