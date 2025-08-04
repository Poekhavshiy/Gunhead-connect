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
#include <QSettings>
#include <QLocale>
#include <QTimeZone>

Transmitter::Transmitter(QObject* parent)
    : QObject(parent), networkManager(new QNetworkAccessManager(this)) {
    qDebug() << "Transmitter object thread:" << this->thread();
    qDebug() << "Main thread:" << QCoreApplication::instance()->thread();
    qDebug() << "Transmitter initialized with Debug State:" << ISDEBUG;
}

QString Transmitter::getApiServerUrl() const {
    QString url = ISDEBUG ? debugApiServerUrl : apiServerUrl;
    qDebug() << "API Server URL:" << url;
    return url;
}
Transmitter& Transmitter::getInstance() {
    qDebug() << "Transmitter instance created.";
    static Transmitter instance;
    return instance;
}

QDateTime Transmitter::getNextAllowedPingTime() const {
    QMutexLocker locker(&lastEventMutex);
    if (!lastEventSentTime.isValid()) return QDateTime();
    return lastEventSentTime.addSecs(1800);
}

bool Transmitter::sendDebugPing(const QString& apiKey, bool force /* = false */) {
    QMutexLocker locker(&lastEventMutex);
    QDateTime now = QDateTime::currentDateTimeUtc();
    if (!force && lastEventSentTime.isValid() && lastEventSentTime.secsTo(now) < 1800) {
        qDebug() << "Debug ping skipped: only" << lastEventSentTime.secsTo(now) << "seconds since last event.";
        return false;
    }
    lastEventSentTime = now;
    locker.unlock();

    QJsonObject payload;
    payload["identifier"] = "debug_ping";
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
    {
        QMutexLocker locker(&lastEventMutex);
        lastEventSentTime = QDateTime::currentDateTimeUtc();
    }

    QJsonObject payload;
    payload["identifier"] = "connection_success";
    payload["log_file_path"] = logFilePath;
    payload["api_key"] = apiKey;

    // Version number and app version
    payload["app_version"] = QCoreApplication::applicationVersion();

    // Theme name
    QSettings settings("KillApiConnect", "KillApiConnectPlus");
    payload["theme"] = settings.value("currentTheme", "unknown").toString();

    // Sound file
    payload["soundfile"] = settings.value("LogDisplay/SoundFile", "unknown").toString();

    // Selected language - convert language name to locale code for API
    QString languageName = settings.value("Language/CurrentLanguage", "English").toString();
    QString localeCode = "en"; // default
    if (languageName == "繁體中文") localeCode = "zh";
    else if (languageName == "日本語") localeCode = "ja";
    else if (languageName == "Українська") localeCode = "uk";
    else if (languageName == "Русский" || languageName == "москаль") localeCode = "ru";
    else if (languageName == "Deutsch") localeCode = "de";
    else if (languageName == "Español") localeCode = "es";
    else if (languageName == "Français") localeCode = "fr";
    else if (languageName == "Italiano") localeCode = "it";
    else if (languageName == "Polski") localeCode = "pl";
    else if (languageName == "Português") localeCode = "pt";
    else if (languageName == "한국어") localeCode = "ko";
    payload["selected_language"] = localeCode;

    // Add settings changes if available
    {
        QMutexLocker locker(&settingsChangesMutex);
        if (!settingsChanges.isEmpty()) {
            QJsonArray changesArray;
            
            for (const SettingsChange& change : settingsChanges) {
                QJsonObject changeObj;
                changeObj["type"] = change.type;
                changeObj["value"] = change.value;
                changeObj["timestamp"] = change.timestamp.toString(Qt::ISODate);
                changesArray.append(changeObj);
            }
            
            payload["settings_changes"] = changesArray;
            qDebug() << "Added" << settingsChanges.size() << "settings changes to connection payload";
        }
    }

    // System locale - use global variable instead of QLocale::system().name()
    payload["system_locale"] = QString::fromStdString(systemLocale);

    // System timezone
    payload["system_timezone"] = QString::fromUtf8(QTimeZone::systemTimeZone().id());

    // Operating system name
#if defined(Q_OS_WIN)
    payload["os_name"] = "Windows";
#elif defined(Q_OS_MAC)
    payload["os_name"] = "macOS";
#elif defined(Q_OS_LINUX)
    payload["os_name"] = "Linux";
#else
    payload["os_name"] = "Unknown";
#endif

    // In-game name (default to TestUser)
    payload["in_game_name"] = currentPlayerName;
    payload["player_geid"] = currentPlayerGEID;
    payload["game_mode"] = currentGameMode;
    payload["sub_game_mode"] = currentSubGameMode;

    QNetworkRequest request{QUrl(getApiServerUrl())};
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("X-API-Key", apiKey.toUtf8());

    QNetworkReply* reply = networkManager->post(request, QJsonDocument(payload).toJson());
    QEventLoop loop;
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    if (reply->error() == QNetworkReply::NoError) {
        qDebug() << "Connection success event sent:" << reply->readAll();
        
        // Clear settings changes after successful send
        clearSettingsChanges();
        
        reply->deleteLater();
        return true;
    } else {
        QByteArray responseData = reply->readAll();
        qWarning() << "Failed to send connection success event:" << reply->errorString();
        qWarning() << "Server response:" << (responseData.isEmpty() ? "[Empty]" : responseData);
        
        // Identify and emit specific error
        ApiErrorType errorType = identifyApiError(reply, responseData);
        
        switch (errorType) {
            case ApiErrorType::PanelClosed:
                emit panelClosedError();
                emit apiError(ApiErrorType::PanelClosed, tr("The Gunhead panel for this server has been closed or doesn't exist."));
                break;
            case ApiErrorType::InvalidApiKey:
                emit invalidApiKeyError();
                emit apiError(ApiErrorType::InvalidApiKey, tr("Invalid API key. Please check your API key and try again."));
                break;
            default:
                emit apiError(ApiErrorType::Other, reply->errorString());
                break;
        }
        
        reply->deleteLater();
        return false;
    }
}

void Transmitter::sendEvent(const QJsonObject& event, const QString& apiKey) {
    {
        QMutexLocker locker(&lastEventMutex);
        lastEventSentTime = QDateTime::currentDateTimeUtc();
    }

    // Create a mutable copy of the event
    QJsonObject validatedEvent = event;
    
    // Check identifier for events we want to filter out
    QString identifier = validatedEvent["identifier"].toString();
    
    // Skip game mode related events - don't send them to API
    if (identifier == "game_mode_pu" || identifier == "game_mode_ac" || 
        identifier == "game_mode_change" || identifier == "game_mode_update") {
        qDebug() << "Skipping game mode event (local only):" << identifier;
        return; // Exit method without sending
    }
    
    // Keep the sanitization logic for game mode data in other events
    if (identifier == "game_mode_pu" || identifier == "game_mode_ac") {
        QString gameMode = validatedEvent["game_mode"].toString();
        
        // Check if game_mode contains a full log line (has timestamps, brackets, etc.)
        if (gameMode.contains("[") || gameMode.contains("<") || gameMode.contains("T") || gameMode.length() > 10) {
            // Fix the game mode value
            if (identifier == "game_mode_pu") {
                validatedEvent["game_mode"] = "PU";
            } else if (identifier == "game_mode_ac") {
                validatedEvent["game_mode"] = "AC";
            }
            
            qDebug() << "Fixed malformed game mode payload:" << gameMode << "->" << validatedEvent["game_mode"].toString();
        }
        
        // Add "game_mode_change" as the identifier for consistent API handling
        validatedEvent["identifier"] = "game_mode_change";
    }
    
    qDebug() << "sendEvent called with event:" << validatedEvent;

    QNetworkRequest request{QUrl(getApiServerUrl())};
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("X-API-Key", apiKey.toUtf8());

    QNetworkReply* reply = networkManager->post(request, QJsonDocument(validatedEvent).toJson());
    qDebug() << "Network request sent.";

    connect(reply, &QNetworkReply::finished, [this, reply]() {
        // Always read the full response regardless of error state
        QByteArray responseData = reply->readAll();
        int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        
        if (reply->error() == QNetworkReply::NoError) {
            qDebug() << "Event sent successfully:" << responseData;
        } else {
            qWarning() << "Failed to send event: HTTP" << statusCode 
                      << "Error:" << reply->error() << "-" << reply->errorString();
            qWarning() << "Server response:" << (responseData.isEmpty() ? "[Empty]" : responseData);
            
            // Log response headers for additional context
            qWarning() << "Response headers:";
            const QList<QByteArray> headerList = reply->rawHeaderList();
            for (const QByteArray &header : headerList) {
                qWarning() << " -" << header << ":" << reply->rawHeader(header);
            }
            
            // Identify and emit specific error
            ApiErrorType errorType = identifyApiError(reply, responseData);
            
            switch (errorType) {
                case ApiErrorType::PanelClosed:
                    emit panelClosedError();
                    emit apiError(ApiErrorType::PanelClosed, tr("The Gunhead panel for this server has been closed or doesn't exist."));
                    break;
                case ApiErrorType::InvalidApiKey:
                    emit invalidApiKeyError();
                    emit apiError(ApiErrorType::InvalidApiKey, tr("Invalid API key. Please check your API key and try again."));
                    break;
                default:
                    emit apiError(ApiErrorType::Other, reply->errorString());
                    break;
            }
        }
        reply->deleteLater();
    });

    connect(reply, &QNetworkReply::errorOccurred, [reply](QNetworkReply::NetworkError code) {
        qWarning() << "Network error occurred:" << code << "-" << reply->errorString();
    });
}

// Add new method to identify error types
Transmitter::ApiErrorType Transmitter::identifyApiError(QNetworkReply* reply, const QByteArray& responseData) {
    // Check for "No killlog panel exists for this server"
    if (responseData.contains("No killlog panel exists for this server")) {
        return ApiErrorType::PanelClosed;
    }
    
    // Check for authentication errors
    if (reply->error() == QNetworkReply::AuthenticationRequiredError ||
        responseData.contains("authentication") ||
        reply->errorString().contains("Host requires authentication")) {
        return ApiErrorType::InvalidApiKey;
    }
    
    // Network-related errors
    if (reply->error() == QNetworkReply::ConnectionRefusedError ||
        reply->error() == QNetworkReply::HostNotFoundError ||
        reply->error() == QNetworkReply::TimeoutError ||
        reply->error() == QNetworkReply::OperationCanceledError) {
        return ApiErrorType::NetworkError;
    }
    
    return ApiErrorType::Other;
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

            // Skip status events - they're for local UI only
            if (event["identifier"].toString() == "status") {
                qDebug() << "Skipping status event (local only):" << log;
                emit logProcessed(log); // Still notify UI it's been processed
                continue;
            }

            // Skip player_character_info events - they're redundant as player info is added to other events
            if (event["identifier"].toString() == "player_character_info") {
                qDebug() << "Skipping player_character_info event (redundant):" << log;
                emit logProcessed(log); // Still notify UI it's been processed
                continue;
            }

            // Add game mode information to specified event types
            QString identifier = event["identifier"].toString();
            if (identifier == "kill_log" || 
                identifier == "vehicle_soft_death" || 
                identifier == "vehicle_destruction" || 
                identifier == "vehicle_instant_destruction") {
                
                // Add current game mode information
                event["game_mode"] = currentGameMode;
                event["sub_game_mode"] = currentSubGameMode;
                
                qDebug() << "Added game mode data to" << identifier << "event:" 
                         << currentGameMode
                         << "/" << currentSubGameMode;
            }

            // Add player information to all event types except status events
            if (identifier != "status" && identifier != "debug_ping") {
                // Add player info if available
                if (!currentPlayerName.isEmpty() && currentPlayerName != "Unknown") {
                    event["player_name"] = currentPlayerName;
                }
                
                if (!currentPlayerGEID.isEmpty()) {
                    event["player_geid"] = currentPlayerGEID;
                }
                
                qDebug() << "Adding player info to event:" << currentPlayerName
                         << "(" << currentPlayerGEID << ")";
            }

            qDebug() << "Processing event identifier:" << identifier;
            
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
    {
        QMutexLocker locker(&lastEventMutex);
        lastEventSentTime = QDateTime::currentDateTimeUtc();
    }

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

// Add after the identifyApiError method

void Transmitter::addSettingsChange(const QString& type, const QString& value) {
    QMutexLocker locker(&settingsChangesMutex);
    
    SettingsChange change;
    change.type = type;
    change.value = value;
    change.timestamp = QDateTime::currentDateTime();
    
    settingsChanges.append(change);
    qDebug() << "Added settings change:" << type << "=" << value;
}

void Transmitter::clearSettingsChanges() {
    QMutexLocker locker(&settingsChangesMutex);
    settingsChanges.clear();
    qDebug() << "Cleared settings changes queue";
}

void Transmitter::updateGameMode(const QString& gameMode, const QString& subGameMode, bool isMonitoring) {
    // Debug the inputs
    qDebug() << "Transmitter::updateGameMode called with gameMode:" << gameMode 
             << "subGameMode:" << subGameMode;
    
    // Store the current values
    currentGameMode = gameMode;
    currentSubGameMode = subGameMode;
    
    // Debug the stored values
    qDebug() << "Transmitter - Current game mode now:" << currentGameMode 
             << "Current sub-game mode now:" << currentSubGameMode;
    
    // Clear the queue
    QMutexLocker locker(&queueMutex);
    logQueue.clear();
    
    // Emit a monitoring stopped signal
    emit monitoringStopped("Game mode changed, resetting monitoring.");
    
    // Emit the signal for local UI updates only
    emit gameModeChanged(gameMode, subGameMode);
}

bool Transmitter::sendGameMode(const QString& apiKey) {
    QJsonObject payload;
    payload["identifier"] = "game_mode_update";
    payload["api_key"] = apiKey;
    payload["game_mode"] = currentGameMode;
    payload["sub_game_mode"] = currentSubGameMode;
    payload["timestamp"] = QDateTime::currentDateTime().toSecsSinceEpoch();
    
    QNetworkRequest request{QUrl(getApiServerUrl())};
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("X-API-Key", apiKey.toUtf8());

    QNetworkReply* reply = networkManager->post(request, QJsonDocument(payload).toJson());
    QEventLoop loop;
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    if (reply->error() == QNetworkReply::NoError) {
        qDebug() << "Game mode update sent:" << currentGameMode
                 << currentSubGameMode;
        reply->deleteLater();
        return true;
    } else {
        QByteArray responseData = reply->readAll();
        qWarning() << "Failed to send game mode update:" << reply->errorString();
        qWarning() << "Server response:" << (responseData.isEmpty() ? "[Empty]" : responseData);
        
        // Identify and emit specific error (reusing existing error handling)
        ApiErrorType errorType = identifyApiError(reply, responseData);
        
        QString errorMessage = tr("API Error: %1").arg(reply->errorString());
        
        switch (errorType) {
            case ApiErrorType::PanelClosed:
                emit panelClosedError();
                emit apiError(ApiErrorType::PanelClosed, tr("The Gunhead panel for this server has been closed or doesn't exist."));
                break;
            case ApiErrorType::InvalidApiKey:
                emit invalidApiKeyError();
                emit apiError(ApiErrorType::InvalidApiKey, tr("Invalid API key. Please check your API key and try again."));
                break;
            default:
                emit apiError(ApiErrorType::Other, errorMessage);
                break;
        }
        
        reply->deleteLater();
        return false;
    }
}

void Transmitter::handleNetworkError(QNetworkReply::NetworkError error, const QString& errorMessage) {
    qDebug() << "Network error:" << error << errorMessage;
    
    // Get HTTP status code from reply if available
    int statusCode = 0;
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (reply) {
        statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    }
    
    // Check for specific error types
    if (error == QNetworkReply::InternalServerError || 
        errorMessage.contains("Internal server error") || 
        statusCode == 500) {
        emit apiError(ApiErrorType::ServerError, tr("Internal server error"));
    } else if (error == QNetworkReply::AuthenticationRequiredError) {
        emit apiError(ApiErrorType::InvalidApiKey, tr("Invalid API key"));
    } else if (errorMessage.contains("Panel is closed")) {
        emit apiError(ApiErrorType::PanelClosed, tr("Panel is closed. Please login to the website first."));
    } else {
        emit apiError(ApiErrorType::NetworkError, errorMessage);
    }
}

// Add after handleNetworkError method

void Transmitter::updatePlayerInfo(const QString& playerName, const QString& playerGEID) {
    // Debug the inputs
    qDebug() << "Transmitter::updatePlayerInfo called with playerName:" << playerName
             << "playerGEID:" << playerGEID;
    
    // Store the current values
    currentPlayerName = playerName;
    currentPlayerGEID = playerGEID;
    
    // Debug the stored values
    qDebug() << "Transmitter - Current player name now:" << currentPlayerName
             << "Current player GEID now:" << currentPlayerGEID;
    
    // Emit the signal
    emit playerInfoChanged(playerName, playerGEID);
}

// In the getter methods
QString Transmitter::getCurrentPlayerName() const {
    qDebug() << "Transmitter::getCurrentPlayerName returning:" << currentPlayerName;
    return currentPlayerName;
}

QString Transmitter::getCurrentPlayerGEID() const {
    return currentPlayerGEID;
}

QString Transmitter::getCurrentGameMode() const {
    qDebug() << "Transmitter::getCurrentGameMode returning:" << currentGameMode;
    return currentGameMode;
}

QString Transmitter::getCurrentSubGameMode() const {
    qDebug() << "Transmitter::getCurrentSubGameMode returning:" << currentSubGameMode;
    return currentSubGameMode;
}