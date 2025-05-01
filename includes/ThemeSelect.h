#ifndef THEMESELECT_H
#define THEMESELECT_H

#include <QWidget>
#include <QMainWindow>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSettings>
#include <QFile>
#include <QFontDatabase>
#include <QApplication>
#include <QButtonGroup>
#include <QVector>
#include <QString>
#include <QSize>
#include <QStyle>

struct Theme {
    QString stylePath;
    QString friendlyName;
    QString fontPath;
    QString backgroundImage;
    QSize mainWindowPreferredSize;
    QSize chooseThemeWindowPreferredSize;    
    QSize chooseLanguageWindowPreferredSize;
    QSize settingsWindowPreferredSize;
    QSize statusLabelPreferredSize; // New property for status label sizing
    int mainButtonRightSpace; // New property for button right spacing
};

class ThemeSelectWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit ThemeSelectWindow(QWidget* parent = nullptr);
    void init(QApplication& app);  // Call this early in main()
    Theme loadCurrentTheme();  // Returns {qssPath, fontPath}


signals:
    void themeChanged(Theme themeData); // Signal to notify theme change

private:
    QVBoxLayout* mainLayout;
    QLabel* header;
    QPushButton* selectButton;

    QVector<Theme> loadThemes(); // Function to load embedded themes
    QPushButton* m_currentThemeButton = nullptr;

    void connectSignals();
    void applyTheme(const Theme& theme);
    void saveTheme(const Theme& t); // Save the current theme to settings
   
};

#endif // THEMESELECT_H