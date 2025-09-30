// LanguageSelect
#include "LanguageSelect.h"
#include "language_manager.h"
#include "globals.h"
#include "ThemeManager.h" // Change this from "ThemeSelect.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QButtonGroup>
#include <QStringList>
#include <QTimer>
#include <QShowEvent>
#include <QPointer>

// Define all languages displayed in their native names
const QStringList LanguageSelectWindow::allLanguages = {
    "English",               // English
    "Українська",            // Ukrainian
    "Русский",               // Russian
    "Deutsch",               // German
    "Español",               // Spanish
    "Français",              // French
    "Italiano",              // Italian
    "Polski",                // Polish
    "Português",             // Portuguese
    "繁體中文",               // Traditional Chinese
    "日本語",                 // Japanese
    "한국어"                  // Korean
};

LanguageSelectWindow::LanguageSelectWindow(QWidget* parent)
    : QMainWindow(parent) {
    setWindowTitle(tr("Language Selector"));
    setWindowIcon(QIcon(":/icons/Gunhead.ico"));
    setObjectName("languageSelectWindow");

    // Load current theme from ThemeManager instead of ThemeSelectWindow
    Theme currentTheme = ThemeManager::instance().loadCurrentTheme();
    
    // Apply the theme's preferred size
    QSize windowSize = currentTheme.chooseLanguageWindowPreferredSize;
    setFixedSize(windowSize);

    // Set up the central widget
    QWidget* central = new QWidget(this);
    central->setObjectName("languageSelectContainer");
    central->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setCentralWidget(central);

    // Set up the main layout
    mainLayout = new QVBoxLayout(central);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(10);

    // Header for language selection
    header = new QLabel(tr("> LANGUAGE SELECT"), central);
    header->setObjectName("headerLabel");
    mainLayout->addWidget(header);

    QString currentLanguage = LanguageManager::instance().getSelectedLanguage();

    // Button group for all languages
    languageButtons = new QButtonGroup(this);
    for (const QString& lang : allLanguages) {
        setLanguageButton(mainLayout, languageButtons, lang, currentLanguage, central);
    }
    
    // Add a spacer at the bottom to create padding
    QSpacerItem* bottomPadding = new QSpacerItem(20, 20, QSizePolicy::Minimum, QSizePolicy::Fixed);
    mainLayout->addSpacerItem(bottomPadding);

    // Connect signals and slots
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged,
            this, &LanguageSelectWindow::onThemeChanged);
    connect(&LanguageManager::instance(), &LanguageManager::languageChanged, this, &LanguageSelectWindow::retranslateUi);
    connectSignals();
}

void LanguageSelectWindow::setLanguageButton(QVBoxLayout* mainLayout, QButtonGroup* languageButtons, const QString& lang, const QString& currentLanguage, QWidget* central) {
    // Determine the display text for the button
    QString displayText = lang;
    
    // Work around to fix encoding issue.
    if (QString::fromStdString(systemLocale).startsWith("uk") && 
        currentLanguage == "Українська" && 
        lang == "Русский") {
        displayText = QString::fromUtf8(QByteArray::fromHex("d09cd0bed181d0bad0b0d0bbd18c"));
    }
    
    QPushButton* btn = new QPushButton(displayText, central);
    btn->setObjectName("languageSelectionButton");
    
    // Store the actual language value as a property for logic purposes
    btn->setProperty("actualLanguage", lang);
    
    // Wrap the button in a layout with margins
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->setContentsMargins(10, 0, 10, 0);
    buttonLayout->addWidget(btn);

    QWidget* buttonContainer = new QWidget(central);
    buttonContainer->setLayout(buttonLayout);

    mainLayout->addWidget(buttonContainer);
    languageButtons->addButton(btn);

    // Apply selected state if this is the current language
    if (lang == currentLanguage) {
        btn->setEnabled(false);
        btn->setProperty("selected", true);
        btn->setStyleSheet(""); // Explicitly clear any inline styles
        
        // Force style system to apply the "selected" property styling
        btn->style()->unpolish(btn);
        btn->style()->polish(btn);
        
        m_currentLanguageButton = btn;
    }
}

void LanguageSelectWindow::connectSignals() {
    // Connect language buttons to set and apply the selected language
    for (QAbstractButton* abstractButton : languageButtons->buttons()) {
        QPushButton* button = qobject_cast<QPushButton*>(abstractButton);
        if (!button) {
            continue;
        }

        connect(button, &QPushButton::clicked, this, [this, button]() {
            // Use the actual language value, not the display text
            QString actualLanguage = button->property("actualLanguage").toString();
            qDebug() << "Button clicked with actual language:" << actualLanguage;
            
            // Reset the previously selected button
            if (m_currentLanguageButton && m_currentLanguageButton != button) {
                m_currentLanguageButton->setEnabled(true);
                m_currentLanguageButton->setProperty("selected", false);
                m_currentLanguageButton->setStyleSheet(""); // Let QSS handle styling
                
                // Force style recalculation
                m_currentLanguageButton->style()->unpolish(m_currentLanguageButton);
                m_currentLanguageButton->style()->polish(m_currentLanguageButton);
            }

            // Update the selected button
            m_currentLanguageButton = button;
            m_currentLanguageButton->setEnabled(false);
            m_currentLanguageButton->setProperty("selected", true);
            m_currentLanguageButton->setStyleSheet(""); // Let QSS handle styling
            
            // Force style recalculation
            m_currentLanguageButton->style()->unpolish(m_currentLanguageButton);
            m_currentLanguageButton->style()->polish(m_currentLanguageButton);

            // Set and apply the selected language using actual language value
            LanguageManager::instance().setSelectedLanguage(actualLanguage);
            LanguageManager::instance().saveSelectedLanguage(); // ADDED: Save to settings
            LanguageManager::instance().applySelectedLanguage();
        });
    }
}

void LanguageSelectWindow::showEvent(QShowEvent* event) {
    QMainWindow::showEvent(event);
}

void LanguageSelectWindow::retranslateUi() {
    // Translate window title and header
    setWindowTitle(tr("Language Selector"));
    header->setText(tr("> LANGUAGE SELECT"));

    // Get current language
    QString currentLanguage = LanguageManager::instance().getSelectedLanguage();
    
    // Update button styling and text based on current selection
    for (QAbstractButton* button : languageButtons->buttons()) {
        QPushButton* btn = qobject_cast<QPushButton*>(button);
        if (!btn) continue;
        
        // Get the actual language this button represents
        QString actualLanguage = btn->property("actualLanguage").toString();
        
        // Update display text based on current conditions
        QString displayText = actualLanguage;
        if (QString::fromStdString(systemLocale).startsWith("uk") && 
            currentLanguage == "Українська" && 
            actualLanguage == "Русский") {
            displayText = "Москаль";
        }
        btn->setText(displayText);
        
        // Update property based on current selection
        bool isSelected = (actualLanguage == currentLanguage);
        if (isSelected) {
            btn->setEnabled(false);
            btn->setProperty("selected", true);
            btn->setStyleSheet(""); 
            m_currentLanguageButton = btn;
        } else {
            btn->setEnabled(true);
            btn->setProperty("selected", false);
            btn->setStyleSheet(""); 
        }
        
        // Force style system to apply property-based styling
        btn->style()->unpolish(btn);
        btn->style()->polish(btn);
    }
}

void LanguageSelectWindow::onThemeChanged(const Theme& theme) {
    // Update window size from theme when theme changes
    setFixedSize(theme.chooseLanguageWindowPreferredSize);
    qDebug() << "Language window size updated from theme:" << theme.chooseLanguageWindowPreferredSize;
}