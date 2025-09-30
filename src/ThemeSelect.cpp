#include <QSettings>
#include <QFile>
#include <QFontDatabase>
#include <QApplication>
#include <QDebug>
#include <QPushButton>
#include <QVBoxLayout>
#include <QLabel>
#include <QButtonGroup>
#include <QTimer>
#include <QMainWindow>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QTabBar> // Add this line
#include <QShowEvent>
#include <QPointer>
#include "ThemeSelect.h"
#include "ThemeManager.h"

// Modified function to find main window without dependency on MainWindow class
QMainWindow* ThemeSelectWindow::findMainWindow() {
    // Look for MainWindow among all top-level widgets
    for (QWidget* widget : QApplication::topLevelWidgets()) {
        if (widget->objectName() == "MainWindow") {
            return qobject_cast<QMainWindow*>(widget);
        }
    }
    return nullptr;
}

ThemeSelectWindow::ThemeSelectWindow(QWidget* parent)
    : QMainWindow(parent)
{
    // Set frameless window hint for custom title bar
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground, false);
    
    setWindowTitle(tr("Theme Selector"));
    setObjectName("themeSelectWindow");

    // Get themes from ThemeManager
    QVector<Theme> themes = ThemeManager::instance().loadThemes();
    if (themes.isEmpty()) {
        qWarning() << "No themes loaded. Exiting.";
        return;
    }

    // Load current theme
    Theme current = ThemeManager::instance().loadCurrentTheme();
    
    // Apply theme through ThemeManager
    ThemeManager::instance().applyTheme(current);
    
    // Adjust size for custom title bar
    QSize windowSize = current.chooseThemeWindowPreferredSize;
    windowSize.setHeight(windowSize.height() + 32);
    setFixedSize(windowSize);

    // Create custom title bar
    titleBar = new CustomTitleBar(this, false); // No maximize button
    titleBar->setTitle(tr("Theme Selector"));
    titleBar->setIcon(QIcon(":/icons/Gunhead.png"));
    
    // Connect title bar signals
    connect(titleBar, &CustomTitleBar::minimizeClicked, this, &ThemeSelectWindow::showMinimized);
    connect(titleBar, &CustomTitleBar::closeClicked, this, &ThemeSelectWindow::close);
    
    // Create container widget to hold title bar and content
    QWidget* outerContainer = new QWidget(this);
    outerContainer->setObjectName("themeWindowContainer");
    QVBoxLayout* outerLayout = new QVBoxLayout(outerContainer);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);
    outerLayout->addWidget(titleBar);

    QWidget* central = new QWidget(outerContainer);
    central->setObjectName("themeSelectContainer");
    central->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    outerLayout->addWidget(central, 1);
    setCentralWidget(outerContainer);
    
    // Apply window effects (rounded corners and shadow)
    QPointer<ThemeSelectWindow> safeThis(this);
    QTimer::singleShot(100, this, [safeThis]() {
        if (safeThis) {
            CustomTitleBar::applyWindowEffects(safeThis);
        }
    });
    
    mainLayout = new QVBoxLayout(central);
    mainLayout->setContentsMargins(10, 10, 10, 10);

    header = new QLabel(tr("> THEME SELECTOR"), central);
    header->setObjectName("headerLabel");
    header->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(header);

    QButtonGroup* group = new QButtonGroup(this);
    for (const Theme& t : themes) {
        QPushButton* btn = new QPushButton(t.friendlyName, central);
        btn->setObjectName("themeSelectionButton");
        mainLayout->addWidget(btn);
        group->addButton(btn);

        if (t.stylePath == current.stylePath) {
            btn->setEnabled(false);
            btn->setProperty("selected", true);
            btn->setStyleSheet(""); // Let QSS handle the selected style
            
            // Force style system to apply the "selected" property styling
            btn->style()->unpolish(btn);
            btn->style()->polish(btn);
            
            m_currentThemeButton = btn;
        }

        connect(btn, &QPushButton::clicked, this, [this, t, btn]() {
            if (m_currentThemeButton && m_currentThemeButton != btn) {
                m_currentThemeButton->setEnabled(true);
                m_currentThemeButton->setProperty("selected", false);
                m_currentThemeButton->style()->unpolish(m_currentThemeButton);
                m_currentThemeButton->style()->polish(m_currentThemeButton);
            }

            // Save and apply theme through ThemeManager
            ThemeManager::instance().saveTheme(t);
            ThemeManager::instance().applyTheme(t);
            
            setFixedSize(t.chooseThemeWindowPreferredSize);

            btn->setEnabled(false);
            btn->setProperty("selected", true);
            btn->style()->unpolish(btn);
            btn->style()->polish(btn);
            m_currentThemeButton = btn;
        });
    }

    // Add this connection
    connect(&LanguageManager::instance(), &LanguageManager::languageChanged, 
            this, &ThemeSelectWindow::retranslateUi);
}

void ThemeSelectWindow::init(QApplication& app) {
    ThemeManager::instance().applyCurrentThemeToAllWindows();
}

void ThemeSelectWindow::connectSignals() {
}

void ThemeSelectWindow::showEvent(QShowEvent* event) {
    QMainWindow::showEvent(event);
    
    // Reapply window effects each time the window is shown
    QPointer<ThemeSelectWindow> safeThis(this);
    QTimer::singleShot(50, this, [safeThis]() {
        if (safeThis) {
            CustomTitleBar::applyWindowEffects(safeThis);
        }
    });
}

void ThemeSelectWindow::retranslateUi() {
    // Only translate window title and header
    setWindowTitle(tr("Theme Selector"));
    if (titleBar) {
        titleBar->setTitle(tr("Theme Selector"));
    }
    header->setText(tr("> THEME SELECTOR"));
    
    qDebug() << "ThemeSelectWindow UI retranslated";
}
