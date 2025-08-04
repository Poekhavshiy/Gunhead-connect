#include "language_manager.h"
#include <QApplication>
#include <QLocale>
#include <QDebug>
#include <QSettings> // ADDED

void LanguageManager::setSelectedLanguage(const QString& language) {
    if (selectedLanguage != language) {
        selectedLanguage = language;
        qDebug() << "Language changed to:" << language;
    }
}

void LanguageManager::applySelectedLanguage() {
    QString localeCode;
    qDebug() << "Applying language:" << selectedLanguage;
    
    // Match by exact name
    if (selectedLanguage == "繁體中文") localeCode = "zh";
    else if (selectedLanguage == "日本語") localeCode = "ja";
    else if (selectedLanguage == "Українська") localeCode = "uk";
    else if (selectedLanguage == "Русский" || selectedLanguage == "москаль") localeCode = "ru";
    else if (selectedLanguage == "Deutsch") localeCode = "de";
    else if (selectedLanguage == "Español") localeCode = "es";
    else if (selectedLanguage == "Français") localeCode = "fr";
    else if (selectedLanguage == "Italiano") localeCode = "it";
    else if (selectedLanguage == "Polski") localeCode = "pl";
    else if (selectedLanguage == "Português") localeCode = "pt";
    else if (selectedLanguage == "한국어") localeCode = "ko";
    else localeCode = "en";

    QString qmPath = ":/translations/lang_" + localeCode + ".qm";
    qDebug() << "Loading translation from:" << qmPath;
    
    // Remove previous translator if it exists
    qApp->removeTranslator(&translator);
    
    // Load new translator
    if (translator.load(qmPath)) {
        qApp->installTranslator(&translator);
        qDebug() << "Successfully loaded translation for:" << localeCode;
        
        // Emit signal AFTER successfully installing the translator
        emit languageChanged();
    } else {
        qDebug() << "Failed to load translation for:" << localeCode << "from path:" << qmPath;
    }
}

// ADDED: Save the current selected language to QSettings
void LanguageManager::saveSelectedLanguage() {
    QSettings settings("KillApiConnect", "KillApiConnectPlus");
    settings.setValue("Language/CurrentLanguage", selectedLanguage);
    settings.sync();
    qDebug() << "Language saved to settings:" << selectedLanguage;
}

// ADDED: Load the selected language from QSettings
void LanguageManager::loadSelectedLanguage() {
    QSettings settings("KillApiConnect", "KillApiConnectPlus");
    QString savedLanguage = settings.value("Language/CurrentLanguage", "English").toString();
    
    // Convert old locale code format to language name if needed
    if (savedLanguage == "en") savedLanguage = "English";
    else if (savedLanguage == "zh") savedLanguage = "繁體中文";
    else if (savedLanguage == "ja") savedLanguage = "日本語";
    else if (savedLanguage == "uk") savedLanguage = "Українська";
    else if (savedLanguage == "ru") savedLanguage = "Русский";
    else if (savedLanguage == "de") savedLanguage = "Deutsch";
    else if (savedLanguage == "es") savedLanguage = "Español";
    else if (savedLanguage == "fr") savedLanguage = "Français";
    else if (savedLanguage == "it") savedLanguage = "Italiano";
    else if (savedLanguage == "pl") savedLanguage = "Polski";
    else if (savedLanguage == "pt") savedLanguage = "Português";
    else if (savedLanguage == "ko") savedLanguage = "한국어";
    
    selectedLanguage = savedLanguage;
    qDebug() << "Language loaded from settings:" << selectedLanguage;
}