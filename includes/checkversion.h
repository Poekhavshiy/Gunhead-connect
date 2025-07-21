/* CheckVersion.h */
#ifndef CHECKVERSION_H
#define CHECKVERSION_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QUrl>
#include <QString>

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

    // JSON data checks removed; use isParserUpdateAvailable for parser/rules updates

    // Parser version checks
    QString getLatestParserVersion(int timeoutMs);
    CheckVersion::UpdateTriState isParserUpdateAvailable(const QString &currentVersion, int timeoutMs);

signals:
    void errorOccurred(const QString &error);
    void updateAvailable(const QString &version);

private:
    QNetworkAccessManager *networkManager;

    struct Version { int major = 0, minor = 0, patch = 0; };

    Version parseVersion(const QString &versionString) const;
    int compareVersions(const Version &v1, const Version &v2) const;
    const QString APP_VERSIONS_URL = "https://gunhead.space/dist/latest.json";
    const QString APP_PARSER_URL = "https://gunhead.space/dist/logfile_regex_rules.json";
    const QString APP_INSTALLER_URL = "https://gunhead.space/dist//Gunhead-Connect-Setup.msi";
public:
    // Getter for parser rules URL
    QString getParserRulesUrl() const { return APP_PARSER_URL; }
    QString getAppInstallerUrl() const { return APP_INSTALLER_URL; }
    QString readLocalJsonVersion(const QString &filePath); // Moved to public
};

#endif // CHECKVERSION_H