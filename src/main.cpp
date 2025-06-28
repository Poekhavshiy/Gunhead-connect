#include <QApplication>
#include <QFutureWatcher>
#include <QtConcurrent/QtConcurrent>
#include <QTranslator>
#include <QLocale>
#include <QFile>
#include <QFontDatabase>
#include <QSettings>
#include <QIcon>
#include <QTimer>
#include <QLocalSocket>
#include <QLocalServer>

#include "logger.h"
#include "LoadingScreen.h"
#include "MainWindow.h"
#include "ThemeSelect.h"
#include "SettingsWindow.h"
#include "globals.h"
#include "SoundPlayer.h"
#include "version.h" // Compile time version header, always throws error in IDE because it does not exist until compiled in.

// Define the server name for IPC
#define SERVER_NAME "KillApiConnectPlusSingleInstance"

void initialize_default_settings() {
    QCoreApplication::setOrganizationName(APP_ORGANIZATION);
    QCoreApplication::setOrganizationDomain(APP_DOMAIN);
    QCoreApplication::setApplicationName(APP_NAME);
    QCoreApplication::setApplicationVersion(APP_VERSION);

    QSettings settings("KillAPI", "KillAPI-Connect-Plus");

    // Check if settings have already been initialized
    if (!settings.contains("Initialized") || !settings.value("Initialized").toBool()) {
        // General UI/Theme defaults
        settings.setValue("Theme/CurrentTheme", "originalsleek");
        settings.setValue("Theme/FontSize", 12);
        settings.setValue("Theme/AccentColor", "#00BFFF");
        settings.setValue("Theme/BackgroundColor", "#181A1B");
        settings.setValue("Theme/ForegroundColor", "#FFFFFF");

        // Log display defaults
        settings.setValue("LogDisplay/FontSize", 12);
        settings.setValue("LogDisplay/BackgroundColor", "#000000");
        settings.setValue("LogDisplay/TextColor", "#FFFFFF");
        settings.setValue("LogDisplay/ShowPvP", false);
        settings.setValue("LogDisplay/ShowPvE", true);
        settings.setValue("LogDisplay/ShowNPCNames", true);
        settings.setValue("LogDisplay/WindowPosition", QPoint(100, 100));
        settings.setValue("LogDisplay/WindowSize", QSize(600, 400));

        // Other app-specific defaults
        settings.setValue("Language/CurrentLanguage", "en");

        // Mark as initialized
        settings.setValue("Initialized", true);
        settings.sync();
        log_info("main()", "Default settings initialized.");
    } else {
        log_info("main()", "Settings already initialized, skipping default setup.");
    }
}

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName(APP_NAME);
    app.setOrganizationName(APP_ORGANIZATION);
    app.setApplicationVersion(APP_VERSION);
    
    // Set global system locale at runtime
    systemLocale = QLocale::system().name().toStdString();
    
    // Check for existing instance first
    QLocalSocket socket;
    socket.connectToServer(SERVER_NAME);
    if (socket.waitForConnected(500)) {
        // An instance is already running
        // Send a "show" command to the running instance
        socket.write("show");
        socket.flush();
        socket.waitForBytesWritten();
        
        // Log the attempt to start a second instance
        qDebug() << "Application already running - activating existing instance";
        return 0;
    }
    
    // Parse command line args into global variables
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--debug") {
            ISDEBUG = true;
        }
        if (std::string(argv[i]) == "--console") {
            ISDEBUGTOCONSOLE = true;
        }
        if (std::string(argv[i]) == "--clear-settings") {
            // Use the same scope and format as the rest of the app
            QSettings settings(QSettings::IniFormat, QSettings::UserScope, "KillAPI", "KillAPI-Connect-Plus");
            settings.clear();
            settings.sync();

            #ifdef Q_OS_WIN
                QSettings regSettings(QSettings::NativeFormat, QSettings::UserScope, "KillAPI", "KillAPI-Connect-Plus");
                regSettings.clear();
                regSettings.sync();
            #endif

            log_info("main()", "Settings cleared successfully.");
            return 0;
        }
    }

    init_logger(ISDEBUG, ISDEBUGTOCONSOLE, "logs/");
    log_info("main()", "KillAPI-Connect started in " + std::string(ISDEBUG ? "DEBUG" : "RELEASE") + " mode");
    
    // Set up the local server to listen for other instances
    QLocalServer* localServer = new QLocalServer(&app);
    localServer->removeServer(SERVER_NAME);
    if (!localServer->listen(SERVER_NAME)) {
        qWarning() << "Could not create local server:" << localServer->errorString();
    }
    
    // Show loading screen immediately
    LoadingScreen* loadingScreen = new LoadingScreen();
    loadingScreen->show();
    QCoreApplication::processEvents(); // Force the UI to update before continuing with initialization
    
    // Show debug info if in debug mode
    if (ISDEBUG) {
        qDebug() << "Application started in DEBUG MODE";
        loadingScreen->updateProgress(5, "Debug mode enabled");
    }
    
    // Pre-initialize audio subsystem in a background thread
    loadingScreen->updateProgress(10, "Initializing audio system...");
    QFutureWatcher<void> audioWatcher;
    QFuture<void> audioFuture = QtConcurrent::run([]() {
        SoundPlayer::preInitializeAudio();
    });
    
    // Create but don't show main window yet
    loadingScreen->updateProgress(20, "Creating application window...");
    MainWindow* mainWindow = new MainWindow(nullptr, loadingScreen);
    
    // Handle incoming connections from other instances
    QObject::connect(localServer, &QLocalServer::newConnection, [mainWindow, localServer]() {
        QLocalSocket* socket = localServer->nextPendingConnection();
        if (socket) {
            socket->waitForReadyRead(1000);
            QByteArray command = socket->readAll();
            
            if (command == "show") {
                // Use our new public method instead
                mainWindow->activateFromAnotherInstance();
            }
            
            socket->deleteLater();
        }
    });
    
    // Connect initialization progress signal
    QObject::connect(mainWindow, &MainWindow::initializationProgress,
                    loadingScreen, &LoadingScreen::updateProgress);
    
    // Start background initialization in MainWindow
    loadingScreen->updateProgress(30, "Preparing application...");
    mainWindow->startBackgroundInitialization();
    
    // Wait for completion signal before showing main window
    QObject::connect(mainWindow, &MainWindow::initializationComplete, [&]() {
        loadingScreen->updateProgress(100, "Loading complete!");
        
        // Give user a moment to see the completed progress
        QTimer::singleShot(500, [&mainWindow, loadingScreen]() {
            mainWindow->show();
            
            // Add this: reapply theme after window is shown
            QTimer::singleShot(100, []() {
                ThemeManager::instance().applyCurrentThemeToAllWindows();
            });
            
            loadingScreen->accept();
        });
    });
    
    return app.exec();
}