/* CheckVersion.cpp */
#include "CheckVersion.h"
#include <QEventLoop>
#include <QNetworkReply>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QMessageBox>
#include "logger.h"

CheckVersion::CheckVersion(QObject *parent)
    : QObject(parent), networkManager(new QNetworkAccessManager(this))
{
    log_info("CheckVersion", "CheckVersion constructor called.");
}

bool CheckVersion::fetchData(const QUrl &url, QByteArray &outData, int timeoutMs)
{
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QNetworkReply *reply = networkManager->get(request);
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);

    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);

    timer.start(timeoutMs);
    loop.exec();

    bool success = false;
    if (timer.isActive()) {
        timer.stop();
        if (reply->error() == QNetworkReply::NoError) {
            outData = reply->readAll();
            log_debug("CheckVersion", "Raw data fetched from URL: ", url.toString().toStdString(), " Data: ", outData.toStdString());
            success = true;
        } else {
            emit errorOccurred(reply->errorString());
            log_error("CheckVersion", "Network error: ", reply->errorString().toStdString());
        }
    } else {
        emit errorOccurred(tr("Request timed out"));
        log_error("CheckVersion", "Request timed out for URL: ", url.toString().toStdString());
        reply->abort();
    }
    reply->deleteLater();
    return success;
}

QString CheckVersion::getLatestAppVersion(int timeoutMs)
{
    
    QString releaseUrlString = "https://api.github.com/repos/Poekhavshiy/Gunhead-Connect/releases/latest";
    log_debug("CheckVersion", "Release URL: ", releaseUrlString.toStdString());
    
    QUrl releaseUrl(releaseUrlString);
    log_debug("CheckVersion", "Fetching latest app version from URL: ", releaseUrl.toString().toStdString());
    QByteArray data;
    if (!fetchData(releaseUrl, data, timeoutMs)) {
        log_error("CheckVersion", "Failed to fetch latest app version data.");
        emit errorOccurred(tr("Failed to fetch latest app version data."));
        return {};
    }

    try {
        const auto doc = QJsonDocument::fromJson(data);
        if (doc.isNull()) {
            emit errorOccurred(tr("Failed to parse JSON: Document is null"));
            log_error("CheckVersion", "Failed to parse JSON: Document is null");
            return {};
        }

        if (!doc.isObject()) {
            emit errorOccurred(tr("Invalid JSON in latest app response"));
            log_error("CheckVersion", "Invalid JSON in latest app response");
            return {};
        }

        const QString tag = doc.object().value("tag_name").toString();
        if (tag.isEmpty()) {
            emit errorOccurred(tr("No tag name found in latest app response"));
            log_error("CheckVersion", "No tag name found in latest app response");
            return {};
        }

        log_info("CheckVersion", "Latest app version fetched successfully: " + tag.toStdString());
        return tag.startsWith("v") ? tag.mid(1) : tag;

    } catch (const std::exception& e) {
        emit errorOccurred(tr("Exception occurred while parsing JSON: ") + QString::fromStdString(e.what()));
        log_error("CheckVersion", "Exception occurred while parsing JSON: ", e.what());
        return {};
    } catch (...) {
        emit errorOccurred(tr("Unknown exception occurred while parsing JSON"));
        log_error("CheckVersion", "Unknown exception occurred while parsing JSON");
        return {};
    }
}

CheckVersion::UpdateTriState CheckVersion::isAppUpdateAvailable(const QString &currentVersion, int timeoutMs)
{
    log_debug("CheckVersion", "Current app version: ", currentVersion.toStdString());
    log_debug("CheckVersion", "Checking for app update availability...");
    QString latest = getLatestAppVersion(timeoutMs);
    if (latest.isEmpty()){
        log_error("CheckVersion", "The version check process failed.");
        emit errorOccurred(tr("The version check process failed."));
        return UpdateTriState::Error;
    }

    Version vLatest = parseVersion(latest);
    Version vCurrent = parseVersion(currentVersion);
    if (compareVersions(vLatest, vCurrent) > 0) {
        emit updateAvailable(latest);
        log_info("CheckVersion", "Update available: ", latest.toStdString());
        return UpdateTriState::Yes;
    }
    return UpdateTriState::No;
}

bool CheckVersion::downloadFile(const QUrl &url, const QString &destination, int inactivityTimeoutMs)
{
    log_debug("CheckVersion", "Downloading file from URL: ", url.toString().toStdString(), " to: ", destination.toStdString());

    QNetworkRequest request(url);
    QNetworkReply *reply = networkManager->get(request);

    QFile file(destination);
    if (!file.open(QIODevice::WriteOnly)) {
        log_error("CheckVersion", "Cannot open file for writing: ", destination.toStdString());
        emit errorOccurred(tr("Cannot open file for writing: ") + destination);
        reply->abort();
        reply->deleteLater();
        return false;
    }

    QEventLoop loop;
    QTimer inactivityTimer;
    inactivityTimer.setSingleShot(true);

    bool success = false;
    qint64 totalBytesWritten = 0;

    connect(reply, &QNetworkReply::readyRead, [&]() {
        QByteArray chunk = reply->readAll();
        qint64 bytesWritten = file.write(chunk);
        totalBytesWritten += bytesWritten;
        if (bytesWritten < chunk.size()) {
            log_error("CheckVersion", "Failed to write full chunk to file");
            emit errorOccurred(tr("Failed to write all data to disk."));
            reply->abort();
            loop.quit();
            return;
        }
        inactivityTimer.start(inactivityTimeoutMs);
    });

    connect(reply, &QNetworkReply::finished, [&]() {
        inactivityTimer.stop();
        if (reply->error() == QNetworkReply::NoError) {
            success = true;
            log_debug("CheckVersion", "Download finished successfully, total bytes: ", std::to_string(totalBytesWritten));
        } else {
            log_error("CheckVersion", "Download error: ", reply->errorString().toStdString());
            emit errorOccurred(tr("Network error: ") + reply->errorString());
        }
        loop.quit();
    });

    connect(reply, &QNetworkReply::downloadProgress, [&](qint64 received, qint64 total) {
        log_debug("CheckVersion", "Download progress: ", std::to_string(received), "/", std::to_string(total));
    });

    connect(&inactivityTimer, &QTimer::timeout, [&]() {
        log_error("CheckVersion", "Download timed out due to inactivity for URL: ", url.toString().toStdString());
        emit errorOccurred(tr("Download timed out due to inactivity."));
        reply->abort();
        loop.quit();
    });

    inactivityTimer.start(inactivityTimeoutMs);
    loop.exec();

    file.close();
    reply->deleteLater();
    return success;
}


QString CheckVersion::getLatestJsonVersion(int timeoutMs)
{
    const QUrl jsonUrl("https://gunhead.sparked.network/static/data/logfile_regex_rules.json");
    QByteArray data;
    if (!fetchData(jsonUrl, data, timeoutMs)) {
        emit errorOccurred(tr("Failed to fetch latest rules."));
        log_error("CheckVersion", "Failed to fetch latest rules.");
        return {};
    }

    try {
        log_debug("CheckVersion", "Raw JSON data fetched: ", data.toStdString());

        const auto doc = QJsonDocument::fromJson(data);
        if (doc.isNull()) {
            emit errorOccurred(tr("Failed to parse rules: Document is null"));
            log_error("CheckVersion", "Failed to parse rules: Document is null");
            return {};
        }

        if (!doc.isObject()) {
            emit errorOccurred(tr("Invalid rules in latest config response"));
            log_error("CheckVersion", "Invalid rules in latest config response");
            return {};
        }

        const QJsonObject obj = doc.object();

        if (!obj.contains("version")) {
            emit errorOccurred(tr("Rules file is missing 'version' key"));
            log_error("CheckVersion", "Rules file is missing 'version' key: " + QString(QJsonDocument(obj).toJson(QJsonDocument::Indented)).toStdString());
            return {};
        }

        const QJsonValue versionVal = obj.value("version");
        if (!versionVal.isString()) {
            emit errorOccurred(tr("'version' key is not a string"));
            log_error("CheckVersion", "'version' key is not a string: " + QString(QJsonDocument(obj).toJson(QJsonDocument::Indented)).toStdString());
            return {};
        } else if (versionVal.toString().isEmpty()) {
            emit errorOccurred(tr("'version' key is empty"));
            log_error("CheckVersion", "'version' key is empty: " + QString(QJsonDocument(obj).toJson(QJsonDocument::Indented)).toStdString());
            return {};
        }
            QString version = versionVal.toString();

            log_info("CheckVersion", "Latest rules version fetched successfully: " + version.toStdString());
            return version;

    } catch (const std::exception& e) {
        emit errorOccurred(tr("Exception occurred while parsing rules: ") + QString::fromStdString(e.what()));
        log_error("CheckVersion", "Exception occurred while parsing rules: ", e.what());
        return {};
    } catch (...) {
        emit errorOccurred(tr("Unknown exception occurred while parsing rules"));
        log_error("CheckVersion", "Unknown exception occurred while parsing rules");
        return {};
    }
}

CheckVersion::UpdateTriState CheckVersion::isJsonUpdateAvailable(const QString &localFilePath, int timeoutMs)
{
    QString remoteVer = getLatestJsonVersion(timeoutMs);
    log_debug("CheckVersion", "Remote rules version: ", remoteVer.toStdString());
    if (remoteVer.isEmpty()) {
        log_error("CheckVersion", "Failed to fetch latest rules version.");
        emit errorOccurred(tr("Failed to fetch latest rules version."));
        return UpdateTriState::Error;
    }

    QString localVer = readLocalJsonVersion(localFilePath);
    Version vRemote = parseVersion(remoteVer);
    Version vLocal = parseVersion(localVer);

    if (compareVersions(vRemote, vLocal) > 0) {
        emit updateAvailable(remoteVer);
        log_info("CheckVersion", "Rules update available: " + remoteVer.toStdString());
        return UpdateTriState::Yes;
    }
    log_info("CheckVersion", "No rules update available.");
    return UpdateTriState::No;
}

CheckVersion::Version CheckVersion::parseVersion(const QString &versionString) const
{
    Version v{0,0,0};
    const auto parts = versionString.split('.');
    if (parts.size() >= 1) v.major = parts[0].toInt();
    if (parts.size() >= 2) v.minor = parts[1].toInt();
    if (parts.size() >= 3) v.patch = parts[2].toInt();
    return v;
}

int CheckVersion::compareVersions(const Version &v1, const Version &v2) const
{
    if (v1.major != v2.major) return v1.major - v2.major;
    if (v1.minor != v2.minor) return v1.minor - v2.minor;
    return v1.patch - v2.patch;
}

QString CheckVersion::readLocalJsonVersion(const QString &filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        emit errorOccurred(tr("Cannot open local JSON: ") + filePath);
        log_error("CheckVersion", "Cannot open local JSON: ", filePath.toStdString());
        return {};
    }

    const QByteArray data = file.readAll();
    file.close();

    try {
        log_debug("CheckVersion", "Raw local rules data: ", data.toStdString());

        const auto doc = QJsonDocument::fromJson(data);
        if (doc.isNull()) {
            emit errorOccurred(tr("Failed to parse local rules: Document is null"));
            log_error("CheckVersion", "Failed to parse local rules: Document is null");
            return {};
        }

        if (!doc.isObject()) {
            emit errorOccurred(tr("Invalid local rules format"));
            log_error("CheckVersion", "Invalid local rules format");
            return {};
        }

        QString version = doc.object().value("version").toString();
        if (version.isEmpty()) {
            emit errorOccurred(tr("No version found in local rules"));
            log_error("CheckVersion", "No version found in local rules");
            return {};
        }

        log_info("CheckVersion", "Local rules version read successfully: " + version.toStdString());
        return version;

    } catch (const std::exception& e) {
        emit errorOccurred(tr("Exception occurred while parsing local rules: ") + QString::fromStdString(e.what()));
        log_error("CheckVersion", "Exception occurred while parsing local rules: ", e.what());
        return {};
    } catch (...) {
        emit errorOccurred(tr("Unknown exception occurred while parsing local rules"));
        log_error("CheckVersion", "Unknown exception occurred while parsing local rules");
        return {};
    }
}
