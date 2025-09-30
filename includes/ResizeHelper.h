#pragma once

#include <QObject>
#include <QWidget>
#include <QMouseEvent>
#include <QPoint>
#include <QRect>

class ResizeHelper : public QObject {
    Q_OBJECT

public:
    explicit ResizeHelper(QWidget* parent, int titleBarHeight = 32);
    static const int BORDER_WIDTH = 8; // Width of resize border
    
protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    QWidget* widget;
    QPoint dragPosition;
    enum Edge {
        None = 0,
        Left = 1,
        Right = 2,
        Top = 4,
        Bottom = 8
    };
    int currentEdge;
    bool resizing;
    int titleBarHeight;
    
    int edgeAtPosition(const QPoint& pos);
    void updateCursor(int edge);
    void startResize(const QPoint& globalPos, int edge);
    void performResize(const QPoint& globalPos);
    void stopResize();
};
