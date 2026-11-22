cmake_minimum_required(VERSION 3.21)
project(Sysline VERSION 1.0.1 LANGUAGES C CXX)
if(WIN32)
    enable_language(RC)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Network)

set(SYSLINE_SOURCES
    src/main.cpp
    src/app.h src/app.cpp
    src/config.h src/config.cpp
    src/engine.h src/engine.cpp
    src/keys.h src/keys.cpp
    src/i18n.h src/i18n.cpp src/i18n_de.inc
    src/look.h src/look.cpp
    src/mainwindow.h src/mainwindow.cpp
    src/overlay.h src/overlay.cpp
    src/widgets.h src/widgets.cpp
    src/sensors.h src/sensors.cpp
    src/gpulibs.h src/gpulibs.cpp
    src/platform.h src/platform_common.cpp
    src/updater.h src/updater.cpp
)

if(WIN32)
    add_executable(sysline WIN32 ${SYSLINE_SOURCES} src/platform_win.cpp src/sensors_win.cpp assets/app.rc)
    set_target_properties(sysline PROPERTIES OUTPUT_NAME Sysline)
    target_link_libraries(sysline PRIVATE user32 advapi32 shell32 ole32 dxgi pdh iphlpapi ws2_32)
    target_compile_definitions(sysline PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
elseif(APPLE)
    set(SYSLINE_ICNS ${CMAKE_SOURCE_DIR}/assets/icon.icns)
    set_source_files_properties(${SYSLINE_ICNS} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    add_executable(sysline MACOSX_BUNDLE ${SYSLINE_SOURCES} src/platform_mac.cpp src/sensors_mac.cpp ${SYSLINE_ICNS})
    set_target_properties(sysline PROPERTIES
        OUTPUT_NAME Sysline
        MACOSX_BUNDLE_BUNDLE_NAME Sysline
        MACOSX_BUNDLE_GUI_IDENTIFIER win.mert.sysline
        MACOSX_BUNDLE_ICON_FILE icon.icns
        MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
        MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_SOURCE_DIR}/assets/Info.plist.in)
    target_link_libraries(sysline PRIVATE "-framework Carbon" "-framework IOKit" "-framework CoreFoundation" "-framework ApplicationServices")
else()
    add_executable(sysline ${SYSLINE_SOURCES} src/platform_linux.cpp src/sensors_linux.cpp)
    find_package(X11)
    if(X11_FOUND)
        target_compile_definitions(sysline PRIVATE HAVE_X11)
        target_link_libraries(sysline PRIVATE X11::X11)
    endif()
endif()

target_link_libraries(sysline PRIVATE Qt6::Widgets Qt6::Network)
target_compile_definitions(sysline PRIVATE APP_VERSION="${PROJECT_VERSION}")
if(MSVC)
    target_compile_options(sysline PRIVATE /utf-8 /W3 /Zc:__cplusplus)
endif()

if(UNIX AND NOT APPLE)
    include(GNUInstallDirs)
    install(TARGETS sysline RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    install(FILES assets/win.mert.Sysline.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
    install(FILES assets/icon.png DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps RENAME sysline.png)
endif()
