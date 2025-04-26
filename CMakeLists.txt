cmake_minimum_required(VERSION 3.16)
project(KillApiConnect LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_executable(KillApi-Connect-Plus
    main.cpp
    MainWindow.cpp
    MainWindow.h
    assets.qrc
)

enable_testing()

add_executable(math_tests tests/test_math.cpp)
target_link_libraries(math_tests Qt6::Test)

add_test(NAME MathTests COMMAND math_tests)

target_link_libraries(KillApi-Connect-Plus PRIVATE Qt6::Widgets)