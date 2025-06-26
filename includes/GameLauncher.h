#pragma once

#include <QString>
#include <QProcess>

class GameLauncher {
public:
    static bool isStarCitizenRunning();
    static bool launchStarCitizen(const QString& launcherPath);
    static QString findStarCitizenExecutable(const QString& gameFolder);

private:
    static QStringList getStarCitizenProcessNames();
};
