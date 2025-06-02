#pragma once
#include <QDialog>
#include <QLabel>
#include <QProgressBar>

class LoadingScreen : public QDialog {
    Q_OBJECT
    
public:
    LoadingScreen(QWidget* parent = nullptr);
    ~LoadingScreen();
    
public slots:
    void updateProgress(int value, const QString& message);
    void setMaximum(int max);
    
private:
    QLabel* imageLabel;
    QLabel* statusLabel;
    QProgressBar* progressBar;
};