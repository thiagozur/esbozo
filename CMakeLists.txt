cmake_minimum_required(VERSION 3.16)
project(Esbozo VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_PREFIX_PATH "E:/E/Qt/6.12.0/msvc2022_64")

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(SOURCES
    src/main.cpp
    src/ui/BlockWidgets.cpp
    src/ui/Dialogs.cpp
    src/ui/EsbozoWindow.cpp
    src/ui/Wizard.cpp
    src/io/MarkdownExporter.cpp
    src/io/ProjectFile.cpp
)

set(HEADERS
    src/core/Block.hpp
    src/core/TitleBlock.hpp
    src/core/SubtitleBlock.hpp
    src/core/ParagraphBlock.hpp
    src/core/ImageBlock.hpp
    src/core/GraphBlock.hpp
    src/core/EquationBlock.hpp
    src/core/TableBlock.hpp
    src/core/GraphRenderer.hpp
    src/core/EquationRenderer.hpp
    src/core/TableRenderer.hpp
    src/core/Document.hpp
    src/core/InlineMarkup.hpp
    src/core/CommandProcessor.hpp
    src/core/PdfExporter.hpp
    src/ui/Theme.hpp
    src/ui/BlockWidgets.hpp
    src/ui/Dialogs.hpp
    src/ui/EsbozoWindow.hpp
    src/ui/Wizard.hpp
    src/io/GraphCsv.hpp
    src/io/MarkdownExporter.hpp
    src/io/ProjectFile.hpp
)

add_executable(Esbozo
    ${SOURCES}
    ${HEADERS}
)

target_include_directories(Esbozo PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(Esbozo PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)
