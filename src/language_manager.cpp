#include "language_manager.h"
#include <QApplication>
#include <QLocale>
#include <QDebug>

void LanguageManager::setSelectedLanguage(const QString& language) {
    if (selectedLanguage != language) {
        selectedLanguage = language;
        emit languageChanged(); // Notify listeners that the language has changed
        qDebug() << "Language changed:" << language;
    }
}

void LanguageManager::applySelectedLanguage() {
    QString localeCode;
    if (selectedLanguage.contains("Chinese")) localeCode = "zh_CN";
    else if (selectedLanguage.contains("Japanese")) localeCode = "ja";
    else if (selectedLanguage.contains("Ukrainian")) localeCode = "uk";
    else if (selectedLanguage.contains("Russian")) localeCode = "ru";
    else if (selectedLanguage.contains("German")) localeCode = "de";
    else if (selectedLanguage.contains("Spanish")) localeCode = "es";
    else if (selectedLanguage.contains("French")) localeCode = "fr";
    else if (selectedLanguage.contains("Italian")) localeCode = "it";
    else if (selectedLanguage.contains("Polish")) localeCode = "pl";
    else if (selectedLanguage.contains("Portuguese")) localeCode = "pt";
    else if (selectedLanguage.contains("Korean")) localeCode = "ko";
    else localeCode = "en";

    QString qmPath = ":/translations/lang_" + localeCode + ".qm";
    if (translator.load(qmPath)) {
        qApp->installTranslator(&translator);
        qDebug() << "Loaded translation for:" << localeCode;

        emit languageChanged();
    } else {
        qDebug() << "Failed to load translation for:" << localeCode;
    }
}