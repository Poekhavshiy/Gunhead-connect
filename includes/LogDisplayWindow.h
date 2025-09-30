#pragma once

#include <QMainWindow>
#include <QTextBrowser>
#include <QCheckBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QStringList>
#include <QSettings>
#include <QComboBox>
#include <QLabel>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QListWidget>
#include <QListWidgetItem>
#include "Transmitter.h"
#include "SoundPlayer.h"
#include "language_manager.h"
#include "CustomTitleBar.h"

#ifdef Q_OS_WIN
#include <windows.h>
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")
#endif

class FilterDropdownWidget : public QWidget {
    Q_OBJECT

public:
    explicit FilterDropdownWidget(QWidget* parent = nullptr);
    void setShowPvP(bool show);
    void setShowPvE(bool show);
    void setShowShips(bool show);
    void setShowOther(bool show);
    void setShowNPCNames(bool show);
    
    bool getShowPvP() const;
    bool getShowPvE() const;
    bool getShowShips() const;
    bool getShowOther() const;
    bool getShowNPCNames() const;

signals:
    void filterChanged();

private slots:
    void onSelectAllChanged(bool checked);
    void onIndividualFilterChanged();

private:
    QCheckBox* selectAllCheckbox;
    QCheckBox* pvpCheckbox;
    QCheckBox* pveCheckbox;
    QCheckBox* shipsCheckbox;
    QCheckBox* otherCheckbox;
    QCheckBox* npcNamesCheckbox;
    bool updatingSelectAll;
};

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
    
    // New getters for the extended filter system
    bool getShowShips() const { return showShips; }
    bool getShowOther() const { return showOther; }

    void resetLogDisplay(); // Public method that will call the private clearLog()
    void setGameMode(const QString& gameMode, const QString& subGameMode = "");

    void updateStatusLabel(const QString& message);

    // New methods to control the monitoring button state
    void disableMonitoringButton(const QString& text);
    void enableMonitoringButton(const QString& text);
    void updateFilterDropdownWidth();
    void setDebugModeEnabled(bool enabled);
    void recalculateLayout();

signals:
    void windowClosed();
    void toggleMonitoringRequested();
    // Filter signals
    void filterPvPChanged(bool show);
    void filterPvEChanged(bool show);
    void filterShipsChanged(bool show);
    void filterOtherChanged(bool show);
    void filterNPCNamesChanged(bool show);

protected:
    void moveEvent(QMoveEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void closeEvent(QCloseEvent* event) override;
    void showEvent(QShowEvent* event) override;

private:
    Transmitter& transmitter;
    CustomTitleBar* titleBar; // Custom title bar widget
    class ResizeHelper* resizeHelper; // Window resize helper
    QTextBrowser* logDisplay; // Ensure this is QTextBrowser*, not QTextEdit
    
    // New filter system UI
    QPushButton* filterDropdown;
    FilterDropdownWidget* filterWidget;
    QListWidget* filterListWidget;
    QCheckBox* playSoundCheckbox;
    QPushButton* testButton;
    QPushButton* monitoringButton;
    
    // Filter state variables
    bool showPvP;
    bool showPvE;
    bool showShips;
    bool showOther;
    bool showNPCNames;
    bool playSound;
    
    // JSON rules cache
    QJsonObject rulesData;

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
    QString formatEventFromTemplate(const QString& messageTemplate, const nlohmann::json& parsed) const;
    // Load settings from QSettings
    void loadFilterSettings();
    void loadJsonRules();
    void playSystemSound();
    QString getFriendlyGameModeName(const QString& rawMode) const;
    bool shouldShowEventType(const QString& filterType) const;

private slots:
    void clearLog();
    void changeTextColor();
    void changeBackgroundColor();
    void increaseFontSize();
    void decreaseFontSize();
    void handleTestButton();
    void handleMonitoringButton();
    // Updated slots for new filter system
    void onShowPvPToggled(bool checked);
    void onShowPvEToggled(bool checked);
    void onShowShipsToggled(bool checked);
    void onShowOtherToggled(bool checked);
    void onShowNPCNamesToggled(bool checked);
    void onFilterDropdownChanged(int index);

    void retranslateUi();

public slots:
    void processLogQueue(const QString& log);
    void updateFilterSettings(bool showPvP, bool showPvE, bool showNPCNames);
    void updateWindowTitle(const QString& gameMode, const QString& subGameMode);
    void onThemeChanged();
};