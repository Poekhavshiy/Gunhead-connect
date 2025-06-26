#include "GameLauncher.h"
#include <QDebug>
#include <QFileInfo>
#include <QDir>
#include <QStandardPaths>

#ifdef Q_OS_WIN
#include <windows.h>
#include <tlhelp32.h>
#endif

bool GameLauncher::isStarCitizenRunning() {
    QStringList processNames = getStarCitizenProcessNames();
    qDebug() << "GameLauncher: Checking for Star Citizen processes:" << processNames;
    
#ifdef Q_OS_WIN
    HANDLE hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hProcessSnap == INVALID_HANDLE_VALUE) {
        qWarning() << "GameLauncher: Failed to create process snapshot";
        return false;
    }
    
    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);
    
    if (!Process32First(hProcessSnap, &pe32)) {
        qWarning() << "GameLauncher: Failed to get first process";
        CloseHandle(hProcessSnap);
        return false;
    }
    
    bool found = false;
    QStringList runningProcesses;
    do {
        QString processName = QString::fromWCharArray(pe32.szExeFile);
        runningProcesses.append(processName);
        if (processNames.contains(processName, Qt::CaseInsensitive)) {
            qDebug() << "GameLauncher: Found Star Citizen process:" << processName;
            found = true;
            break;
        }
    } while (Process32Next(hProcessSnap, &pe32));
    
    CloseHandle(hProcessSnap);
    
    if (!found) {
        qDebug() << "GameLauncher: No Star Citizen processes found. Checked" << runningProcesses.size() << "processes";
        qDebug() << "GameLauncher: Sample of running processes:" << runningProcesses.mid(0, 10);
    }
    
    return found;
#else
    // For non-Windows platforms, use QProcess to check running processes
    QProcess process;
    process.start("ps", QStringList() << "-A" << "-o" << "comm=");
    process.waitForFinished();
    
    QString output = process.readAllStandardOutput();
    for (const QString& processName : processNames) {
        if (output.contains(processName, Qt::CaseInsensitive)) {
            qDebug() << "Found Star Citizen process:" << processName;
            return true;
        }
    }
    return false;
#endif
}

bool GameLauncher::launchStarCitizen(const QString& launcherPath) {
    qDebug() << "GameLauncher: launchStarCitizen called with launcherPath:" << launcherPath;
    
    if (launcherPath.isEmpty()) {
        qWarning() << "GameLauncher: Launcher path is empty - RSI Launcher path must be configured in Settings";
        return false;
    }
    
    // Check if the launcher file actually exists and is executable
    QFileInfo launcherInfo(launcherPath);
    if (!launcherInfo.exists()) {
        qWarning() << "GameLauncher: Launcher file does not exist:" << launcherPath;
        qWarning() << "GameLauncher: Please verify the RSI Launcher is installed and the path is correct in Settings";
        return false;
    }
    if (!launcherInfo.isFile()) {
        qWarning() << "GameLauncher: Launcher path is not a file:" << launcherPath;
        return false;
    }
    if (!launcherInfo.isExecutable()) {
        qWarning() << "GameLauncher: Launcher file is not executable:" << launcherPath;
        qWarning() << "GameLauncher: Check file permissions for:" << launcherPath;
        return false;
    }
    
    qDebug() << "GameLauncher: Launcher file validation passed, attempting to start process...";
    
    // Try to start the launcher
    bool success = QProcess::startDetached(launcherPath);
    if (success) {
        qDebug() << "GameLauncher: RSI Launcher started successfully from:" << launcherPath;
    } else {
        qWarning() << "GameLauncher: Failed to start RSI Launcher from:" << launcherPath;
        qWarning() << "GameLauncher: Check if the file exists and you have permission to execute it";
    }
    
    return success;
}

QString GameLauncher::findStarCitizenExecutable(const QString& gameFolder) {
    qDebug() << "GameLauncher: Searching for Star Citizen executable in:" << gameFolder;
    
    QStringList possiblePaths = {
        gameFolder + "/Bin64/StarCitizen.exe"  // Main game executable location
    };
    
    qDebug() << "GameLauncher: Will check these paths:" << possiblePaths;
    
    for (const QString& path : possiblePaths) {
        qDebug() << "GameLauncher: Checking path:" << path;
        QFileInfo fileInfo(path);
        if (fileInfo.exists() && fileInfo.isFile()) {
            qDebug() << "GameLauncher: Found Star Citizen executable at:" << path;
            return path;
        } else {
            qDebug() << "GameLauncher: Path not found or not a file:" << path
                     << "exists:" << fileInfo.exists() 
                     << "isFile:" << fileInfo.isFile();
        }
    }
    
    qWarning() << "GameLauncher: Star Citizen executable not found. Searched paths:";
    for (const QString& path : possiblePaths) {
        qWarning() << "GameLauncher:   -" << path;
    }
    
    // List the actual contents of the game folder for debugging
    QDir gameDir(gameFolder);
    if (gameDir.exists()) {
        qDebug() << "GameLauncher: Contents of game folder:" << gameDir.entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
        
        // Also check Bin64 subdirectory
        QDir bin64Dir(gameFolder + "/Bin64");
        if (bin64Dir.exists()) {
            qDebug() << "GameLauncher: Contents of Bin64 folder:" << bin64Dir.entryList(QDir::Files | QDir::NoDotAndDotDot);
        }
    } else {
        qWarning() << "GameLauncher: Game folder does not exist:" << gameFolder;
    }
    
    return QString();
}

QStringList GameLauncher::getStarCitizenProcessNames() {
    return QStringList() 
        << "StarCitizen.exe";  // Only check for the main game executable
}
