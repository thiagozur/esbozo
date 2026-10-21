cmake_minimum_required(VERSION 3.16)
project(Esbozo VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_PREFIX_PATH "E:/E/Qt/6.12.0/msvc2022_64")

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

add_executable(Esbozo
    src/main.cpp
)

target_link_libraries(Esbozo PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)