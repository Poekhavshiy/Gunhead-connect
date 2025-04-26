/* CheckVersion.h */
#ifndef CHECKVERSION_H
#define CHECKVERSION_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QUrl>

class CheckVersion : public QObject {
    Q_OBJECT
public:
    enum class UpdateTriState {
        Yes,
        No,
        Error
    };
    explicit CheckVersion(QObject *parent = nullptr);

    // Generic fetch with timeout
    bool fetchData(const QUrl &url, QByteArray &outData, int timeoutMs);

    // App version checks
    QString getLatestAppVersion(int timeoutMs);
    CheckVersion::UpdateTriState isAppUpdateAvailable(const QString &currentVersion, int timeoutMs);
    bool downloadFile(const QUrl &url, const QString &destination, int timeoutMs);

    // JSON data checks
    QString getLatestJsonVersion(int timeoutMs);
    CheckVersion::UpdateTriState isJsonUpdateAvailable(const QString &localFilePath, int timeoutMs);

signals:
    void errorOccurred(const QString &error);
    void updateAvailable(const QString &version);

private:
    QNetworkAccessManager *networkManager;

    struct Version { int major = 0, minor = 0, patch = 0; };

    Version parseVersion(const QString &versionString) const;
    int compareVersions(const Version &v1, const Version &v2) const;
    QString readLocalJsonVersion(const QString &filePath);
};

#endif // CHECKVERSION_H