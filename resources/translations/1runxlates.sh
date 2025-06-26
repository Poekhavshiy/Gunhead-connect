#!/bin/bash
QT_BIN=/c/Dev/Qt/qt-install-dynamic/bin

# Define translation files
TRANSLATION_FILES=(
    "resources/translations/lang_de.ts"
    "resources/translations/lang_en.ts"
    "resources/translations/lang_es.ts"
    "resources/translations/lang_fr.ts"
    "resources/translations/lang_it.ts"
    "resources/translations/lang_ja.ts"
    "resources/translations/lang_ko.ts"
    "resources/translations/lang_pl.ts"
    "resources/translations/lang_pt.ts"
    "resources/translations/lang_ru.ts"
    "resources/translations/lang_uk.ts"
    "resources/translations/lang_zh.ts"
)

# Check command line argument
if [ "$1" == "update" ]; then
    # Update translation files
    for ts_file in "${TRANSLATION_FILES[@]}"; do
        $QT_BIN/lupdate . -ts "$ts_file"
    done
    echo "Translation files updated."
    
elif [ "$1" == "build" ]; then
    # Build .qm files from .ts files
    for ts_file in "${TRANSLATION_FILES[@]}"; do
        # Extract the base filename without path for output
        qm_file="${ts_file%.ts}.qm"
        $QT_BIN/lrelease "$ts_file" -qm "$qm_file"
        echo "Built: $qm_file"
    done
    echo "Translation files compiled to .qm format."
    
else
    # Show usage
    echo "Usage: $0 [update|build]"
    echo "  update - Update .ts translation files from source code"
    echo "  build  - Build .qm files from .ts files"
    exit 1
fi