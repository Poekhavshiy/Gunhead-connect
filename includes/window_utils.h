#pragma once

#include <QWidget>
#include <QScreen>
#include <QApplication>
#include <QDebug>

namespace WindowUtils {
    // Position a window within the visible screen area
    inline void positionWindowOnScreen(QWidget* window, QWidget* parent = nullptr) {
        if (!window) return;
        
        // Get the screen where the parent window is displayed (or primary screen if no parent)
        QScreen* screen;
        if (parent && parent->isVisible()) {
            screen = parent->screen();
        } else {
            screen = QGuiApplication::primaryScreen();
        }
        
        if (!screen) return;
        
        // Get the available screen geometry (excludes taskbar/dock)
        QRect screenGeom = screen->availableGeometry();
        
        // Calculate desired position (centered on parent if available)
        QPoint targetPos;
        if (parent && parent->isVisible()) {
            // Center on parent
            targetPos = parent->geometry().center() - QPoint(window->width()/2, window->height()/2);
        } else {
            // Center on screen
            targetPos = screenGeom.center() - QPoint(window->width()/2, window->height()/2);
        }
        
        // Ensure the window is fully visible (adjust if needed)
        if (targetPos.x() < screenGeom.left()) {
            targetPos.setX(screenGeom.left());
        } else if (targetPos.x() + window->width() > screenGeom.right()) {
            targetPos.setX(screenGeom.right() - window->width());
        }
        
        if (targetPos.y() < screenGeom.top()) {
            targetPos.setY(screenGeom.top());
        } else if (targetPos.y() + window->height() > screenGeom.bottom()) {
            targetPos.setY(screenGeom.bottom() - window->height());
        }
        
        // Set the window position
        window->move(targetPos);
        qDebug() << "Window positioned at:" << targetPos;
    }
}