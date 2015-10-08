project(nm-tray)
cmake_minimum_required(VERSION 3.1.0 FATAL_ERROR)
set(QT_MIN_VERSION "5.4.0")

set(CMAKE_AUTOMOC on)
set(CMAKE_AUTOUIC on)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")

find_package(Qt5 ${QT_MIN_VERSION} REQUIRED COMPONENTS
    Widgets
    Gui
    Network
    DBus
)
find_package(KF5NetworkManagerQt REQUIRED)

add_definitions(-DQT_NO_SIGNALS_SLOTS_KEYWORDS)

set(SRCS
    src/icons.cpp
    src/nmmodel.cpp
    src/nmlist.cpp
    src/tray.cpp
    src/main.cpp
)

add_executable(nm-tray
    ${SRCS}
)

set_property(TARGET nm-tray PROPERTY CXX_STANDARD 11)
set_property(TARGET nm-tray PROPERTY CXX_STANDARD_REQUIRED on)

target_link_libraries(nm-tray
    Qt5::Widgets
    Qt5::Gui
    KF5::NetworkManagerQt
)

if (WITH_MODEMMANAGER_SUPPORT)
    target_link_libraries(nm-tray KF5::ModemManagerQt)
endif()

install(TARGETS nm-tray RUNTIME DESTINATION "${CMAKE_INSTALL_PREFIX}/bin")
