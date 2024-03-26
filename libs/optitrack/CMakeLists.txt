cmake_minimum_required(VERSION 3.5)

project(OPTITRACK VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)

# find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS SerialPort)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Network)

add_library(
    optitrack STATIC
    include/optitrack.hpp
    src/optitrack.cpp
    )
target_link_libraries(optitrack PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)
# target_link_libraries(optitrack PRIVATE Qt${QT_VERSION_MAJOR}::SerialPort)
target_link_libraries(optitrack PRIVATE Qt${QT_VERSION_MAJOR}::Network)
target_include_directories(optitrack PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src") 
target_include_directories(optitrack PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
