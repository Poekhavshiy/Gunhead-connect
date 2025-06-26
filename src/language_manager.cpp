#include "language_manager.h"
#include <QApplication>
#include <QLocale>
#include <QDebug>

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