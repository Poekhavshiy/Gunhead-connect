#include "LoadingScreen.h"
#include <QVBoxLayout>
#include <QApplication>
#include <QScreen>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>

LoadingScreen::LoadingScreen(QWidget* parent) : QDialog(parent, Qt::Window | Qt::FramelessWindowHint) {
    // Set up the dialog
    setWindowTitle("Loading KillAPI Connect Plus");
    setWindowIcon(QIcon(":/app_icon"));  // Ensure the application icon is set

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
    QPixmap loadingIcon(":/icons/gunhead-logo.png");

    if (!loadingIcon.isNull()) {
        qDebug() << "Gunhead logo loaded successfully";
        // Scale to appropriate size
        imageLabel->setPixmap(loadingIcon.scaled(128, 128, Qt::KeepAspectRatio, Qt::SmoothTransformation));

        // Add rotation animation
        QGraphicsOpacityEffect* effect = new QGraphicsOpacityEffect(this);
        effect->setOpacity(0.85);  // Adjust opacity to make it slightly more opaque
        imageLabel->setGraphicsEffect(effect);

        QPropertyAnimation* animation = new QPropertyAnimation(effect, "opacity");
        animation->setDuration(1500);
        animation->setLoopCount(-1);  // infinite
        animation->setStartValue(0.7);
        animation->setEndValue(0.85);
        animation->setEasingCurve(QEasingCurve::InOutQuad);
        animation->start();
    } else {
        qDebug() << "Failed to load gunhead logo, using text fallback";
        imageLabel->setText("Loading...");
        imageLabel->setAlignment(Qt::AlignCenter);
        imageLabel->setStyleSheet("font-size: 24px; color: #CEFB17;");  // Set fallback text color
    }

    //layout->addWidget(imageLabel, 0, Qt::AlignCenter);
    layout->addWidget(imageLabel, 0, Qt::AlignTop | Qt::AlignHCenter);
    // Status message
    statusLabel = new QLabel("Initializing...", this);
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
}

void LoadingScreen::updateProgress(int value, const QString& message) {
    progressBar->setValue(value);
    statusLabel->setText(message);

    // Process events to update the UI immediately
    QApplication::processEvents();
}

void LoadingScreen::setMaximum(int max) {
    progressBar->setMaximum(max);
}

LoadingScreen::~LoadingScreen() {
    // Clean up any resources if needed
    // The QObjects will be automatically deleted by Qt's parent-child mechanism
}