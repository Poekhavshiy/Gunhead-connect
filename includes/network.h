#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QUrl>

class Network : public QObject {
    Q_OBJECT

public:
    explicit Network(QObject *parent = nullptr);
    void postJson(const QUrl &endpoint, const QByteArray &jsonData);

signals:
    void requestFinished(bool success, const QString &response);

private slots:
    void onReplyFinished(QNetworkReply *reply);

private:
    QNetworkAccessManager m_manager;
};
