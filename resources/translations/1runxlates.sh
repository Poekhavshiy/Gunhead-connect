#!/bin/bash
QT_BIN=/c/Dev/Qt/qt-install-dynamic/bin

if [ "$1" == "update" ]; then
    # Update translation files using the project file
    $QT_BIN/lupdate translations.pro
    echo "Translation files updated."
    
elif [ "$1" == "build" ]; then
    # Build all .qm files directly from the project file
    $QT_BIN/lrelease translations.pro
    echo "Translation files compiled to .qm format."
    
else
    # Show usage
    echo "Usage: $0 [update|build]"
    echo "  update - Update .ts translation files from source code"
    echo "  build  - Build .qm files from .ts files"
    exit 1
fi