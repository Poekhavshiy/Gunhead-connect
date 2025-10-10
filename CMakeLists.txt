cmake_minimum_required(VERSION 3.16)
project(Gunhead-Connect VERSION 0.1.10  LANGUAGES CXX)
set(PROJECT_AUTHOR "Gunhead Team")
set(PROJECT_ORGANIZATION "Gunhead Team")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Persist FetchContent downloads across builds/presets unless overridden
if(NOT DEFINED FETCHCONTENT_BASE_DIR)
    set(FETCHCONTENT_BASE_DIR "${CMAKE_SOURCE_DIR}/.fc_cache" CACHE PATH "Base directory for FetchContent caches")
endif()

# Allow opting out of tests (and their dependencies) entirely
option(ENABLE_TESTS "Build unit tests" OFF)

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
    ${SRC_DIR}/SoundPlayer.cpp
    ${SRC_DIR}/LoadingScreen.cpp
    ${SRC_DIR}/GameLauncher.cpp
    ${SRC_DIR}/ThemeManager.cpp
    ${SRC_DIR}/CustomTitleBar.cpp
    ${SRC_DIR}/ResizeHelper.cpp
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
    ${INC_DIR}/logger.h
    ${INC_DIR}/globals.h
    ${INC_DIR}/SoundPlayer.h
    ${INC_DIR}/LoadingScreen.h
    ${INC_DIR}/GameLauncher.h
    ${INC_DIR}/ThemeManager.h
    ${INC_DIR}/CustomTitleBar.h
    ${INC_DIR}/ResizeHelper.h
)

set(UIS
    ${UI_DIR}/mainwindow.ui
)

set(QRC
    ${RES_DIR}/resources.qrc
)

# Qt setup
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network LinguistTools Multimedia MultimediaWidgets Concurrent)

qt_add_executable(Gunhead-Connect
    WIN32
    ${SOURCES} ${HEADERS} ${UIS} ${QRC}
)

# Add this after the project() line:
configure_file(
    ${INC_DIR}/version.h.in
    ${CMAKE_BINARY_DIR}/generated/version.h
    @ONLY
)

# Add the generated directory to include paths
target_include_directories(Gunhead-Connect PRIVATE 
    ${INC_DIR}
    ${CMAKE_BINARY_DIR}/generated
)

target_include_directories(Gunhead-Connect PRIVATE ${INC_DIR})

target_link_libraries(Gunhead-Connect PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    Qt6::Multimedia
    Qt6::Concurrent
    dwmapi
    winmm
)

# Add proper deployment of DLLs
if(WIN32)
  add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      $<TARGET_FILE:Qt6::Multimedia>
      $<TARGET_FILE:Qt6::MultimediaWidgets>
      $<TARGET_FILE_DIR:${PROJECT_NAME}>)
endif()

## Dependency fetching policy and verbosity
option(FETCH_DEPS "Allow CMake to fetch dependencies via FetchContent" ON)
set(FETCHCONTENT_QUIET OFF)
# Allow users to force offline behavior via cache or environment variable
if(NOT DEFINED FETCHCONTENT_FULLY_DISCONNECTED AND DEFINED ENV{FETCHCONTENT_FULLY_DISCONNECTED})
    set(FETCHCONTENT_FULLY_DISCONNECTED $ENV{FETCHCONTENT_FULLY_DISCONNECTED})
endif()

## JSON library (prefer system, fallback to FetchContent)
find_package(nlohmann_json QUIET CONFIG)
if(NOT nlohmann_json_FOUND)
    if(NOT FETCH_DEPS)
        message(FATAL_ERROR "nlohmann_json not found and FETCH_DEPS=OFF. Install system package (MSYS2: 'pacman -S mingw-w64-x86_64-nlohmann-json') or enable FETCH_DEPS.")
    endif()
        message(STATUS "nlohmann_json not found via find_package, using FetchContent (URL tarball)")
    include(FetchContent)
        # Tarball URL is more reliable than git clone; make URL and HASH configurable
        set(JSON_URL "https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz" CACHE STRING "nlohmann_json archive URL")
        # Use the hash observed in your environment to pass integrity check; can be overridden
        set(JSON_URL_HASH "SHA256=42f6e95cad6ec532fd372391373363b62a14af6d771056dbfc86160e6dfff7aa" CACHE STRING "Expected hash for JSON_URL; leave empty to skip")
        set(JSON_FETCH_ARGS
            URL ${JSON_URL}
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        )
        if(JSON_URL_HASH)
            list(APPEND JSON_FETCH_ARGS URL_HASH ${JSON_URL_HASH})
        endif()
        FetchContent_Declare(json ${JSON_FETCH_ARGS})
    set(FETCHCONTENT_UPDATES_DISCONNECTED ON)
    FetchContent_MakeAvailable(json)
endif()
target_link_libraries(Gunhead-Connect PRIVATE nlohmann_json::nlohmann_json)

# Translations
set(TS_FILES
    ${TRANSLATIONS_DIR}/lang_en.ts
    ${TRANSLATIONS_DIR}/lang_zh.ts
    ${TRANSLATIONS_DIR}/lang_ja.ts
    ${TRANSLATIONS_DIR}/lang_uk.ts
    ${TRANSLATIONS_DIR}/lang_ru.ts
    ${TRANSLATIONS_DIR}/lang_de.ts
    ${TRANSLATIONS_DIR}/lang_es.ts
    ${TRANSLATIONS_DIR}/lang_fr.ts
    ${TRANSLATIONS_DIR}/lang_it.ts
    ${TRANSLATIONS_DIR}/lang_pl.ts
    ${TRANSLATIONS_DIR}/lang_pt.ts
    ${TRANSLATIONS_DIR}/lang_ko.ts
)
qt_add_translations(Gunhead-Connect TS_FILES ${TS_FILES})

# Tests - only include when enabled and not in Release mode
if(ENABLE_TESTS AND NOT CMAKE_BUILD_TYPE STREQUAL "Release")
    # Prefer a local Catch2 install if available; else FetchContent
    find_package(Catch2 3 QUIET CONFIG)
        if(NOT Catch2_FOUND)
        if(NOT FETCH_DEPS)
            message(FATAL_ERROR "Catch2 not found and FETCH_DEPS=OFF. Install system package (MSYS2: 'pacman -S mingw-w64-x86_64-catch2') or enable FETCH_DEPS.")
        endif()
            message(STATUS "Catch2 not found via find_package, using FetchContent (URL tarball)")
        include(FetchContent)
            set(CATCH2_URL "https://github.com/catchorg/Catch2/archive/refs/tags/v3.11.0.tar.gz" CACHE STRING "Catch2 archive URL")
            set(CATCH2_URL_HASH "SHA256=82fa1cb59dc28bab220935923f7469b997b259eb192fb9355db62da03c2a3137" CACHE STRING "Expected hash for CATCH2_URL; leave empty to skip")
            set(CATCH2_FETCH_ARGS
                URL ${CATCH2_URL}
                DOWNLOAD_EXTRACT_TIMESTAMP TRUE
            )
            if(CATCH2_URL_HASH)
                list(APPEND CATCH2_FETCH_ARGS URL_HASH ${CATCH2_URL_HASH})
            endif()
            FetchContent_Declare(Catch2 ${CATCH2_FETCH_ARGS})
        set(FETCHCONTENT_UPDATES_DISCONNECTED ON)
        FetchContent_MakeAvailable(Catch2)
    endif()
  enable_testing()

  add_executable(tests
      tests/test_log_parser.cpp
      ${SRC_DIR}/log_parser.cpp
      ${SRC_DIR}/logger.cpp
      ${SRC_DIR}/globals.cpp
  )

  # Add the generated directory to the tests include paths
  target_include_directories(tests PRIVATE 
      ${SRC_DIR} 
      ${INC_DIR} 
      ${CMAKE_SOURCE_DIR}/tests
      ${CMAKE_BINARY_DIR}/generated
  )

    target_link_libraries(tests PRIVATE Catch2::Catch2WithMain nlohmann_json::nlohmann_json)
  include(Catch)
  catch_discover_tests(tests WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endif()

# Install binary
install(TARGETS Gunhead-Connect RUNTIME DESTINATION bin)

# Include the user-specific configuration file first
if(EXISTS "${CMAKE_SOURCE_DIR}/.vscode/cmake_config.cmake")
    include("${CMAKE_SOURCE_DIR}/.vscode/cmake_config.cmake")
else()
    message(FATAL_ERROR "Configuration file not found: .vscode/cmake_config.cmake")
endif()

# Windows DLL copying
if(WIN32)
    # Set the absolute path to the icon file
    set(ICON_ABSOLUTE_PATH "${CMAKE_SOURCE_DIR}/resources/icons/Gunhead.ico")
    
    # Configure the RC file
    configure_file(
        ${CMAKE_SOURCE_DIR}/resources/appicon.rc.in
        ${CMAKE_BINARY_DIR}/generated/appicon.rc
        @ONLY
    )
    
    # Use the generated RC file
    set(APP_ICON_RESOURCE "${CMAKE_BINARY_DIR}/generated/appicon.rc")
    set_source_files_properties(${APP_ICON_RESOURCE} PROPERTIES LANGUAGE RC)
    target_sources(Gunhead-Connect PRIVATE ${APP_ICON_RESOURCE})

    set(APP_DEST ${OUTDIR_WINDOWS})
    set(APP_DLL_DEST ${OUTDIR_WINDOWS})
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
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        add_custom_command(TARGET Gunhead-Connect POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${MINGW_DLLS} ${QT_DLLS} ${APP_DLL_DEST}
            COMMENT "Copying DLLs for Release build"
        )
    else()
        # Always add for main app
        add_custom_command(TARGET Gunhead-Connect POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${MINGW_DLLS} ${QT_DLLS} ${APP_DLL_DEST}
            COMMENT "Copying DLLs for Gunhead-Connect"
        )
        # Add for tests only if the target exists
        if(TARGET tests)
            add_custom_command(TARGET tests POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different ${MINGW_DLLS} ${QT_DLLS} ${APP_DLL_DEST}
                COMMENT "Copying DLLs for tests"
            )
        endif()
    endif()

    add_custom_command(TARGET Gunhead-Connect POST_BUILD
        # QT Plugins
        COMMAND ${CMAKE_COMMAND} -E make_directory ${APP_DLL_DEST}/platforms
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${QT_PLUGIN_PATH}/platforms/qwindows.dll" ${APP_DLL_DEST}/platforms
        COMMAND ${CMAKE_COMMAND} -E make_directory ${APP_DLL_DEST}/tls
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${QT_PLUGIN_PATH}/tls/qopensslbackend.dll" ${APP_DLL_DEST}/tls
        COMMAND ${CMAKE_COMMAND} -E make_directory ${APP_DLL_DEST}/multimedia
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${QT_PLUGIN_PATH}/multimedia/windowsmediaplugin.dll" ${APP_DLL_DEST}/multimedia
        #other files
        COMMAND ${CMAKE_COMMAND} -E make_directory ${APP_DEST}/data
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}/data/logfile_regex_rules.json ${APP_DEST}/data/logfile_regex_rules.json
        COMMAND ${CMAKE_COMMAND} -E make_directory ${APP_DEST}/sounds
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}/buildfiles/sounds/womp-womp.mp3 ${APP_DEST}/sounds/womp-womp.mp3
    )
endif()
