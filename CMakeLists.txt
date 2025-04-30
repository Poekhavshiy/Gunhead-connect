cmake_minimum_required(VERSION 3.16)
project(KillApiConnect VERSION 0.1.2  LANGUAGES CXX)
set(PROJECT_AUTHOR "Poekhavshiy")
set(PROJECT_ORGANIZATION "Poekhavshiy")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Qt features
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Output paths
set(OUTDIR_WINDOWS ${CMAKE_BINARY_DIR}/bin/windows)
set(OUTDIR_LINUX   ${CMAKE_BINARY_DIR}/bin/linux)
if(WIN32)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${OUTDIR_WINDOWS})
elseif(UNIX)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${OUTDIR_LINUX})
endif()

# Paths
set(SRC_DIR ${CMAKE_SOURCE_DIR}/src)
set(INC_DIR ${CMAKE_SOURCE_DIR}/includes)
set(RES_DIR ${CMAKE_SOURCE_DIR}/resources)
set(UI_DIR  ${CMAKE_SOURCE_DIR}/ui)
set(TRANSLATIONS_DIR ${RES_DIR}/translations)

# Source files
set(SOURCES
    ${SRC_DIR}/main.cpp
    ${SRC_DIR}/logger.cpp
    ${SRC_DIR}/mainwindow.cpp
    ${SRC_DIR}/logmonitor.cpp
    ${SRC_DIR}/network.cpp
    ${SRC_DIR}/log_parser.cpp
    ${SRC_DIR}/MainWindow.cpp
    ${SRC_DIR}/SettingsWindow.cpp
    ${SRC_DIR}/LogDisplayWindow.cpp
    ${SRC_DIR}/Transmitter.cpp
    ${SRC_DIR}/checkversion.cpp
    ${SRC_DIR}/ThemeSelect.cpp
    ${SRC_DIR}/LanguageSelect.cpp
    ${SRC_DIR}/language_manager.cpp
    ${SRC_DIR}/globals.cpp
)

set(HEADERS
    ${INC_DIR}/ThemeSelect.h
    ${INC_DIR}/language_manager.h
    ${INC_DIR}/LanguageSelect.h
    ${INC_DIR}/checkversion.h
    ${INC_DIR}/Transmitter.h
    ${INC_DIR}/LogDisplayWindow.h
    ${INC_DIR}/SettingsWindow.h
    ${INC_DIR}/MainWindow.h
    ${INC_DIR}/logmonitor.h
    ${INC_DIR}/log_parser.h
    ${INC_DIR}/network.h
    ${INC_DIR}/logger.h
    ${INC_DIR}/globals.h
)

set(UIS
    ${UI_DIR}/mainwindow.ui
)

set(QRC
    ${RES_DIR}/resources.qrc
)

# Qt setup
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network LinguistTools Multimedia)

qt_add_executable(KillApiConnect
    WIN32
    ${SOURCES} ${HEADERS} ${UIS} ${QRC}
)

target_include_directories(KillApiConnect PRIVATE ${INC_DIR})

target_link_libraries(KillApiConnect PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    Qt6::Multimedia
    dwmapi
)

# JSON library
include(FetchContent)
FetchContent_Declare(json
  GIT_REPOSITORY https://github.com/nlohmann/json
  GIT_TAG v3.12.0
)
FetchContent_MakeAvailable(json)
target_link_libraries(KillApiConnect PRIVATE nlohmann_json::nlohmann_json)

# Translations
set(TS_FILES
    ${TRANSLATIONS_DIR}/lang_en.ts
    ${TRANSLATIONS_DIR}/lang_zh_CN.ts
    ${TRANSLATIONS_DIR}/lang_ja.ts
    ${TRANSLATIONS_DIR}/lang_ua.ts
    ${TRANSLATIONS_DIR}/lang_ru.ts
    ${TRANSLATIONS_DIR}/lang_de.ts
    ${TRANSLATIONS_DIR}/lang_es.ts
    ${TRANSLATIONS_DIR}/lang_fr.ts
    ${TRANSLATIONS_DIR}/lang_it.ts
    ${TRANSLATIONS_DIR}/lang_pl.ts
    ${TRANSLATIONS_DIR}/lang_pt.ts
    ${TRANSLATIONS_DIR}/lang_kr.ts
)
qt_add_translations(KillApiConnect TS_FILES ${TS_FILES})

# Tests
FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG v3.8.1
)
FetchContent_MakeAvailable(Catch2)
enable_testing()

add_executable(tests
    tests/test_log_parser.cpp
    ${SRC_DIR}/log_parser.cpp
    ${SRC_DIR}/logger.cpp
    ${SRC_DIR}/globals.cpp
)
target_include_directories(tests PRIVATE ${SRC_DIR} ${INC_DIR} ${CMAKE_SOURCE_DIR}/tests)
target_link_libraries(tests PRIVATE Catch2::Catch2WithMain nlohmann_json::nlohmann_json)
include(Catch)
catch_discover_tests(tests WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

# Install binary
install(TARGETS KillApiConnect RUNTIME DESTINATION bin)

# Windows DLL copying
if(WIN32)
    set(APP_ICON_RESOURCE "${CMAKE_SOURCE_DIR}/resources/appicon.rc")
    set_source_files_properties(${APP_ICON_RESOURCE} PROPERTIES LANGUAGE RC)
    target_sources(KillApiConnect PRIVATE ${APP_ICON_RESOURCE})
    
    set(MINGW_DIR "C:/msys64/mingw64")
    set(QT_PLUGIN_PATH "C:/Dev/Qt/qt-install-dynamic/plugins")
    set(APP_DEST ${OUTDIR_WINDOWS})
    set(MINGW_DLLS
        ${MINGW_DIR}/bin/libgcc_s_seh-1.dll
        ${MINGW_DIR}/bin/libstdc++-6.dll
        ${MINGW_DIR}/bin/libwinpthread-1.dll
        ${MINGW_DIR}/bin/libcrypto-3-x64.dll
        ${MINGW_DIR}/bin/libssl-3-x64.dll
        ${MINGW_DIR}/bin/libbrotlidec.dll
        ${MINGW_DIR}/bin/libbrotlicommon.dll
        ${MINGW_DIR}/bin/libpng16-16.dll
        ${MINGW_DIR}/bin/libfreetype-6.dll
        ${MINGW_DIR}/bin/libharfbuzz-0.dll
    )
    set(QT_DLLS
        ${CMAKE_PREFIX_PATH}/bin/Qt6Core.dll
        ${CMAKE_PREFIX_PATH}/bin/Qt6Network.dll
        ${CMAKE_PREFIX_PATH}/bin/Qt6Widgets.dll
        ${CMAKE_PREFIX_PATH}/bin/Qt6Gui.dll
        ${CMAKE_PREFIX_PATH}/bin/Qt6Multimedia.dll
    )

    # Common copy logic
    foreach(TGT KillApiConnect tests)
        add_custom_command(TARGET ${TGT} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${MINGW_DLLS} ${QT_DLLS} ${APP_DEST}
            COMMENT "Copying DLLs for ${TGT}"
        )
    endforeach()

    add_custom_command(TARGET KillApiConnect POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory ${APP_DEST}/platforms
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${QT_PLUGIN_PATH}/platforms/qwindows.dll" ${APP_DEST}/platforms
        COMMAND ${CMAKE_COMMAND} -E make_directory ${APP_DEST}/tls
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${QT_PLUGIN_PATH}/tls/qopensslbackend.dll" ${APP_DEST}/tls
        COMMAND ${CMAKE_COMMAND} -E make_directory ${APP_DEST}/multimedia
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${QT_PLUGIN_PATH}/multimedia/windowsmediaplugin.dll" ${APP_DEST}/multimedia
        COMMAND ${CMAKE_COMMAND} -E make_directory ${APP_DEST}/data
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}/data/logfile_regex_rules.json ${APP_DEST}/data/logfile_regex_rules.json
    )
endif()
