cmake_minimum_required(VERSION 3.21)
project(arctis-control VERSION 0.3.0 LANGUAGES CXX)

find_package(Qt6 6.8 REQUIRED COMPONENTS Gui Network Qml Quick QuickControls2)
qt_standard_project_setup(REQUIRES 6.8)

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
    SOURCES
        src/headsetcontroller.h
        src/headsetcontroller.cpp
    RESOURCES
        assets/icon.svg
        assets/headset.svg
)

target_include_directories(arctis-control PRIVATE src)
target_compile_definitions(arctis-control PRIVATE APP_VERSION="${PROJECT_VERSION}")
target_compile_options(arctis-control PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(arctis-control PRIVATE Qt6::Gui Qt6::Network Qt6::Qml Qt6::Quick Qt6::QuickControls2)
# Sin RUNPATH ($ORIGIN): el binario solo carga bibliotecas del sistema.
set_target_properties(arctis-control PROPERTIES INSTALL_RPATH "")

include(GNUInstallDirs)
install(TARGETS arctis-control RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES packaging/arctis-control.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES assets/icon.svg
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps
        RENAME arctis-control.svg)
install(FILES LICENSE DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/arctis-control)
