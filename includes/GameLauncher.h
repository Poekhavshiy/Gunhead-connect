#pragma once

#include <QString>
#include <QProcess>
#include <QFile>
#include <QObject>

class GameLauncher : public QObject {
    Q_OBJECT
public:
    explicit GameLauncher(QObject* parent = nullptr);
    bool isStarCitizenRunning();
    bool launchStarCitizen(const QString& launcherPath);
    QString findStarCitizenExecutable(const QString& gameFolder);
    QStringList getStarCitizenProcessNames();

private:
    QProcess* starCitizenProcess = nullptr; // ADDED: For capturing external process output
    QFile* starCitizenLogFile = nullptr;    // ADDED: For logging external process output

    void handleStarCitizenOutput();         // ADDED: Slot for stdout
    void handleStarCitizenError();          // ADDED: Slot for stderr
    void cleanupStarCitizenProcess();       // ADDED: Cleanup resources
};
