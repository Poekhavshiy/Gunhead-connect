#ifndef THEMESELECT_H
#define THEMESELECT_H

#include <QWidget>
#include <QMainWindow>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QButtonGroup>
#include <QVector>
#include <QString>
#include <QStyle>
#include "language_manager.h"
#include "ThemeManager.h"  // Include ThemeManager

class ThemeSelectWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit ThemeSelectWindow(QWidget* parent = nullptr);
    void init(QApplication& app);  // Call this early in main()

private:
    QVBoxLayout* mainLayout;
    QLabel* header;
    QPushButton* selectButton;
    QPushButton* m_currentThemeButton = nullptr;

    void connectSignals();
    QMainWindow* findMainWindow();

protected:
    void showEvent(QShowEvent* event) override;

private slots:
    void retranslateUi();
};

#endif // THEMESELECT_H