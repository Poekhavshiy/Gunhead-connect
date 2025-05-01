#pragma once

#include <QMainWindow>
#include <QPushButton>
#include <QVBoxLayout>
#include <QLabel>
#include <QButtonGroup>
#include "ThemeSelect.h"

class LanguageSelectWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit LanguageSelectWindow(QWidget* parent = nullptr);

    // Static lists of languages
    static const QStringList officialLanguages;
    static const QStringList fanLanguages;

    // Helper function to add the "Coming Soon™" suffix
    static QString addComingSoonSuffix(const QString& language);

public slots:
    void onThemeChanged(const Theme& theme); // Slot to handle theme changes

private:
    QVBoxLayout* mainLayout;
    QButtonGroup* languageButtons;
    QLabel* header;
    QLabel* fanHeader;
    QPushButton* selectButton;
    QPushButton* m_currentLanguageButton = nullptr;

    void connectSignals();
    void retranslateUi();
    void setLanguageButton(QVBoxLayout* mainLayout, QButtonGroup* languageButtons, const QString& lang, const QString& currentLanguage, QWidget* central);
};