#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QJsonObject>
#include <QJsonDocument>
#include <QQueue>
#include <QMutex>
#include <QNetworkReply>
#include "log_parser.h"

class Transmitter : public QObject {
    Q_OBJECT

public:
    static Transmitter& getInstance();

    explicit Transmitter(QObject* parent = nullptr);

    bool sendDebugPing(const QString& logFilePath, const QString& apiKey);
    bool sendConnectionSuccess(const QString& logFilePath, const QString& apiKey);
    void enqueueLog(const QString& log);
    void processQueueInThread(const QString& apiKey);

    QQueue<QString> logQueue;

public slots:
    void sendEvent(const QJsonObject& event, const QString& apiKey);

signals:
    void logQueueUpdated();
    void logProcessed(const QString& log);

private:
    QNetworkAccessManager* networkManager;
    const QString apiServerUrl = "https://gunhead.sparked.network/api/interaction";
    const QString debugApiServerUrl = "https://bagman.sparked.network/api/interaction";

    QMutex queueMutex;
    bool debugState = false;

    QString getApiServerUrl() const;
    void processDirectJson(const QString& jsonStr, const QString& apiKey);
};