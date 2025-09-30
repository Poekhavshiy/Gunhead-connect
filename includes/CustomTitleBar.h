#pragma once

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QPoint>

class CustomTitleBar : public QWidget {
    Q_OBJECT

public:
    explicit CustomTitleBar(QWidget* parent = nullptr, bool showMaximizeButton = true);
    
    void setTitle(const QString& title);
    QString getTitle() const;
    void setIcon(const QIcon& icon);
    void updateMaximizeButton(bool isMaximized);
    
    // Apply visual effects to parent window
    static void applyWindowEffects(QWidget* window);
    
signals:
    void minimizeClicked();
    void maximizeClicked();
    void closeClicked();

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;

private:
    QLabel* iconLabel;
    QLabel* titleLabel;
    QPushButton* minimizeButton;
    QPushButton* maximizeButton;
    QPushButton* closeButton;
    
    QPoint dragPosition;
    bool dragging;
    bool showMaximize;
    
    void setupUI();
    void setupConnections();
};
