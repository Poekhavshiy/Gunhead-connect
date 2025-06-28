TEMPLATE = app
QT += core gui widgets network multimedia
CONFIG += c++17

# Define sources to be scanned
SOURCES += src/*.cpp
HEADERS += includes/*.h

# Define translation files
TRANSLATIONS += \
    resources/translations/lang_de.ts \
    resources/translations/lang_en.ts \
    resources/translations/lang_es.ts \
    resources/translations/lang_fr.ts \
    resources/translations/lang_it.ts \
    resources/translations/lang_ja.ts \
    resources/translations/lang_ko.ts \
    resources/translations/lang_pl.ts \
    resources/translations/lang_pt.ts \
    resources/translations/lang_ru.ts \
    resources/translations/lang_uk.ts \
    resources/translations/lang_zh.ts