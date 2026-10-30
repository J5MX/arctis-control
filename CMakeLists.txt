cmake_minimum_required(VERSION 3.21)
project(nova5-panel VERSION 0.2.1 LANGUAGES CXX)

find_package(Qt6 6.8 REQUIRED COMPONENTS Gui Network Qml Quick QuickControls2)
qt_standard_project_setup(REQUIRES 6.8)

qt_add_executable(nova5-panel src/main.cpp)

set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(nova5-panel
    URI Nova5Panel
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/Theme.qml
        qml/Card.qml
        qml/CutButton.qml
        qml/BatteryRing.qml
        qml/EqBand.qml
        qml/Toggle.qml
    SOURCES
        src/headsetcontroller.h
        src/headsetcontroller.cpp
    RESOURCES
        assets/icon.svg
        assets/headset.svg
)

target_include_directories(nova5-panel PRIVATE src)
target_compile_definitions(nova5-panel PRIVATE NOVA5_VERSION="${PROJECT_VERSION}")
target_compile_options(nova5-panel PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(nova5-panel PRIVATE Qt6::Gui Qt6::Network Qt6::Qml Qt6::Quick Qt6::QuickControls2)
# Sin RUNPATH ($ORIGIN): el binario solo carga bibliotecas del sistema.
set_target_properties(nova5-panel PROPERTIES INSTALL_RPATH "")

include(GNUInstallDirs)
install(TARGETS nova5-panel RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES packaging/nova5-panel.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES assets/icon.svg
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps
        RENAME nova5-panel.svg)
