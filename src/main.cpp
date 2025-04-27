#include <QApplication>
#include <QTranslator>
#include <QLocale>
#include <QFile>
#include <QFontDatabase>
#include <QSettings>
#include <QIcon>
#include "logger.h"
#include "MainWindow.h"
#include "ThemeSelect.h"
#include "SettingsWindow.h"
#include "globals.h"

void initialize_default_settings() {
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
        settings.setValue("Network/ApiServerUrl", "https://gunhead.sparked.network/api/interaction");
        settings.setValue("Network/DebugApiServerUrl", "https://bagman.sparked.network/api/interaction");
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

    init_logger(ISDEBUG, ISDEBUGTOCONSOLE, "data/");

    log_info("main()", "KillAPI-Connect started in " + std::string(ISDEBUG ? "DEBUG" : "RELEASE") + " mode");

    // Initialize settings before loading MainWindow
    initialize_default_settings();

    QApplication app(argc, argv);

    QIcon icon(":/icons/KillApi.ico");
    if (icon.isNull()) {
        qDebug() << "Failed to load icon from resources.";
    } else {
        qDebug() << "Icon loaded successfully.";
    }
    

    // Set the application icon
    app.setWindowIcon(icon);
    // Load translation
    QTranslator translator;
    QString locale = QLocale::system().name(); // e.g. "en_US"
    if (translator.load(":resources/translations/lang_" + locale + ".qm")) {
        app.installTranslator(&translator);
    }

    // Create a temporary instance of ThemeSelectWindow to initialize the theme
    ThemeSelectWindow themeSelectWindow;
    themeSelectWindow.init(app);

    // Initialize settings window
    SettingsWindow settingsWindow;
    settingsWindow.init();

    // Check for updates if enabled
    if (settingsWindow.getCheckUpdatesOnStartup()) {
        settingsWindow.checkForUpdates();
    }

    MainWindow window;
    
    //window.resize(settings.value("LogDisplay/WindowSize", QSize(310,250)).toSize());
    window.show();

#ifdef _WIN32
    // Windows logic
    log_info("main()", "Windows environment detected.");
#else
    // Linux logic
    log_info("main()", "Linux environment detected.");
#endif

    return app.exec();
}
