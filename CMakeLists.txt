cmake_minimum_required(VERSION 3.16)
project(KillApiConnect LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_executable(KillApiConnect
    main.cpp
    MainWindow.cpp
    MainWindow.h
    assets.qrc
)

target_link_libraries(KillApiConnect PRIVATE Qt6::Widgets)