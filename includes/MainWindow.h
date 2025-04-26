#pragma once

#include <QMainWindow>
#include <QPointer>
#include <QPushButton>
#include <QVBoxLayout>
#include <QLabel>
#include <QString>
#include <QVariant>
#include "SettingsWindow.h"
#include "LogDisplayWindow.h"
#include "ThemeSelect.h"
#include "logmonitor.h"
#include "Transmitter.h"

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    bool isApplicationShuttingDown() const; // Getter for isShuttingDown

private:
    QPushButton* startButton;
    QPushButton* settingsButton;
    QPushButton* logButton; // Declare logButton
    QLabel* statusLabel;

    QPointer<QWidget> bgContainer; // Background container
    QPointer<SettingsWindow> settingsWindow; // Settings window
    LogDisplayWindow* logDisplayWindow; // Log display window
    LogMonitor* logMonitor; // LogMonitor instance
    QString gameLogFilePath; // Path to the game.log file

    Transmitter transmitter; // Single instance of Transmitter

    QVector<QHBoxLayout*> buttonLayouts; // Store references to each button row layout
    int currentButtonSpacing; // Add this to track spacing independent of QSettings

    bool logDisplayVisible; // Flag to track log display visibility
    bool isMonitoring; // Flag to track monitoring state

    // Filtering options
    bool showPvP;
    bool showPvE;
    bool showNPCNames;
    
    void closeEvent(QCloseEvent* event) override; // Handle close event
    void focusInEvent(QFocusEvent* event) override; // Handle focus event
    void loadRegexRules();
    void debugPaths(); // Debug paths for settings and files
    void updateStatusLabel(const QString& status);

public:
    // Getter methods for filter settings
    bool getShowPvP() const { return showPvP; }
    bool getShowPvE() const { return showPvE; }
    bool getShowNPCNames() const { return showNPCNames; }
    
    // Setter methods for filter settings with signal
    void setShowPvP(bool show);
    void setShowPvE(bool show);
    void setShowNPCNames(bool show);

signals:
    void filterSettingsChanged(); // Add this signal

private slots:
    void toggleSettingsWindow();
    void toggleLogDisplayWindow(bool forceNotVisible = false);
    void applyTheme(Theme themeData);
    void handleLogLine(const QString& jsonPayload); 
    void startMonitoring();
    void stopMonitoring();
    void handleMonitoringToggleRequest(); // Add this slot
};
