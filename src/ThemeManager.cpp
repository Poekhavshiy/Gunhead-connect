#include "ThemeManager.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QFontDatabase>
#include <QMainWindow>
#include <QPushButton>
#include <QTabWidget>
#include <QTabBar>
#include <QTimer>
#include <QDebug>
#include <QMenu>

ThemeManager& ThemeManager::instance() {
    static ThemeManager instance;
    return instance;
}

ThemeManager::ThemeManager(QObject* parent) 
    : QObject(parent) 
{
    qDebug() << "ThemeManager singleton initialized";
}

Theme ThemeManager::loadCurrentTheme() {
    QSettings settings("KillApiConnect", "KillApiConnectPlus");
    Theme theme;
    QString themePath = settings.value("currentTheme", ":/themes/originalsleek.qss").toString();
    theme.stylePath = themePath;
    theme.fontPath = settings.value("currentFont", ":/fonts/PressStart2P-Regular.ttf").toString();
    theme.backgroundImage = settings.value("currentBackgroundImage", "").toString();
    
    // Initialize with fallback values
    theme.mainWindowPreferredSize = QSize(800, 600);
    theme.chooseThemeWindowPreferredSize = QSize(400, 400);
    theme.chooseLanguageWindowPreferredSize = QSize(300, 300);
    theme.settingsWindowPreferredSize = QSize(500, 400);
    theme.statusLabelPreferredSize = settings.value("statusLabelPreferredSize", QSize(280, 80)).toSize();
    theme.mainButtonRightSpace = settings.value("mainButtonRightSpace", 140).toInt();
    
    // Find matching theme in themes.json for window sizes
    QVector<Theme> themes = loadThemes();
    for (const Theme& t : themes) {
        if (t.stylePath == themePath) {
            theme.mainWindowPreferredSize = t.mainWindowPreferredSize;
            theme.chooseThemeWindowPreferredSize = t.chooseThemeWindowPreferredSize;
            theme.chooseLanguageWindowPreferredSize = t.chooseLanguageWindowPreferredSize;
            theme.settingsWindowPreferredSize = t.settingsWindowPreferredSize;
            theme.statusLabelPreferredSize = t.statusLabelPreferredSize;
            theme.mainButtonRightSpace = t.mainButtonRightSpace;
            theme.friendlyName = t.friendlyName;
            
            qDebug() << "Loaded theme properties for:" << t.friendlyName;
            break;
        }
    }
    
    return theme;
}

QVector<Theme> ThemeManager::loadThemes() {
    QVector<Theme> themes;
    QFile file(":/themes/themes.json");
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Unable to open themes.json";
        return themes;
    }

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isArray()) {
        qWarning() << "Invalid JSON format for themes.";
        return themes;
    }

    for (const QJsonValue& val : doc.array()) {
        if (!val.isObject()) continue;
        QJsonObject obj = val.toObject();
        Theme t;
        t.friendlyName = obj.value("name").toString();
        t.stylePath = obj.value("qss").toString();
        t.fontPath = obj.value("font").toString();
        t.backgroundImage = obj.value("background").toString();

        auto extractSize = [](const QJsonObject& o) -> QSize {
            return QSize(o.value("width").toInt(), o.value("height").toInt());
        };

        if (obj.contains("mainWindowPreferredSize")) t.mainWindowPreferredSize = extractSize(obj["mainWindowPreferredSize"].toObject());
        if (obj.contains("chooseThemeWindowPreferredSize")) t.chooseThemeWindowPreferredSize = extractSize(obj["chooseThemeWindowPreferredSize"].toObject());
        if (obj.contains("chooseLanguageWindowPreferredSize")) t.chooseLanguageWindowPreferredSize = extractSize(obj["chooseLanguageWindowPreferredSize"].toObject());
        if (obj.contains("settingsWindowPreferredSize")) t.settingsWindowPreferredSize = extractSize(obj["settingsWindowPreferredSize"].toObject());
        if (obj.contains("statusLabelPreferredSize")) t.statusLabelPreferredSize = extractSize(obj["statusLabelPreferredSize"].toObject());
        t.mainButtonRightSpace = obj.value("mainButtonRightSpace").toInt(140); // Default 140px

        themes.append(t);
    }

    return themes;
}

void ThemeManager::applyTheme(const Theme& theme) {
    // Apply stylesheet to all top-level windows
    applyStylesheet(theme.stylePath);
    
    // Apply font to all top-level windows
    applyFont(theme.fontPath);
    
    // Force relayout of various UI components
    forceRelayout();
    
    // Emit signals for listeners
    emit themeChanged(theme);
    emit themeApplied();
}

void ThemeManager::saveTheme(const Theme& theme) {
    QSettings settings("KillApiConnect", "KillApiConnectPlus");
    settings.setValue("currentTheme", theme.stylePath);
    settings.setValue("currentFont", theme.fontPath);
    settings.setValue("currentBackgroundImage", theme.backgroundImage);
    settings.setValue("statusLabelPreferredSize", theme.statusLabelPreferredSize);
    settings.setValue("mainButtonRightSpace", theme.mainButtonRightSpace);

    qDebug() << "Saved theme:" << theme.stylePath << "with font:" << theme.fontPath;
}

void ThemeManager::applyCurrentThemeToAllWindows() {
    Theme currentTheme = loadCurrentTheme();
    applyTheme(currentTheme);
}

void ThemeManager::applyStylesheet(const QString& stylePath) {
    QFile f(stylePath);
    if (f.open(QFile::ReadOnly | QFile::Text)) {
        QString style = f.readAll();
        
        // Apply to all top-level windows with known object names
        for (QWidget* widget : QApplication::topLevelWidgets()) {
            if (widget->objectName() == "MainWindow" || 
                widget->objectName() == "LogDisplayWindow" ||
                widget->objectName() == "SettingsWindow" ||
                widget->objectName() == "languageSelectWindow" ||
                widget->objectName() == "themeSelectWindow") {
                
                widget->setStyleSheet(style);
                
                // Reset any system menus we find directly, without using MainWindow
                // This is more generic and avoids the circular dependency
                QList<QMenu*> menus = widget->findChildren<QMenu*>();
                for (QMenu* menu : menus) {
                    if (menu->objectName() == "trayIconMenu" || 
                        menu->property("systemMenu").toBool()) {
                        menu->setStyleSheet("");
                        menu->setAttribute(Qt::WA_StyledBackground, false);
                        menu->setStyle(QApplication::style());
                    }
                }
                
                qDebug() << "Applied theme to window:" << widget->objectName();
            }
        }
        
        qDebug() << "Applied QSS to themed windows:" << stylePath;
    } else {
        qWarning() << "Cannot load QSS:" << stylePath;
    }
}

void ThemeManager::applyFont(const QString& fontPath) {
    int id = QFontDatabase::addApplicationFont(fontPath);
    if (id != -1) {
        const auto fam = QFontDatabase::applicationFontFamilies(id);
        if (!fam.isEmpty()) {
            // Create font object with the theme font family
            QFont themeFont(fam.first());

            // Apply font to specific widgets
            for (QWidget* widget : QApplication::topLevelWidgets()) {
                if (widget->objectName() == "MainWindow" || 
                    widget->objectName() == "LogDisplayWindow" ||
                    widget->objectName() == "SettingsWindow" ||
                    widget->objectName() == "languageSelectWindow" ||
                    widget->objectName() == "themeSelectWindow") {

                    // Find the central widget to apply font
                    QWidget* central = qobject_cast<QMainWindow*>(widget)->centralWidget();
                    if (central) {
                        central->setFont(themeFont);
                    }
                }
            }

            qDebug() << "Applied font:" << fam.first();
        }
    } else {
        qWarning() << "Cannot load font:" << fontPath;
    }
}

void ThemeManager::forceRelayout() {
    // Force relayout of QTabWidget and QTabBar in SettingsWindow
    for (QWidget* widget : QApplication::topLevelWidgets()) {
        if (widget->objectName() == "SettingsWindow") {
            QTabWidget* tabWidget = widget->findChild<QTabWidget*>();
            if (tabWidget) {
                // First pass layout update
                tabWidget->updateGeometry();
                tabWidget->adjustSize();
                
                QTabBar* tabBar = tabWidget->tabBar();
                if (tabBar) {
                    // Set tab to expand and fill available space
                    tabBar->setExpanding(true);
                    
                    // Force all tabs to recalculate their sizes based on content
                    for (int i = 0; i < tabBar->count(); i++) {
                        QString tabText = tabWidget->tabText(i);
                        // Calculate width based on text and font metrics
                        QFontMetrics fm(tabBar->font());
                        int textWidth = fm.horizontalAdvance(tabText);
                        
                        // Add padding for the tab
                        int minTabWidth = textWidth + 60; // Add padding
                        
                        // Set minimum size hint for this tab
                        tabBar->setTabData(i, QVariant(minTabWidth));
                    }
                    
                    // Update layout and visuals
                    tabBar->updateGeometry();
                    tabBar->adjustSize();
                    
                    // Schedule a second update after delay to ensure fonts are loaded
                    QTimer::singleShot(100, [tabBar, tabWidget]() {
                        tabBar->updateGeometry();
                        tabBar->adjustSize();
                        tabBar->repaint();
                        tabWidget->updateGeometry();
                        tabWidget->adjustSize();
                        tabWidget->repaint();
                    });
                }
                
                // Final immediate update
                tabWidget->repaint();
            }
        }
        
        // Handle LogDisplayWindow filter dropdown button
        if (widget->objectName() == "LogDisplayWindow") {
            QPushButton* filterBtn = widget->findChild<QPushButton*>("filterDropdown");
            if (filterBtn) {
                filterBtn->updateGeometry();
                filterBtn->adjustSize();
                
                // Wait briefly for font to fully load before final adjustment
                QTimer::singleShot(100, [filterBtn]() {
                    filterBtn->updateGeometry();
                    filterBtn->adjustSize();
                    filterBtn->repaint();
                });
            }
        }
    }
}