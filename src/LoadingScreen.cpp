#include "LoadingScreen.h"
#include <QVBoxLayout>
#include <QApplication>
#include <QScreen>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include <QCoreApplication> // For tr()

LoadingScreen::LoadingScreen(QWidget* parent) : QDialog(parent, Qt::Window | Qt::FramelessWindowHint) {
    // Set up the dialog
    setWindowTitle(tr("Loading Gunhead Connect"));
    setWindowIcon(QIcon(":/icons/Gunhead.ico"));  // Ensure the application icon is set

    // Ensure the window is recognized by the taskbar
    setAttribute(Qt::WA_ShowWithoutActivating, false);
    setAttribute(Qt::WA_QuitOnClose, false);  // Prevent quitting when the loading screen closes

    setFixedSize(240, 240);

    // Center on screen
    QRect screenGeometry = QApplication::primaryScreen()->geometry();
    int x = (screenGeometry.width() - width()) / 2;
    int y = (screenGeometry.height() - height()) / 2;
    move(x, y);

    // Create layout with minimal margins
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(5, 5, 5, 5); // Reduce margins

    // Replace GIF with the gunhead-logo.png resource
    imageLabel = new QLabel(this);
    imageLabel->setFixedSize(220, 140); // Adjust size to be wider but not as tall

    // Center align content within the label itself
    imageLabel->setAlignment(Qt::AlignCenter);

    // Load the gunhead-logo.png resource
    QPixmap loadingIcon(":/icons/Gunhead.png");

    if (!loadingIcon.isNull()) {
        qDebug() << tr("Gunhead logo loaded successfully");
        // Store the original pixmap
        originalPixmap = loadingIcon.scaled(128, 128, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        imageLabel->setPixmap(originalPixmap);

        // Create rotation animation
        rotationAnimation = new QPropertyAnimation(this, "rotation");
        rotationAnimation->setDuration(2000); // 2 seconds per rotation
        rotationAnimation->setLoopCount(-1);  // infinite
        rotationAnimation->setStartValue(0.0);
        rotationAnimation->setEndValue(360.0);
        rotationAnimation->setEasingCurve(QEasingCurve::Linear);
        rotationAnimation->start();
    } else {
        qDebug() << tr("Failed to load gunhead logo, using text fallback");
        imageLabel->setText(tr("Loading..."));
        imageLabel->setAlignment(Qt::AlignCenter);
        imageLabel->setStyleSheet("font-size: 24px; color: #CEFB17;");  // Set fallback text color
    }

    //layout->addWidget(imageLabel, 0, Qt::AlignCenter);
    layout->addWidget(imageLabel, 0, Qt::AlignTop | Qt::AlignHCenter);
    // Status message
    statusLabel = new QLabel(tr("Initializing..."), this);
    statusLabel->setAlignment(Qt::AlignCenter);
    statusLabel->setStyleSheet("color: #CEFB17;");  // Set status message color
    layout->addWidget(statusLabel);

    // Progress bar
    progressBar = new QProgressBar(this);
    progressBar->setRange(0, 100);
    progressBar->setValue(0);
    progressBar->setTextVisible(true);
    progressBar->setStyleSheet(
        "QProgressBar {"
        "    border: 2px solid #CEFB17;"
        "    border-radius: 5px;"
        "    text-align: center;"
        "    color: #CEFB17;"
        "}"
        "QProgressBar::chunk {"
        "    background-color: #CEFB17;"
        "    width: 10px;"
        "}"
    );  // Set progress bar color
    layout->addWidget(progressBar);

    setLayout(layout);

    // Hardcode the "originalsleek" theme for the loading screen's actual elements only
    // Applies to: background, status label, progress bar, and fallback text

    // Main background and font color
    this->setStyleSheet(
        "QDialog {"
        "    background-color: #121212;"
        "    color: #f0f0f0;"
        "    font-family: 'Roboto', sans-serif;"
        "    font-size: 14px;"
        "}"
        "QLabel {"
        "    color: #bbbbbb;"
        "    font-family: 'Roboto', sans-serif;"
        "    font-size: 14px;"
        "}"
        "QLabel#statusLabel {"
        "    font-size: 16px;"
        "    color: #f0f0f0;"
        "}"
        "QProgressBar {"
        "    border: 2px solid #CEFB17;"
        "    border-radius: 5px;"
        "    text-align: center;"
        "    color: #CEFB17;"
        "    background-color: #181818;"
        "}"
        "QProgressBar::chunk {"
        "    background-color: #CEFB17;"
        "    width: 10px;"
        "}"
                "QProgressBar {"
        "    border: 2px solid #CEFB17;"
        "    border-radius: 5px;"
        "    text-align: center;"
        "    color: #CEFB17;"
        "}"
        "QProgressBar::chunk {"
        "    background-color: #CEFB17;"
        "    width: 10px;"
        "}"
    );

    // Set object name for status label so the style applies
    statusLabel->setObjectName("statusLabel");

    // Force a hardcoded dark background, regardless of global theme
    this->setStyleSheet("background-color: #181818;"); // or your preferred default color

    // Also clear stylesheets for all children to avoid theme bleed
    for (QObject* child : this->children()) {
        if (QWidget* w = qobject_cast<QWidget*>(child)) {
            w->setStyleSheet("");
        }
    }
}

void LoadingScreen::updateProgress(int value, const QString& message) {
    progressBar->setValue(value);
    statusLabel->setText(tr("%1").arg(message));

    // Process events to update the UI immediately
    QApplication::processEvents();
}

void LoadingScreen::setMaximum(int max) {
    progressBar->setMaximum(max);
}

void LoadingScreen::setRotation(qreal angle) {
    rotationAngle = angle;
    
    // Create a transform for rotation
    QTransform transform;
    transform.translate(originalPixmap.width()/2, originalPixmap.height()/2);
    transform.rotate(rotationAngle);
    transform.translate(-originalPixmap.width()/2, -originalPixmap.height()/2);
    
    // Apply the rotation to the pixmap
    QPixmap rotatedPixmap = originalPixmap.transformed(transform, Qt::SmoothTransformation);
    
    // Update the label
    imageLabel->setPixmap(rotatedPixmap);
}

qreal LoadingScreen::rotation() const { return rotationAngle; }

LoadingScreen::~LoadingScreen() {
    // Clean up any resources if needed
    // The QObjects will be automatically deleted by Qt's parent-child mechanism
}

// Add this implementation to LoadingScreen.cpp:
void LoadingScreen::showEvent(QShowEvent* event) {
    QDialog::showEvent(event);
    
    // Ensure animation is running when the window becomes visible
    if (rotationAnimation && rotationAnimation->state() != QAbstractAnimation::Running) {
        rotationAnimation->start();
    }
    
    // Process events to ensure animation starts immediately
    QApplication::processEvents();
}