#include "network.h"
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QDebug>

Network::Network(QObject *parent) : QObject(parent) {
    qDebug() << "Network object created";
    connect(&m_manager, &QNetworkAccessManager::finished, this, &Network::onReplyFinished);
}

void Network::postJson(const QUrl &endpoint, const QByteArray &jsonData) {
    QNetworkRequest request(endpoint);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    qDebug() << "Sending POST request to" << endpoint;
    m_manager.post(request, jsonData);
}

void Network::onReplyFinished(QNetworkReply *reply) {
    bool success = (reply->error() == QNetworkReply::NoError);
    QString response = reply->readAll();
    qDebug() << "Reply received:" << response;
    emit requestFinished(success, response);
    reply->deleteLater();
}
