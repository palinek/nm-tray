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

    LinguistTools
)
find_package(KF5NetworkManagerQt REQUIRED)
include(GNUInstallDirs)

set(TRANSLATION_DIR "${CMAKE_INSTALL_FULL_DATADIR}/${PROJECT_NAME}")
add_definitions(-DQT_NO_SIGNALS_SLOTS_KEYWORDS "-DTRANSLATION_DIR=\"${TRANSLATION_DIR}\"")

#workaround for binary KF5NetworkManagerQt package built with different version of NetworkManager as present on current machine
if (DEFINED BIN_NM_VERSION)
    string(REPLACE "." ";" BIN_NM_VERSION_LIST "${BIN_NM_VERSION}")
    list(GET BIN_NM_VERSION_LIST 0 BIN_NM_MAJOR)
    list(GET BIN_NM_VERSION_LIST 1 BIN_NM_MINOR)
    list(GET BIN_NM_VERSION_LIST 2 BIN_NM_PATCH)
else ()
    set(BIN_NM_MAJOR 999)
    set(BIN_NM_MINOR 0)
    set(BIN_NM_PATCH 0)
endif ()
message(STATUS "Building as with NM_VERSION='${BIN_NM_MAJOR}.${BIN_NM_MINOR}.${BIN_NM_PATCH}'...")
add_definitions(-DBIN_NM_MAJOR=${BIN_NM_MAJOR} -DBIN_NM_MINOR=${BIN_NM_MINOR} -DBIN_NM_PATCH=${BIN_NM_PATCH})


set(SRCS
    src/log.cpp
    src/icons.cpp
    src/nmmodel.cpp
    src/nmproxy.cpp
    src/connectioninfo.cpp
    src/nmlist.cpp
    src/menuview.cpp
    src/windowmenu.cpp
    src/tray.cpp
    src/translate.cpp
    src/main.cpp
)


file(GLOB TSS "translations/${PROJECT_NAME}_*.ts")
if (UPDATE_TRANSLATIONS)
    message(WARNING "!! Disable updating translation after make (-DUPDATE_TRANSLATIONS=no) to avoid 'make clean' delete them !!")
    qt5_create_translation(QMS ${TSS} "src")
else ()
    qt5_add_translation(QMS ${TSS})
endif()

add_executable(nm-tray
    ${SRCS}
    ${QMS}
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

install(TARGETS nm-tray RUNTIME DESTINATION "${CMAKE_INSTALL_FULL_BINDIR}" COMPONENT runtime)
install(FILES ${QMS} DESTINATION "${TRANSLATION_DIR}" COMPONENT translations)
