cmake_minimum_required(VERSION 3.16)
project(storyspire-qt VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Network Sql)

qt_standard_project_setup()

qt_add_executable(storyspire-qt
    src/main.cpp
    src/MainWindow.cpp
    src/MainWindow.h
    src/BookTree.cpp
    src/BookTree.h
    src/Editor.cpp
    src/Editor.h
    src/InspirationPanel.cpp
    src/InspirationPanel.h
    src/FlareClient.cpp
    src/FlareClient.h
    src/AiPanel.cpp
    src/AiPanel.h
    src/Exporter.cpp
    src/Exporter.h
    src/OutlinePanel.cpp
    src/OutlinePanel.h
)

target_link_libraries(storyspire-qt PRIVATE Qt6::Widgets Qt6::Network Qt6::Sql)

target_compile_definitions(storyspire-qt PRIVATE APP_VERSION="${PROJECT_VERSION}")
