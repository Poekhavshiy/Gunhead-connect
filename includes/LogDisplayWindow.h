#pragma once

#include <QMainWindow>
#include <QTextEdit>
#include <QCheckBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QStringList>
#include <QSettings>
#include "Transmitter.h"
#include "SoundPlayer.h"

#ifdef Q_OS_WIN
#include <windows.h>
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")
#endif

class LogDisplayWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit LogDisplayWindow(Transmitter& transmitter, QWidget* parent = nullptr);

    void addEvent(const QString& eventText);
    void setColors(const QString& bgColor, const QString& fgColor);
    void setFontSize(int fontSize);
    void processLogLine(const QString& logLine);
    void setApplicationShuttingDown(bool shuttingDown);
    void updateMonitoringButtonText(bool isMonitoring);
    void updateGameFolder(const QString& newFolder);
    
    // Add getters for filter settings
    bool getShowPvP() const { return showPvP; }
    bool getShowPvE() const { return showPvE; }
    bool getShowNPCNames() const { return showNPCNames; }

    void resetLogDisplay(); // Public method that will call the private clearLog()
    void setGameMode(const QString& gameMode, const QString& subGameMode = "");

    void updateStatusLabel(const QString& message);

    // New methods to control the monitoring button state
    void disableMonitoringButton(const QString& text);
    void enableMonitoringButton(const QString& text);

signals:
    void windowClosed();
    void toggleMonitoringRequested();
    // Add new signals for filter settings
    void filterPvPChanged(bool show);
    void filterPvEChanged(bool show);
    void filterNPCNamesChanged(bool show);

protected:
    void moveEvent(QMoveEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

private:
    Transmitter& transmitter;
    QTextEdit* logDisplay;
    QCheckBox* showPvPCheckbox;
    QCheckBox* showPvECheckbox;
    QCheckBox* showNPCNamesCheckbox;
    QCheckBox* playSoundCheckbox;
    QPushButton* testButton;
    QPushButton* monitoringButton;
    bool showPvP;
    bool showPvE;
    bool showNPCNames;
    bool playSound;

    QString logBgColor = "#1e1e1e";
    QString logFgColor = "#dcdcdc";
    int logFontSize = 12;
    QStringList eventBuffer;
    bool isShuttingDown = false;
    SoundPlayer* soundPlayer;
    QString currentGameMode = "Unknown";
    QString currentSubGameMode = "";

    // Add member variables for initial game mode and sub game mode
    QString initialGameMode;
    QString initialSubGameMode;

    void setupUI();
    void applyColors();
    void filterAndDisplayLogs();
    QStringList filterLogs(const QStringList& logs) const;
    QString prettifyLog(const QString& log) const;
    QString formatEvent(const QString& identifier, const nlohmann::json& parsed) const;
    // Load settings from QSettings
    void loadFilterSettings();
    void playSystemSound();
    QString getFriendlyGameModeName(const QString& rawMode) const;

private slots:
    void clearLog();
    void changeTextColor();
    void changeBackgroundColor();
    void increaseFontSize();
    void decreaseFontSize();
    void handleTestButton();
    void handleMonitoringButton();
    // Add slots for checkbox changes
    void onShowPvPToggled(bool checked);
    void onShowPvEToggled(bool checked);
    void onShowNPCNamesToggled(bool checked);

public slots:
    void processLogQueue(const QString& log);
    void updateFilterSettings(bool showPvP, bool showPvE, bool showNPCNames);
    void updateWindowTitle(const QString& gameMode, const QString& subGameMode);

};