TEMPLATE = app
QT += core gui widgets network multimedia
CONFIG += c++17

# Include paths for lupdate to find headers
INCLUDEPATH += ../../includes

# Define sources to be scanned
SOURCES += \
    ../../src/checkversion.cpp \
    ../../src/GameLauncher.cpp \
    ../../src/globals.cpp \
    ../../src/LanguageSelect.cpp \
    ../../src/language_manager.cpp \
    ../../src/LoadingScreen.cpp \
    ../../src/LogDisplayWindow.cpp \
    ../../src/logger.cpp \
    ../../src/logmonitor.cpp \
    ../../src/log_parser.cpp \
    ../../src/main.cpp \
    ../../src/MainWindow.cpp \
    ../../src/SettingsWindow.cpp \
    ../../src/SoundPlayer.cpp \
    ../../src/ThemeManager.cpp \
    ../../src/ThemeSelect.cpp \
    ../../src/Transmitter.cpp

HEADERS += \
    ../../includes/checkversion.h \
    ../../includes/FilterUtils.h \
    ../../includes/GameLauncher.h \
    ../../includes/globals.h \
    ../../includes/LanguageSelect.h \
    ../../includes/language_manager.h \
    ../../includes/LoadingScreen.h \
    ../../includes/LogDisplayWindow.h \
    ../../includes/logger.h \
    ../../includes/logmonitor.h \
    ../../includes/log_parser.h \
    ../../includes/MainWindow.h \
    ../../includes/qt_logger.h \
    ../../includes/SettingsWindow.h \
    ../../includes/SoundPlayer.h \
    ../../includes/ThemeManager.h \
    ../../includes/ThemeSelect.h \
    ../../includes/Transmitter.h \
    ../../includes/window_utils.h

# Define translation files
TRANSLATIONS += \
    lang_de.ts \
    lang_en.ts \
    lang_es.ts \
    lang_fr.ts \
    lang_it.ts \
    lang_ja.ts \
    lang_ko.ts \
    lang_pl.ts \
    lang_pt.ts \
    lang_ru.ts \
    lang_uk.ts \
    lang_zh.ts