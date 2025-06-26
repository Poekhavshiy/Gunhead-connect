#pragma once

#include <QDialog>
#include <QLabel>
#include <QProgressBar>
#include <QPropertyAnimation>

class LoadingScreen : public QDialog {
    Q_OBJECT
    Q_PROPERTY(qreal rotation READ rotation WRITE setRotation)

public:
    explicit LoadingScreen(QWidget* parent = nullptr);
    ~LoadingScreen();

    void updateProgress(int value, const QString& message);
    void setMaximum(int max);
    void setRotation(qreal angle);
    qreal rotation() const;

protected:
    void showEvent(QShowEvent* event) override;

private:
    QLabel* imageLabel;
    QLabel* statusLabel;
    QProgressBar* progressBar;
    QPixmap originalPixmap;
    qreal rotationAngle = 0;
    QPropertyAnimation* rotationAnimation;
};