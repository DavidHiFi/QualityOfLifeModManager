cmake_minimum_required(VERSION 3.16)
project(QualityOfLifeModManager LANGUAGES CXX)
# App version lives in src/Version.h (CLL_VERSION).

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Network Concurrent)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Network Concurrent)
find_package(Qt${QT_VERSION_MAJOR} OPTIONAL_COMPONENTS Svg)

set(SOURCES
    src/main.cpp
    src/AppSettings.cpp
    src/Storage.cpp
    src/GameLauncher.cpp
    src/PlutoniumAuth.cpp
    src/LoginDialog.cpp
    src/ConfigMaker.cpp
    src/ArchiveTool.cpp
    src/Downloader.cpp
    src/Dialogs.cpp
    src/Theme.cpp
    src/I18n.cpp
    src/ImportKitDialog.cpp
    src/MakeConfigDialog.cpp
    src/MainWindow.cpp
    src/pages/ModsPage.cpp
    src/pages/ServerPage.cpp
    src/SteamDetector.cpp
    src/SetupWizard.cpp
    src/ProgressDialog.cpp
    src/SmartModInstaller.cpp
    src/CllInstaller.cpp
    src/GithubModInstaller.cpp
    src/ModPreview.cpp
    src/GameCatalog.cpp
    src/AnimatedLogo.cpp
    src/pages/PlayPage.cpp
    src/pages/SettingsPage.cpp
    src/pages/AboutPage.cpp
    src/HomeCatalog.cpp
    src/pages/HomePage.cpp
    src/UpdateService.cpp
    src/QolService.cpp
    src/pages/QolPage.cpp
)

set(HEADERS
    src/AppSettings.h
    src/Storage.h
    src/GameLauncher.h
    src/VersionCompare.h
    src/PlutoniumAuth.h
    src/LoginDialog.h
    src/ConfigMaker.h
    src/ArchiveTool.h
    src/Downloader.h
    src/Dialogs.h
    src/Theme.h
    src/I18n.h
    src/ImportKitDialog.h
    src/MakeConfigDialog.h
    src/MainWindow.h
    src/pages/ModsPage.h
    src/pages/ServerPage.h
    src/SteamDetector.h
    src/SetupWizard.h
    src/ProgressDialog.h
    src/SmartModInstaller.h
    src/CllInstaller.h
    src/GithubModInstaller.h
    src/ModPreview.h
    src/GameCatalog.h
    src/AnimatedLogo.h
    src/pages/PlayPage.h
    src/pages/SettingsPage.h
    src/pages/AboutPage.h
    src/HomeCatalog.h
    src/pages/HomePage.h
    src/UpdateService.h
    src/QolService.h
    src/pages/QolPage.h
)

qt_add_resources(QRC_SOURCES resources/resources.qrc)

add_executable(LanLauncherQt
    ${SOURCES}
    ${HEADERS}
    ${QRC_SOURCES}
)

target_include_directories(LanLauncherQt PRIVATE src)

target_link_libraries(LanLauncherQt PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Network
    Qt${QT_VERSION_MAJOR}::Concurrent
)
if (TARGET Qt${QT_VERSION_MAJOR}::Svg)
    target_link_libraries(LanLauncherQt PRIVATE Qt${QT_VERSION_MAJOR}::Svg)
endif()

set_target_properties(LanLauncherQt PROPERTIES OUTPUT_NAME "QualityOfLifeModManager")

if (WIN32)
    set_target_properties(LanLauncherQt PROPERTIES WIN32_EXECUTABLE TRUE)
    # src/Version.h stays the one place the version is written; read it back so
    # the exe carries a version resource too (file properties, Apps & features).
    file(READ "${CMAKE_CURRENT_SOURCE_DIR}/src/Version.h" _version_header)
    string(REGEX MATCH "#define CLL_VERSION \"([0-9]+\\.[0-9]+\\.[0-9]+)\"" _m "${_version_header}")
    set(APP_VERSION "${CMAKE_MATCH_1}")
    if (NOT APP_VERSION)
        message(FATAL_ERROR "CLL_VERSION not found in src/Version.h")
    endif()
    string(REPLACE "." "," APP_VERSION_COMMAS "${APP_VERSION},0")
    set(APP_NAME "Quality of Life Mod Manager")
    set(APP_AUTHOR "DavidHiFi")
    set(APP_ICON "${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/icon.ico")
    if (EXISTS "${APP_ICON}")
        configure_file(
            "${CMAKE_CURRENT_SOURCE_DIR}/resources/app.rc.in"
            "${CMAKE_CURRENT_BINARY_DIR}/app.rc"
            @ONLY)
        target_sources(LanLauncherQt PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/app.rc")
    else()
        message(WARNING "resources/icons/icon.ico not found — exe will have the default icon")
    endif()
endif()

if (WIN32)
    # crypt32: DPAPI, for the saved Plutonium account token (PlutoniumAuth).
    target_link_libraries(LanLauncherQt PRIVATE crypt32)
endif()

if (MINGW)
    target_link_options(LanLauncherQt PRIVATE -static-libgcc -static-libstdc++)
endif()

# Static Qt: window, jpeg/png, svg and TLS plugins are baked into the exe
get_target_property(_qt_core_type Qt${QT_VERSION_MAJOR}::Core TYPE)
if (_qt_core_type STREQUAL "STATIC_LIBRARY")
    message(STATUS "Qt estatico detectado — importando plugins para o executavel")
    if (MINGW)
        target_link_options(LanLauncherQt PRIVATE -static)
    endif()
    if (QT_VERSION_MAJOR GREATER_EQUAL 6)
        set(_ll_static_plugins)
        foreach(_plugin IN ITEMS
            QWindowsIntegrationPlugin
            QModernWindowsStylePlugin
            QWindowsVistaStylePlugin
            QJpegPlugin
            QGifPlugin
            QICOPlugin
            QPngPlugin
            QWebpPlugin
            QSvgPlugin
            QSvgIconPlugin
            QTlsBackendSchannelPlugin
        )
            if (TARGET Qt${QT_VERSION_MAJOR}::${_plugin})
                list(APPEND _ll_static_plugins Qt${QT_VERSION_MAJOR}::${_plugin})
            endif()
        endforeach()
        if (_ll_static_plugins)
            qt_import_plugins(LanLauncherQt INCLUDE ${_ll_static_plugins})
        endif()
    endif()
endif()

install(TARGETS LanLauncherQt RUNTIME DESTINATION .)
