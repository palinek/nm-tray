project(nm-tray)
cmake_minimum_required(VERSION 3.1.0 FATAL_ERROR)

set(NM_TRAY_VERSION "0.1.0")

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
find_package(KF5NetworkManagerQt "5.24.0" REQUIRED)
include(GNUInstallDirs)

set(TRANSLATION_DIR "${CMAKE_INSTALL_FULL_DATADIR}/${PROJECT_NAME}")
add_definitions(-DQT_NO_SIGNALS_SLOTS_KEYWORDS "-DTRANSLATION_DIR=\"${TRANSLATION_DIR}\"" "-DNM_TRAY_VERSION=\"${NM_TRAY_VERSION}\"")
if (NOT NM_TRAY_XDG_AUTOSTART_DIR)
    #Note: we need the default to be /etc... (not the ${CMAKE_INSTALL_PREFIX}/etc)
    set(NM_TRAY_XDG_AUTOSTART_DIR "/${CMAKE_INSTALL_SYSCONFDIR}/xdg/autostart")
endif ()

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
install(FILES "resources/nm-tray.desktop" DESTINATION "${CMAKE_INSTALL_FULL_DATAROOTDIR}/applications" COMPONENT data)
install(FILES "resources/nm-tray-autostart.desktop" DESTINATION "${NM_TRAY_XDG_AUTOSTART_DIR}" COMPONENT data)
