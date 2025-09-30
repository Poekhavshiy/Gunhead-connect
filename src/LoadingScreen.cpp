#include "LoadingScreen.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QApplication>
#include <QScreen>
#include <QCoreApplication> // For tr()
#include <QProgressBar>
#include <QPropertyAnimation>
#include <QGraphicsEffect>
#include <QRegion>
#include <QPainterPath>
#include <QTimer>
#include <QTransform>
#include <QFont>
#include <QPainter>
#include <QPaintEvent>
#include <QPointer>

// SpinnerWidget implementation
SpinnerWidget::SpinnerWidget(QWidget* parent) : QWidget(parent), m_rotation(0) {
    setFixedSize(36, 36);
}

void SpinnerWidget::setRotation(qreal rotation) {
    m_rotation = rotation;
    update(); // Trigger a repaint
}

void SpinnerWidget::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event)
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // Save the current state
    painter.save();
    
    // Move to center and rotate
    painter.translate(width() / 2.0, height() / 2.0);
    painter.rotate(m_rotation);
    painter.translate(-width() / 2.0, -height() / 2.0);
    
    // Draw the spinner ring
    QPen pen;
    pen.setWidth(6);
    pen.setColor(QColor(0x33, 0x33, 0x33)); // #333
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    
    // Draw main circle
    QRect rect(3, 3, width() - 6, height() - 6); // Account for pen width
    painter.drawEllipse(rect);
    
    // Draw the white arc (top portion)
    pen.setColor(Qt::white);
    painter.setPen(pen);
    painter.drawArc(rect, 90 * 16, 90 * 16); // 90 degrees starting from top
    
    // Restore painter state
    painter.restore();
}

LoadingScreen::LoadingScreen(QWidget* parent) : QDialog(parent) {
    // Set up the dialog
    setWindowTitle(tr("Loading Gunhead Connect"));
    setWindowIcon(QIcon(":/icons/Gunhead.ico"));

    // Ensure the window is recognized by the taskbar
    setAttribute(Qt::WA_ShowWithoutActivating, false);
    setAttribute(Qt::WA_QuitOnClose, false);  // Prevent quitting when the loading screen closes

    setFixedSize(600, 400);

    // Center on screen
    QRect screenGeometry = QApplication::primaryScreen()->geometry();
    int x = (screenGeometry.width() - width()) / 2;
    int y = (screenGeometry.height() - height()) / 2;
    move(x, y);

    // Set background color
    setStyleSheet("QDialog { background-color: black; }");

    // Create main layout
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // Content widget with background
    QWidget* contentWidget = new QWidget(this);
    contentWidget->setStyleSheet(
        "background-image: url(:/loading screens/_welcome.png);"
        "background-position: center;"
        "background-repeat: no-repeat;"
        "background-color: black;"
    );
    QVBoxLayout* contentLayout = new QVBoxLayout(contentWidget);
    contentLayout->setContentsMargins(20, 20, 20, 0);

    // Welcome label 
    QLabel* welcomeLabel = new QLabel(tr("Gunhead Connect"), contentWidget);
    welcomeLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    welcomeLabel->setStyleSheet("font-family: Roboto,-apple-system,BlinkMacSystemFont,Segoe UI,sans-serif; font-size: 28px; font-weight: bold; color: white; margin-left: 280px; margin-top: 50px; background: transparent;");
    contentLayout->addWidget(welcomeLabel);

    // Subtitle lines
    QLabel* subtitleLabel1 = new QLabel(tr("Start your game."), contentWidget);
    subtitleLabel1->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    subtitleLabel1->setStyleSheet("font-family: Roboto,-apple-system,BlinkMacSystemFont,Segoe UI,sans-serif; font-size: 14px; color: lightgray; margin-left: 280px; margin-top: 5px; background: transparent;");
    contentLayout->addWidget(subtitleLabel1);

    QLabel* subtitleLabel2 = new QLabel(tr("Record your gameplay."), contentWidget);
    subtitleLabel2->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    subtitleLabel2->setStyleSheet("font-family: Roboto,-apple-system,BlinkMacSystemFont,Segoe UI,sans-serif; font-size: 14px; color: lightgray; margin-left: 280px; margin-bottom: 10px; background: transparent;");
    contentLayout->addWidget(subtitleLabel2);

    contentLayout->addStretch();

    mainLayout->addWidget(contentWidget);

    // Footer widget
    QWidget* footer = new QWidget(this);
    footer->setFixedHeight(80);
    footer->setStyleSheet("background-color: #161617; border-bottom-left-radius: 10px; border-bottom-right-radius: 10px;");

    QHBoxLayout* footerLayout = new QHBoxLayout(footer);
    footerLayout->setContentsMargins(20, 0, 20, 0);

    // Left side - Gunhead logo/text with icon
    QHBoxLayout* logoLayout = new QHBoxLayout();
    logoLayout->setSpacing(8);
    
    QLabel* logoIcon = new QLabel(footer);
    QPixmap gunheadIcon(":/icons/Gunhead.png");
    logoIcon->setPixmap(gunheadIcon.scaled(24, 24, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    logoLayout->addWidget(logoIcon);
    
    QLabel* appLabel = new QLabel("Gunhead", footer);
    appLabel->setStyleSheet("font-family: Roboto,-apple-system,BlinkMacSystemFont,Segoe UI,sans-serif; color: white; font-size: 18px; font-weight: bold;");
    logoLayout->addWidget(appLabel);
    
    footerLayout->addLayout(logoLayout);

    footerLayout->addStretch();

    // Right side - status message and spinner grouped together
    QHBoxLayout* rightSideLayout = new QHBoxLayout();
    rightSideLayout->setSpacing(15); // Space between text and spinner
    
    // Status message in vertical layout (2 rows) aligned to the right
    QVBoxLayout* statusLayout = new QVBoxLayout();
    statusLayout->setSpacing(2); // Small gap between rows
    statusLayout->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    
    statusLabel = new QLabel(tr("Gunhead is getting ready"), footer);
    statusLabel->setAlignment(Qt::AlignRight);
    statusLabel->setStyleSheet("font-family: Roboto,-apple-system,BlinkMacSystemFont,Segoe UI,sans-serif; color: lightgray; font-size: 14px;");
    statusLayout->addWidget(statusLabel);

    QLabel* pleaseWaitLabel = new QLabel(tr("Please wait..."), footer);
    pleaseWaitLabel->setAlignment(Qt::AlignRight);
    pleaseWaitLabel->setStyleSheet("font-family: Roboto,-apple-system,BlinkMacSystemFont,Segoe UI,sans-serif; color: #999999; font-size: 12px;"); // Darker and smaller
    statusLayout->addWidget(pleaseWaitLabel);
    
    rightSideLayout->addLayout(statusLayout);

    // Custom spinning loading indicator
    spinnerWidget = new SpinnerWidget(footer);
    
    // Create smooth rotation animation
    spinnerAnimation = new QPropertyAnimation(spinnerWidget, "rotation", this);
    spinnerAnimation->setDuration(800); // 0.8 seconds per rotation (faster)
    spinnerAnimation->setStartValue(0);
    spinnerAnimation->setEndValue(360);
    spinnerAnimation->setLoopCount(-1); // Infinite loop
    spinnerAnimation->start();
    
    rightSideLayout->addWidget(spinnerWidget);
    
    footerLayout->addLayout(rightSideLayout);

    mainLayout->addWidget(footer);

    setLayout(mainLayout);
    
    // Apply rounded corners using a mask but preserve border with smoother edges
    QPainterPath path;
    path.addRoundedRect(rect(), 12, 12); // Match border-radius
    QRegion region(path.toFillPolygon().toPolygon());
    setMask(region);
    
    // Enable antialiasing for smoother edges
    setAttribute(Qt::WA_NoSystemBackground, true);
    setAutoFillBackground(false);
}

void LoadingScreen::updateProgress(int value, const QString& message) {
    statusLabel->setText(tr("Connecting to server..."));

    // Process events to update the UI immediately
    QApplication::processEvents();
}


LoadingScreen::~LoadingScreen() {
    // Clean up any resources if needed
    // The QObjects will be automatically deleted by Qt's parent-child mechanism
}

// Include the moc file for SpinnerWidget
#include "LoadingScreen.moc"