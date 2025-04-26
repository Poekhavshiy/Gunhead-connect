#pragma once

#include <QObject>
#include <QString>
#include <QTranslator> // Add this include

class LanguageManager : public QObject {
    Q_OBJECT

public:
    static LanguageManager& instance() {
        static LanguageManager instance;
        return instance;
    }
    QString getSelectedLanguage() const { return selectedLanguage; }
    void setSelectedLanguage(const QString& language);
    void applySelectedLanguage();

signals:
    void languageChanged();

private:
    LanguageManager() = default;
    QString selectedLanguage;
    QTranslator translator; // Add this member
};

