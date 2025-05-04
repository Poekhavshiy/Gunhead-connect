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

    void setupUI();
    void applyColors();
    void filterAndDisplayLogs();
    QStringList filterLogs(const QStringList& logs) const;
    QString prettifyLog(const QString& log) const;
    QString formatEvent(const QString& identifier, const nlohmann::json& parsed) const;
    // Load settings from QSettings
    void loadFilterSettings();
    void updateStatusLabel(const QString& message);

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
};