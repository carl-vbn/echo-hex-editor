cmake_minimum_required(VERSION 3.16)

project(echohexedit VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_executable(echohexedit
    echohexedit.qrc

    src/main.cpp

    src/core/document.cpp
    src/core/document.h

    src/core/node.cpp
    src/core/node.h
    src/core/nodemodel.cpp
    src/core/nodemodel.h
    src/core/nodeutil.cpp
    src/core/nodeutil.h

    src/theme/theme.cpp
    src/theme/theme.h

    src/ui/mainwindow.cpp
    src/ui/mainwindow.h
    src/ui/titlebar.cpp
    src/ui/titlebar.h
    src/ui/toolbar.cpp
    src/ui/toolbar.h

    src/ui/hexview.cpp
    src/ui/hexview.h
    src/ui/parsedview.cpp
    src/ui/parsedview.h
    src/ui/messagebox.cpp
    src/ui/messagebox.h

    src/ui/panels/leftpanel.cpp
    src/ui/panels/leftpanel.h
    src/ui/panels/centerpanel.cpp
    src/ui/panels/centerpanel.h
    src/ui/panels/rightpanel.cpp
    src/ui/panels/rightpanel.h

    src/ext/parser.h
    src/ext/parser.cpp
    src/ext/impl/bmp/parser.h
    src/ext/impl/bmp/parser.cpp
)

target_link_libraries(echohexedit PRIVATE Qt6::Widgets)
target_include_directories(echohexedit PRIVATE src)
