#ifndef THEMEMANAGER_H
#define THEMEMANAGER_H

#include <QObject>
#include <QVector>
#include <QString>
#include <QSize>
#include <QApplication>
#include <QSettings>

// Use the existing Theme struct
struct Theme {
    QString stylePath;
    QString friendlyName;
    QString fontPath;
    QString backgroundImage;
    QSize mainWindowPreferredSize;
    QSize chooseThemeWindowPreferredSize;    
    QSize chooseLanguageWindowPreferredSize;
    QSize settingsWindowPreferredSize;
    QSize statusLabelPreferredSize;
    int mainButtonRightSpace;
};

class ThemeManager : public QObject {
    Q_OBJECT

public:
    static ThemeManager& instance();
    
    // Theme operations
    Theme loadCurrentTheme();
    QVector<Theme> loadThemes();
    void applyTheme(const Theme& theme);
    void saveTheme(const Theme& theme);
    
    // Apply themes to all windows
    void applyCurrentThemeToAllWindows();
    
signals:
    void themeChanged(Theme themeData);
    void themeApplied();

private:
    explicit ThemeManager(QObject* parent = nullptr);
    ~ThemeManager() = default;
    ThemeManager(const ThemeManager&) = delete;
    ThemeManager& operator=(const ThemeManager&) = delete;
    
    // Helper methods
    void applyStylesheet(const QString& stylePath);
    void applyFont(const QString& fontPath);
    void forceRelayout();
};

#endif // THEMEMANAGER_H