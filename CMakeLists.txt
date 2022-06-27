project(nm-tray)

cmake_minimum_required(VERSION 3.1.0 FATAL_ERROR)
cmake_policy(SET CMP0071 NEW)

set(NM_TRAY_VERSION "0.4.2")

set(QT_MIN_VERSION "5.4.0")
set(KF5_MIN_VERSION "5.36.0")

set(CMAKE_AUTOMOC on)
set(CMAKE_AUTOUIC on)
set(CMAKE_AUTORCC on)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")

if (BUILD_WITH_QT6)
    find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS
        Widgets
        Gui
        Network
        DBus

        LinguistTools
    )
else ()
    find_package(Qt5 ${QT_MIN_VERSION} REQUIRED COMPONENTS
        Widgets
        Gui
        Network
        DBus

        LinguistTools
    )
endif ()
find_package(KF5NetworkManagerQt ${KF5_MIN_VERSION} REQUIRED)
include(GNUInstallDirs)

set(TRANSLATION_DIR "${CMAKE_INSTALL_FULL_DATADIR}/${PROJECT_NAME}")
add_definitions(-DQT_NO_SIGNALS_SLOTS_KEYWORDS "-DTRANSLATION_DIR=\"${TRANSLATION_DIR}\"" "-DNM_TRAY_VERSION=\"${NM_TRAY_VERSION}\"")
if (NOT NM_TRAY_XDG_AUTOSTART_DIR)
    #Note: we need the default to be /etc... (not the ${CMAKE_INSTALL_PREFIX}/etc)
    set(NM_TRAY_XDG_AUTOSTART_DIR "${CMAKE_INSTALL_SYSCONFDIR}/xdg/autostart")
endif ()

message(STATUS "Translations destination dir: ${TRANSLATION_DIR}")
message(STATUS "Autostart .desktop entry destination dir: ${NM_TRAY_XDG_AUTOSTART_DIR}\n   (can be overriden by setting the NM_TRAY_XDG_AUTOSTART_DIR cmake variable)")

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

set(RESOURCES
    resources/icons.qrc
)

file(GLOB TSS "translations/${PROJECT_NAME}_*.ts")
if (UPDATE_TRANSLATIONS)
    message(WARNING "!! Disable updating translation after make (-DUPDATE_TRANSLATIONS=no) to avoid 'make clean' delete them !!")
    if (BUILD_WITH_QT6)
        qt6_create_translation(QMS "translations/${PROJECT_NAME}.ts" ${TSS} "src")
    else ()
        qt5_create_translation(QMS "translations/${PROJECT_NAME}.ts" ${TSS} "src")
    endif()
else ()
    if (BUILD_WITH_QT6)
        qt6_add_translation(QMS ${TSS})
    else ()
        qt5_add_translation(QMS ${TSS})
    endif()
endif()

qt6_add_dbus_interface(SRCS
    dbus/org.freedesktop.Notifications.xml
    dbus/org.freedesktop.Notifications
)

add_executable(nm-tray
    ${SRCS}
    ${QMS}
    ${RESOURCES}
)

set_property(TARGET nm-tray PROPERTY CXX_STANDARD 11)
set_property(TARGET nm-tray PROPERTY CXX_STANDARD_REQUIRED on)

if (BUILD_WITH_QT6)
    target_link_libraries(nm-tray
        Qt6::Widgets
        Qt6::Gui
        KF5::NetworkManagerQt
    )
else ()
    target_link_libraries(nm-tray
        Qt5::Widgets
        Qt5::Gui
        KF5::NetworkManagerQt
    )
endif()

if (WITH_MODEMMANAGER_SUPPORT)
    if (BUILD_WITH_QT6)
        find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS Xml)
    else ()
        find_package(Qt5 ${QT_MIN_VERSION} REQUIRED COMPONENTS Xml)
    endif()
    find_package(KF5ModemManagerQt ${KF5_MIN_VERSION} REQUIRED)
    target_link_libraries(nm-tray KF5::ModemManagerQt)
endif()

install(TARGETS nm-tray RUNTIME DESTINATION "${CMAKE_INSTALL_FULL_BINDIR}" COMPONENT runtime)
install(FILES ${QMS} DESTINATION "${TRANSLATION_DIR}" COMPONENT translations)
install(FILES "resources/nm-tray.desktop" DESTINATION "${CMAKE_INSTALL_FULL_DATAROOTDIR}/applications" COMPONENT data)
install(FILES "resources/nm-tray-autostart.desktop" DESTINATION "${NM_TRAY_XDG_AUTOSTART_DIR}" COMPONENT data)
install(FILES "resources/nm-tray.conf" DESTINATION "${CMAKE_INSTALL_FULL_DATADIR}/${PROJECT_NAME}" COMPONENT data)
