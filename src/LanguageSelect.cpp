// LanguageSelect
#include "LanguageSelect.h"
#include "language_manager.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QButtonGroup>
#include <QStringList>

// Define the official languages (translated into their native names)
const QStringList LanguageSelectWindow::officialLanguages = {
    QObject::tr("English"),
    QObject::tr("Українська"), // Ukrainian
    QObject::tr("Русский")     // Russian
};

// Define the fan translations (translated into their native names)
const QStringList LanguageSelectWindow::fanLanguages = {
    QObject::tr("Deutsch"),               // German
    QObject::tr("Español"),               // Spanish
    QObject::tr("Français"),              // French
    QObject::tr("Italiano"),              // Italian
    QObject::tr("Polski"),                // Polish
    QObject::tr("Português"),             // Portuguese
    QObject::tr("简体中文"),               // Simplified Chinese
    QObject::tr("日本語"),                 // Japanese
    QObject::tr("한국어")                  // Korean
};

// Helper function to add the "Coming Soon™" suffix
QString LanguageSelectWindow::addComingSoonSuffix(const QString& language) {
    return language + " " + QObject::tr("(Coming Soon™)");
}

LanguageSelectWindow::LanguageSelectWindow(QWidget* parent)
    : QMainWindow(parent) {
    setWindowTitle(tr("Language Selector"));
    setObjectName("languageSelectWindow");

    // Load persisted window size
    QSettings settings("KillApiConnect", "KillApiConnectPlus");
    QSize savedSize = settings.value("chooseLanguageWindowPreferredSize", QSize(400, 600)).toSize();
    setFixedSize(savedSize);

    // Set up the central widget
    QWidget* central = new QWidget(this);
    setCentralWidget(central);

    // Set up the main layout
    mainLayout = new QVBoxLayout(central);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(10);

    // Header for official translations
    header = new QLabel(tr("> LANGUAGE SELECT"), central);
    header->setObjectName("headerLabel");
    mainLayout->addWidget(header);

    QString currentLanguage = LanguageManager::instance().getSelectedLanguage();

    // Button group for official languages
    languageButtons = new QButtonGroup(this);
    for (const QString& lang : officialLanguages) {
        setLanguageButton(mainLayout, languageButtons, lang, currentLanguage, central);
    }

    // Header for fan translations
    fanHeader = new QLabel(tr("> FAN TRANSLATIONS"), central);
    fanHeader->setObjectName("fanHeaderLabel");
    mainLayout->addWidget(fanHeader);

    // Button group for fan translations
    for (const QString& lang : fanLanguages) {
        QString langWithSuffix = addComingSoonSuffix(lang);
        setLanguageButton(mainLayout, languageButtons, langWithSuffix, currentLanguage, central);
    }

    // Connect signals and slots
    connect(&LanguageManager::instance(), &LanguageManager::languageChanged, this, &LanguageSelectWindow::retranslateUi);
    connectSignals();
}

void LanguageSelectWindow::connectSignals() {
    // Connect language buttons to set and apply the selected language
    for (QAbstractButton* abstractButton : languageButtons->buttons()) {
        QPushButton* button = qobject_cast<QPushButton*>(abstractButton); // Cast to QPushButton*
        if (!button) {
            continue; // Skip if the cast fails (shouldn't happen in this case)
        }

        connect(button, &QPushButton::clicked, this, [this, button]() {
            qDebug() << "Button text:" << button->text();
            // Reset the previously selected button to default style
            if (m_currentLanguageButton) {
                m_currentLanguageButton->setEnabled(true);
                m_currentLanguageButton->setStyleSheet("QPushButton { padding: 8px; }"); // Default style
            }

            // Update the selected button
            m_currentLanguageButton = button;
            m_currentLanguageButton->setEnabled(false);
            m_currentLanguageButton->setStyleSheet("QPushButton { padding: 8px; background-color: #00aaff; color: white; }"); // Selected style

            // Set and apply the selected language
            LanguageManager::instance().setSelectedLanguage(button->text());
            LanguageManager::instance().applySelectedLanguage();
        });
    }
}

void LanguageSelectWindow::setLanguageButton(QVBoxLayout* mainLayout, QButtonGroup* languageButtons, const QString& lang, const QString& currentLanguage, QWidget* central) {
    const QString defaultStyle = "QPushButton { padding: 8px; }";
    const QString selectedStyle = "QPushButton { padding: 8px; background-color: #00aaff; color: white; }";
    QPushButton* btn = new QPushButton(lang, central);
    btn->setObjectName("languageButton");
    btn->setStyleSheet(defaultStyle);

    // Wrap the button in a layout with margins
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->setContentsMargins(10, 0, 10, 0); // Add 10px left and right margins
    buttonLayout->addWidget(btn);

    QWidget* buttonContainer = new QWidget(central);
    buttonContainer->setLayout(buttonLayout);

    mainLayout->addWidget(buttonContainer);
    languageButtons->addButton(btn);

    // Apply selectedStyle if this is the current language
    if (lang == currentLanguage) {
        btn->setEnabled(false);
        btn->setStyleSheet(selectedStyle);
        m_currentLanguageButton = btn; // Set the current language button
    }
}

void LanguageSelectWindow::retranslateUi() {
    setWindowTitle(tr("Language Selector"));
    header->setText(tr("> LANGUAGE SELECT"));
    fanHeader->setText(tr("> FAN TRANSLATIONS"));

    QStringList allLanguages;
    for (const QString& lang : officialLanguages) {
        allLanguages.append(lang);
    }
    for (const QString& lang : fanLanguages) {
        allLanguages.append(addComingSoonSuffix(lang));
    }

    auto buttons = languageButtons->buttons();
    QString currentLanguage = LanguageManager::instance().getSelectedLanguage();

    for (int i = 0; i < allLanguages.size(); ++i) {
        QPushButton* button = qobject_cast<QPushButton*>(buttons[i]);
        if (!button) {
            continue;
        }

        button->setText(allLanguages[i]);

        // Apply selectedStyle to the currently selected language
        if (allLanguages[i] == currentLanguage) {
            button->setEnabled(false);
            button->setStyleSheet("QPushButton { padding: 8px; background-color: #00aaff; color: white; }"); // Selected style
            m_currentLanguageButton = button;
        } else {
            button->setEnabled(true);
            button->setStyleSheet("QPushButton { padding: 8px; }"); // Default style
        }
    }
}

void LanguageSelectWindow::onThemeChanged(const Theme& theme) {
    qDebug() << "Language Window:  ======   Choose Language Window Preferred Size:" << theme.chooseLanguageWindowPreferredSize;
    setFixedSize(theme.chooseLanguageWindowPreferredSize); // Resize LanguageSelectWindow
}

