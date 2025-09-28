#pragma once

#include <QDialog>
#include <QLabel>
#include <QProgressBar>
#include <QPropertyAnimation>
#include <QTimer>
#include <QWidget>
#include <QPainter>

// Custom spinning widget for the loading indicator
class SpinnerWidget : public QWidget {
    Q_OBJECT
    Q_PROPERTY(qreal rotation READ rotation WRITE setRotation)
    
public:
    explicit SpinnerWidget(QWidget* parent = nullptr);
    
    qreal rotation() const { return m_rotation; }
    void setRotation(qreal rotation);
    
protected:
    void paintEvent(QPaintEvent* event) override;
    
private:
    qreal m_rotation;
};

class LoadingScreen : public QDialog {
    Q_OBJECT

public:
    explicit LoadingScreen(QWidget* parent = nullptr);
    ~LoadingScreen();

    void updateProgress(int value, const QString& message);

private:
    QLabel* statusLabel;
    SpinnerWidget* spinnerWidget;
    QPropertyAnimation* spinnerAnimation;
};