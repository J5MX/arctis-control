cmake_minimum_required(VERSION 3.21)
project(arctis-control VERSION 0.6.0 LANGUAGES CXX)

find_package(Qt6 6.8 REQUIRED COMPONENTS DBus Gui Network Qml Quick QuickControls2 Widgets LinguistTools)
qt_standard_project_setup(REQUIRES 6.8)
# build/compile_commands.json lets clangd (editors, code intelligence) understand the code.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Only for security tests: lets a test build run a fake headsetcontrol.
# Release packages always use the system binary.
set(ARCTIS_HEADSETCONTROL "/usr/bin/headsetcontrol" CACHE FILEPATH "headsetcontrol executable (tests only)")
set(ARCTIS_PACTL "/usr/bin/pactl" CACHE FILEPATH "pactl executable (tests only)")

qt_add_executable(arctis-control src/main.cpp)

set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(arctis-control
    URI ArctisControl
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/Theme.qml
        qml/Card.qml
        qml/CutButton.qml
        qml/BatteryRing.qml
        qml/EqBand.qml
        qml/Toggle.qml
        qml/SettingSwitch.qml
        qml/SettingSlider.qml
        qml/SettingChoice.qml
        qml/AppSwitch.qml
        qml/AppChoice.qml
    SOURCES
        src/headsetcontroller.h
        src/headsetcontroller.cpp
        src/headsetmic.h
        src/headsetmic.cpp
        src/appcontroller.h
        src/appcontroller.cpp
        src/batteryalerts.h
    RESOURCES
        assets/icon.svg
        assets/headset.svg
)

target_include_directories(arctis-control PRIVATE src)
target_compile_definitions(arctis-control PRIVATE APP_VERSION="${PROJECT_VERSION}"
                                                   HEADSETCONTROL_PATH="${ARCTIS_HEADSETCONTROL}"
                                                   PACTL_PATH="${ARCTIS_PACTL}")
target_compile_options(arctis-control PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(arctis-control PRIVATE Qt6::DBus Qt6::Gui Qt6::Network Qt6::Qml Qt6::Quick Qt6::QuickControls2
                                             Qt6::Widgets)
# No RUNPATH ($ORIGIN): the binary only loads system libraries.
set_target_properties(arctis-control PROPERTIES INSTALL_RPATH "")

# UI translations (source strings in English), embedded under :/i18n.
qt_add_translations(arctis-control
    TS_FILES i18n/arctis-control_es.ts
    SOURCE_TARGETS arctis-control
    LUPDATE_OPTIONS -no-obsolete
)

# Unit tests (ctest).
include(CTest)
if(BUILD_TESTING)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    qt_add_executable(test_batteryalerts tests/unit/test_batteryalerts.cpp)
    target_include_directories(test_batteryalerts PRIVATE src)
    target_link_libraries(test_batteryalerts PRIVATE Qt6::Test)
    add_test(NAME battery_alerts COMMAND test_batteryalerts)
endif()

include(GNUInstallDirs)
install(TARGETS arctis-control RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES packaging/arctis-control.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES assets/icon.svg
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps
        RENAME arctis-control.svg)
install(FILES LICENSE DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/arctis-control)
