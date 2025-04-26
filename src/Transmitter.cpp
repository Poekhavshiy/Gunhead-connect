#include "Transmitter.h"
#include <QNetworkRequest>
#include <QJsonObject>
#include <QJsonDocument>
#include <QDebug>
#include <QEventLoop>
#include <QTimer>
#include <QMutexLocker>
#include <QQueue>
#include <QThread>
#include <QCoreApplication>
#include <QRegularExpression>
#include <QDateTime>

Transmitter::Transmitter(QObject* parent)
    : QObject(parent), networkManager(new QNetworkAccessManager(this)) {
    qDebug() << "Transmitter object thread:" << this->thread();
    qDebug() << "Main thread:" << QCoreApplication::instance()->thread();
}

QString Transmitter::getApiServerUrl() const {
    QString url = debugState ? debugApiServerUrl : apiServerUrl;
    qDebug() << "API Server URL:" << url;
    return url;
}
Transmitter& Transmitter::getInstance() {
    qDebug() << "Transmitter instance created.";
    static Transmitter instance;
    return instance;
}


bool Transmitter::sendDebugPing(const QString& logFilePath, const QString& apiKey) {
    QJsonObject payload;
    payload["identifier"] = "debug_ping";
    payload["log_file_path"] = logFilePath;
    payload["api_key"] = apiKey;

    QNetworkRequest request{QUrl(getApiServerUrl())};
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("X-API-Key", apiKey.toUtf8());

    QNetworkReply* reply = networkManager->post(request, QJsonDocument(payload).toJson());
    QEventLoop loop;
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    if (reply->error() == QNetworkReply::NoError) {
        qDebug() << "Debug ping successful:" << reply->readAll();
        reply->deleteLater();
        return true;
    } else {
        qWarning() << "Debug ping failed:" << reply->errorString();
        reply->deleteLater();
        return false;
    }
}

bool Transmitter::sendConnectionSuccess(const QString& logFilePath, const QString& apiKey) {
    QJsonObject payload;
    payload["identifier"] = "connection_success";
    payload["log_file_path"] = logFilePath;
    payload["api_key"] = apiKey;

    QNetworkRequest request{QUrl(getApiServerUrl())};
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("X-API-Key", apiKey.toUtf8());

    QNetworkReply* reply = networkManager->post(request, QJsonDocument(payload).toJson());
    QEventLoop loop;
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    if (reply->error() == QNetworkReply::NoError) {
        qDebug() << "Connection success event sent:" << reply->readAll();
        reply->deleteLater();
        return true;
    } else {
        qWarning() << "Failed to send connection success event:" << reply->errorString();
        reply->deleteLater();
        return false;
    }
}

void Transmitter::sendEvent(const QJsonObject& event, const QString& apiKey) {
    qDebug() << "sendEvent called with event:" << event;

    QNetworkRequest request{QUrl(getApiServerUrl())};
    qDebug() << "API URL:" << getApiServerUrl();
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("X-API-Key", apiKey.toUtf8());

    QNetworkReply* reply = networkManager->post(request, QJsonDocument(event).toJson());
    qDebug() << "Network request sent.";

    connect(reply, &QNetworkReply::finished, [reply]() {
        if (reply->error() == QNetworkReply::NoError) {
            qDebug() << "Event sent successfully:" << reply->readAll();
        } else {
            qWarning() << "Failed to send event:" << reply->errorString();
        }
        reply->deleteLater();
    });

    connect(reply, &QNetworkReply::errorOccurred, [reply](QNetworkReply::NetworkError code) {
        qWarning() << "Network error occurred:" << code << reply->errorString();
    });
}

void Transmitter::enqueueLog(const QString& log) {
    // Check if this is already valid JSON
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(log.toUtf8(), &error);
    QString logToEnqueue;

    if (error.error == QJsonParseError::NoError && !doc.isEmpty()) {
        // Already valid JSON
        qDebug() << "Enqueueing pre-parsed JSON:" << log;
        logToEnqueue = log;
    } else {
        // Raw log line that needs parsing
        qDebug() << "Parsing raw log line:" << log;
        std::string parsedJson = parse_log_line(log.toStdString());
        if (parsedJson == "{}" || parsedJson.empty()) {
            qDebug() << "Skipping log - would produce empty JSON:" << log;
            return; // Skip this log entry entirely
        }
        logToEnqueue = QString::fromStdString(parsedJson);
    }

    qDebug() << "Attempting to lock queueMutex in enqueueLog.";
    QMutexLocker locker(&queueMutex);
    qDebug() << "queueMutex locked in enqueueLog.";
    logQueue.enqueue(logToEnqueue);
    qDebug() << "Log enqueued. Log queue size:" << logQueue.size();

    // Emit the signal only if not already processing
    static bool processing = false;
    if (!processing) {
        processing = true;
        emit logQueueUpdated();
        QTimer::singleShot(0, this, [&]() { processing = false; });
    }
}

void Transmitter::processQueueInThread(const QString& apiKey) {
    QThread* thread = QThread::create([this, apiKey]() {
        qDebug() << "Transmitter thread:" << QThread::currentThread();
        qDebug() << "Processing queue with API key:" << apiKey.left(5) + "...";
        
        while (true) {
            QString log;
            {
                QMutexLocker locker(&queueMutex);
                if (logQueue.isEmpty()) {
                    qDebug() << "Log queue is empty, exiting thread.";
                    break;
                }
                log = logQueue.dequeue();
                qDebug() << "Dequeued log:" << log;
            }

            // Check if this is valid JSON - it should be since enqueueLog already checked
            QJsonParseError error;
            QJsonDocument doc = QJsonDocument::fromJson(log.toUtf8(), &error);
            
            if (error.error != QJsonParseError::NoError || doc.isEmpty()) {
                qWarning() << "Invalid JSON in queue, attempting to parse again:" << log;
                std::string parsedJson = parse_log_line(log.toStdString());
                if (parsedJson == "{}" || parsedJson.empty()) {
                    qWarning() << "Failed to parse log, skipping:" << log;
                    continue;
                }
                log = QString::fromStdString(parsedJson);
                doc = QJsonDocument::fromJson(log.toUtf8());
            }
            
            QJsonObject event = doc.object();
            if (event.isEmpty()) {
                qWarning() << "Empty JSON object, skipping:" << log;
                continue;
            }

            qDebug() << "Processing event identifier:" << event["identifier"].toString();
            
            // First notify that a log has been processed
            emit logProcessed(log);
            
            // Then send the event to the API
            QMetaObject::invokeMethod(this, "sendEvent", Qt::QueuedConnection,
                                      Q_ARG(QJsonObject, event),
                                      Q_ARG(QString, apiKey));
        }
    });
    
    thread->start();
    connect(thread, &QThread::finished, thread, &QThread::deleteLater);
}

void Transmitter::processDirectJson(const QString& jsonStr, const QString& apiKey) {
    if (jsonStr.isEmpty() || jsonStr == "{}") {
        qWarning() << "Ignoring empty JSON";
        return;
    }

    QJsonObject event = QJsonDocument::fromJson(jsonStr.toUtf8()).object();
    if (event.isEmpty()) {
        qWarning() << "Failed to parse JSON:" << jsonStr;
        return;
    }

    qDebug() << "Processing direct JSON:" << event;
    emit logProcessed(jsonStr); // Notify that a log has been processed

    // Send the event to the API server
    sendEvent(event, apiKey);
}