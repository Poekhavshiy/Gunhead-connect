#include "ResizeHelper.h"
#include <QApplication>
#include <QMouseEvent>
#include <QHoverEvent>
#include <QDebug>

ResizeHelper::ResizeHelper(QWidget* parent, int titleBarHeight)
    : QObject(parent), widget(parent), currentEdge(None), resizing(false), titleBarHeight(titleBarHeight) {
    widget->installEventFilter(this);
    widget->setAttribute(Qt::WA_Hover);
    widget->setMouseTracking(true);
}

bool ResizeHelper::eventFilter(QObject* obj, QEvent* event) {
    if (obj != widget) {
        return QObject::eventFilter(obj, event);
    }

    switch (event->type()) {
    case QEvent::HoverMove: {
        QHoverEvent* hoverEvent = static_cast<QHoverEvent*>(event);
        if (!resizing) {
            int edge = edgeAtPosition(hoverEvent->position().toPoint());
            updateCursor(edge);
        }
        break;
    }
    case QEvent::MouseButtonPress: {
        QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->button() == Qt::LeftButton) {
            int edge = edgeAtPosition(mouseEvent->pos());
            if (edge != None) {
                startResize(mouseEvent->globalPosition().toPoint(), edge);
                return true;
            }
        }
        break;
    }
    case QEvent::MouseMove: {
        if (resizing) {
            QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
            performResize(mouseEvent->globalPosition().toPoint());
            return true;
        }
        break;
    }
    case QEvent::MouseButtonRelease: {
        if (resizing) {
            stopResize();
            return true;
        }
        break;
    }
    case QEvent::Leave: {
        if (!resizing) {
            widget->setCursor(Qt::ArrowCursor);
        }
        break;
    }
    default:
        break;
    }

    return QObject::eventFilter(obj, event);
}

int ResizeHelper::edgeAtPosition(const QPoint& pos) {
    int edge = None;
    
    // Check if we're in the title bar area (exclude it from top resize)
    bool inTitleBar = (pos.y() < titleBarHeight);
    
    if (pos.x() <= BORDER_WIDTH) {
        edge |= Left;
    } else if (pos.x() >= widget->width() - BORDER_WIDTH) {
        edge |= Right;
    }
    
    // Only allow top edge resize if NOT in title bar area
    if (!inTitleBar && pos.y() <= BORDER_WIDTH) {
        edge |= Top;
    } else if (pos.y() >= widget->height() - BORDER_WIDTH) {
        edge |= Bottom;
    }
    
    return edge;
}

void ResizeHelper::updateCursor(int edge) {
    if (edge == (Top | Left) || edge == (Bottom | Right)) {
        widget->setCursor(Qt::SizeFDiagCursor);
    } else if (edge == (Top | Right) || edge == (Bottom | Left)) {
        widget->setCursor(Qt::SizeBDiagCursor);
    } else if (edge == Top || edge == Bottom) {
        widget->setCursor(Qt::SizeVerCursor);
    } else if (edge == Left || edge == Right) {
        widget->setCursor(Qt::SizeHorCursor);
    } else {
        widget->setCursor(Qt::ArrowCursor);
    }
}

void ResizeHelper::startResize(const QPoint& globalPos, int edge) {
    resizing = true;
    currentEdge = edge;
    dragPosition = globalPos;
    updateCursor(edge);
}

void ResizeHelper::performResize(const QPoint& globalPos) {
    if (!resizing || currentEdge == None) {
        return;
    }

    QPoint delta = globalPos - dragPosition;
    QRect geometry = widget->geometry();
    
    if (currentEdge & Left) {
        int newWidth = geometry.width() - delta.x();
        if (newWidth >= widget->minimumWidth()) {
            geometry.setLeft(geometry.left() + delta.x());
        }
    }
    if (currentEdge & Right) {
        int newWidth = geometry.width() + delta.x();
        if (newWidth >= widget->minimumWidth()) {
            geometry.setRight(geometry.right() + delta.x());
        }
    }
    if (currentEdge & Top) {
        int newHeight = geometry.height() - delta.y();
        if (newHeight >= widget->minimumHeight()) {
            geometry.setTop(geometry.top() + delta.y());
        }
    }
    if (currentEdge & Bottom) {
        int newHeight = geometry.height() + delta.y();
        if (newHeight >= widget->minimumHeight()) {
            geometry.setBottom(geometry.bottom() + delta.y());
        }
    }
    
    widget->setGeometry(geometry);
    dragPosition = globalPos;
}

void ResizeHelper::stopResize() {
    resizing = false;
    currentEdge = None;
    widget->setCursor(Qt::ArrowCursor);
}
