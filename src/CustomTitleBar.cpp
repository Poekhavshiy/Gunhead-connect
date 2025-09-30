#include "CustomTitleBar.h"
#include <QStyle>
#include <QApplication>
#include <QWindow>
#include <QGraphicsDropShadowEffect>
#include <QIcon>

#ifdef Q_OS_WIN
#include <windows.h>
#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")
#endif

// Add include for consistency, assuming BORDER_WIDTH is defined in ResizeHelper
#include "ResizeHelper.h"

CustomTitleBar::CustomTitleBar(QWidget* parent, bool showMaximizeButton)
    : QWidget(parent), dragging(false), showMaximize(showMaximizeButton) {
    setFixedHeight(32);  // Epic Games launcher uses ~32px title bar
    setObjectName("CustomTitleBar");
    setupUI();
    setupConnections();
}

void CustomTitleBar::setupUI() {
    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setContentsMargins(12, 0, 0, 0);
    layout->setSpacing(10);
    
    // Icon (20x20 for better visibility)
    iconLabel = new QLabel(this);
    iconLabel->setFixedSize(20, 20);
    iconLabel->setScaledContents(true);
    layout->addWidget(iconLabel);
    
    // Title
    titleLabel = new QLabel(this);
    titleLabel->setObjectName("titleBarLabel");
    titleLabel->setStyleSheet("font-size: 13px; font-weight: 500; color: #FFFFFF;");
    layout->addWidget(titleLabel);
    
    // Spacer
    layout->addStretch();
    
    // Minimize button
    minimizeButton = new QPushButton(this);
    minimizeButton->setObjectName("titleBarMinimize");
    minimizeButton->setFixedSize(46, 32);
    minimizeButton->setFlat(true);
    // Comment: Use Unicode for minimize button instead of SVG
    minimizeButton->setText("─");
    minimizeButton->setStyleSheet(
        "QPushButton#titleBarMinimize {"
        "   background-color: transparent;"
        "   border: none;"
        "   border-radius: 0px;"
        "   color: white;"
        "   font-size: 16px;"
        "   font-weight: bold;"
        "   text-align: center;"
        "}"
        "QPushButton#titleBarMinimize:hover {"
        "   background-color: rgba(255, 255, 255, 0.1);"
        "}"
        "QPushButton#titleBarMinimize:pressed {"
        "   background-color: rgba(255, 255, 255, 0.05);"
        "}"
    );
    layout->addWidget(minimizeButton);
    
    // Maximize button (optional)
    if (showMaximize) {
        maximizeButton = new QPushButton(this);
        maximizeButton->setObjectName("titleBarMaximize");
        maximizeButton->setFixedSize(46, 32);
        maximizeButton->setFlat(true);
        // Comment: Use Unicode for maximize/restore button
        maximizeButton->setText("□");
        maximizeButton->setStyleSheet(
            "QPushButton#titleBarMaximize {"
            "   background-color: transparent;"
            "   border: none;"
            "   border-radius: 0px;"
            "   color: white;"
            "   font-size: 16px;"
            "   text-align: center;"
            "}"
            "QPushButton#titleBarMaximize:hover {"
            "   background-color: rgba(255, 255, 255, 0.1);"
            "}"
            "QPushButton#titleBarMaximize:pressed {"
            "   background-color: rgba(255, 255, 255, 0.05);"
            "}"
        );
        layout->addWidget(maximizeButton);
    } else {
        maximizeButton = nullptr;
    }
    
    // Close button
    closeButton = new QPushButton(this);
    closeButton->setObjectName("titleBarClose");
    closeButton->setFixedSize(46, 32);
    closeButton->setFlat(true);
    // Comment: Use Unicode for close button
    closeButton->setText("×");
    closeButton->setStyleSheet(
        "QPushButton#titleBarClose {"
        "   background-color: transparent;"
        "   border: none;"
        "   border-radius: 0px;"
        "   color: white;"
        "   font-size: 18px;"
        "   font-weight: bold;"
        "   text-align: center;"
        "}"
        "QPushButton#titleBarClose:hover {"
        "   background-color: #C42B1C;"
        "}"
        "QPushButton#titleBarClose:pressed {"
        "   background-color: #9E2217;"
        "}"
    );
    layout->addWidget(closeButton);
    
    // Apply dark background with slight transparency for theme compatibility
    setStyleSheet(
        "QWidget#CustomTitleBar {"
        "   background-color: rgba(14, 14, 16, 0.95);"
        "   border-bottom: 1px solid rgba(42, 42, 46, 0.5);"
        "}"
    );
}

void CustomTitleBar::setupConnections() {
    connect(minimizeButton, &QPushButton::clicked, this, &CustomTitleBar::minimizeClicked);
    if (maximizeButton) {
        connect(maximizeButton, &QPushButton::clicked, this, &CustomTitleBar::maximizeClicked);
    }
    connect(closeButton, &QPushButton::clicked, this, &CustomTitleBar::closeClicked);
}

void CustomTitleBar::setTitle(const QString& title) {
    titleLabel->setText(title);
}

QString CustomTitleBar::getTitle() const {
    return titleLabel->text();
}

// Comment: Remove tinting for logo to preserve original colors and improve visibility
void CustomTitleBar::setIcon(const QIcon& icon) {
    if (!icon.isNull()) {
        QPixmap pixmap = icon.pixmap(20, 20);
        if (!pixmap.isNull()) {
            iconLabel->setPixmap(pixmap);
            iconLabel->show();
        } else {
            iconLabel->hide();
        }
    } else {
        iconLabel->hide();
    }
}

// Comment: Update maximize button to toggle between □ and ❐ Unicode characters
void CustomTitleBar::updateMaximizeButton(bool isMaximized) {
    if (maximizeButton) {
        maximizeButton->setText(isMaximized ? "❐" : "□");
    }
}

void CustomTitleBar::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        QPoint pos = event->pos();
        if (pos.y() <= ResizeHelper::BORDER_WIDTH) {
            // Ignore drag if on top edge to allow resizing
            event->ignore();
            return;
        }
        dragging = true;
        // Get the top-level window, not just parent
        QWidget* window = this->window();
        dragPosition = event->globalPosition().toPoint() - window->frameGeometry().topLeft();
        event->accept();
    }
}

void CustomTitleBar::mouseMoveEvent(QMouseEvent* event) {
    if (dragging && (event->buttons() & Qt::LeftButton)) {
        // Always use the top-level window for moving
        QWidget* window = this->window();
        if (window) {
            // If maximized, restore before moving
            if (window->isMaximized()) {
                window->showNormal();
                // Recalculate drag position for smaller window
                dragPosition = QPoint(window->width() / 2, dragPosition.y());
            }
            window->move(event->globalPosition().toPoint() - dragPosition);
        }
        event->accept();
    }
}

void CustomTitleBar::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        dragging = false;
        event->accept();
    }
}

void CustomTitleBar::mouseDoubleClickEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton && maximizeButton) {
        emit maximizeClicked();
        event->accept();
    }
}

void CustomTitleBar::applyWindowEffects(QWidget* window) {
    if (!window) return;
    
#ifdef Q_OS_WIN
    // Apply Windows 11-style rounded corners and shadow using DWM
    HWND hwnd = (HWND)window->winId();
    
    // Enable shadow for the window
    MARGINS margins = {1, 1, 1, 1};
    DwmExtendFrameIntoClientArea(hwnd, &margins);
    
    // Set rounded corners (Windows 11+)
    // Note: DWMWA_WINDOW_CORNER_PREFERENCE = 33, DWMWCP_ROUND = 2
    int cornerPreference = 2; // DWMWCP_ROUND
    DwmSetWindowAttribute(hwnd, (DWORD)33, &cornerPreference, sizeof(cornerPreference));
    
    // Set window border color to dark
    COLORREF borderColor = RGB(42, 42, 46); // #2A2A2E
    DwmSetWindowAttribute(hwnd, (DWORD)34, &borderColor, sizeof(borderColor));
    
    // Enable immersive dark mode
    int useDarkMode = 1;
    DwmSetWindowAttribute(hwnd, 20, &useDarkMode, sizeof(useDarkMode));
#else
    // For non-Windows platforms, use Qt graphics effects
    QGraphicsDropShadowEffect* shadow = new QGraphicsDropShadowEffect(window);
    shadow->setBlurRadius(20);
    shadow->setColor(QColor(0, 0, 0, 100));
    shadow->setOffset(0, 2);
    window->setGraphicsEffect(shadow);
#endif
}
