#pragma once

#include <QMainWindow>
#include <QPushButton>
#include <QVBoxLayout>
#include <QLabel>
#include <QButtonGroup>
#include "ThemeSelect.h"
#include "CustomTitleBar.h"

class LanguageSelectWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit LanguageSelectWindow(QWidget* parent = nullptr);

    // Static lists of languages
    static const QStringList allLanguages;

public slots:
    void onThemeChanged(const Theme& theme); // Slot to handle theme changes

signals:
    void languageChanged(const QString& language);

private:
    CustomTitleBar* titleBar; // Custom title bar widget
    QVBoxLayout* mainLayout;
    QButtonGroup* languageButtons;
    QLabel* header;
    QLabel* fanHeader;
    QPushButton* selectButton;
    QPushButton* m_currentLanguageButton = nullptr;

    void connectSignals();
    void retranslateUi();
    void setLanguageButton(QVBoxLayout* mainLayout, QButtonGroup* languageButtons, const QString& lang, const QString& currentLanguage, QWidget* central);

protected:
    void showEvent(QShowEvent* event) override;
};