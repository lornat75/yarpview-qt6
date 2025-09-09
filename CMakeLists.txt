cmake_minimum_required(VERSION 3.16)
project(yarpview_qt6 VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(YARP REQUIRED COMPONENTS os sig dev)
find_package(Qt6 REQUIRED COMPONENTS Widgets Gui)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

add_executable(yarpview-qt6
    src/main.cpp
    src/Options.h
    src/Options.cpp
    src/ImageReceiver.h
    src/ImageReceiver.cpp
    src/ImageWidget.h
    src/ImageWidget.cpp
    src/MainWindow.h
    src/MainWindow.cpp
)

target_link_libraries(yarpview-qt6
    PRIVATE
        YARP::YARP_os
        YARP::YARP_sig
        YARP::YARP_dev
        YARP::YARP_init
        Qt6::Widgets
        Qt6::Gui
)

install(TARGETS yarpview-qt6 RUNTIME DESTINATION bin)
