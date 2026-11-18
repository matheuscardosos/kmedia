cmake_minimum_required(VERSION 3.25)

project(KMedia
    VERSION 0.1.0
    DESCRIPTION "Native C++20, Qt 6, and libmpv congregation media presentation workstation"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

option(KMEDIA_BUILD_TESTS "Build unit and integration tests" ON)

find_package(Qt6 6.8 REQUIRED COMPONENTS
    Core
    Gui
    LinguistTools
    Network
    OpenGL
    Pdf
    Quick
    QuickControls2
    Sql
    Test
    WebSockets
)

qt_standard_project_setup(REQUIRES 6.8)

if(APPLE)
    include(AppleWorkarounds)
    kmedia_strip_missing_macos_agl()
endif()

find_package(LibMPV REQUIRED)

include(CompilerWarnings)

qt_add_library(kmedia_core STATIC
    src/core/DatabaseManager.h
    src/core/DatabaseManager.cpp
    src/core/NetworkManager.h
    src/core/NetworkManager.cpp
    src/core/ProfileManager.h
    src/core/ProfileManager.cpp
    src/core/RuntimeInfo.h
    src/core/RuntimeInfo.cpp
    src/core/SettingsStore.h
    src/core/SettingsStore.cpp
    src/core/SystemUtils.h
    src/core/SystemUtils.cpp
    src/integrations/DisplayManager.h
    src/integrations/DisplayManager.cpp
    src/media/BackgroundMusicEngine.h
    src/media/BackgroundMusicEngine.cpp
    src/media/MpvRendererItem.h
    src/media/MpvRendererItem.cpp
    src/media/PdfViewerController.h
    src/media/PdfViewerController.cpp
    src/media/PlaybackController.h
    src/media/PlaybackController.cpp
    src/media/ThumbnailCache.h
    src/media/ThumbnailCache.cpp
)

kmedia_set_compiler_warnings(kmedia_core)

target_include_directories(kmedia_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(kmedia_core PUBLIC
    KMEDIA_APP_VERSION="${PROJECT_VERSION}"
)

target_link_libraries(kmedia_core
    PUBLIC
        Qt6::Core
        Qt6::Gui
        Qt6::Network
        Qt6::OpenGL
        Qt6::Pdf
        Qt6::Quick
        Qt6::Sql
        Qt6::WebSockets
    PRIVATE
        LibMPV::LibMPV
        $<$<PLATFORM_ID:Windows>:psapi>
)

set(KMEDIA_PLATFORM_SOURCES "")
if(WIN32 AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/kmedia.rc")
    list(APPEND KMEDIA_PLATFORM_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/kmedia.rc"
    )
elseif(APPLE AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/kmedia.icns")
    set(MACOSX_BUNDLE_ICON_FILE kmedia.icns)
    set_source_files_properties(
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/kmedia.icns"
        PROPERTIES MACOSX_PACKAGE_LOCATION "Resources"
    )
    list(APPEND KMEDIA_PLATFORM_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/kmedia.icns"
    )
endif()

qt_add_executable(kmedia
    WIN32
    MACOSX_BUNDLE
    src/main.cpp
    ${KMEDIA_PLATFORM_SOURCES}
)

kmedia_set_compiler_warnings(kmedia)

set_source_files_properties(resources/qml/Theme.qml PROPERTIES
    QT_QML_SINGLETON_TYPE TRUE
    QT_RESOURCE_ALIAS Theme.qml
)
set_source_files_properties(resources/qml/Main.qml PROPERTIES
    QT_RESOURCE_ALIAS Main.qml
)
set_source_files_properties(resources/qml/PresentationWindow.qml PROPERTIES
    QT_RESOURCE_ALIAS PresentationWindow.qml
)
set_source_files_properties(resources/qml/components/NavRailButton.qml PROPERTIES
    QT_RESOURCE_ALIAS components/NavRailButton.qml
)
set_source_files_properties(resources/qml/components/StatusRow.qml PROPERTIES
    QT_RESOURCE_ALIAS components/StatusRow.qml
)
set_source_files_properties(resources/qml/components/LeftNavRail.qml PROPERTIES
    QT_RESOURCE_ALIAS components/LeftNavRail.qml
)
set_source_files_properties(resources/qml/components/CenterWorkspace.qml PROPERTIES
    QT_RESOURCE_ALIAS components/CenterWorkspace.qml
)
set_source_files_properties(resources/qml/components/InspectorPanel.qml PROPERTIES
    QT_RESOURCE_ALIAS components/InspectorPanel.qml
)
set_source_files_properties(resources/icons/icon.png PROPERTIES
    QT_RESOURCE_ALIAS icons/icon.png
)

qt_add_qml_module(kmedia
    URI KMedia.UI
    DEPENDENCIES
        QtQuick
    SOURCES
        src/app/ApplicationController.h
        src/app/ApplicationController.cpp
        src/app/QmlTypes.h
    QML_FILES
        resources/qml/Theme.qml
        resources/qml/Main.qml
        resources/qml/PresentationWindow.qml
        resources/qml/components/NavRailButton.qml
        resources/qml/components/StatusRow.qml
        resources/qml/components/LeftNavRail.qml
        resources/qml/components/CenterWorkspace.qml
        resources/qml/components/InspectorPanel.qml
    RESOURCES
        resources/icons/icon.png
)

target_include_directories(kmedia PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/app
    ${CMAKE_CURRENT_SOURCE_DIR}/src/media
)

target_link_libraries(kmedia PRIVATE
    kmedia_core
    Qt6::Gui
    Qt6::OpenGL
    Qt6::Quick
    Qt6::QuickControls2
    LibMPV::LibMPV
)

if(WIN32 AND DEFINED LibMPV_RUNTIME_DLL AND EXISTS "${LibMPV_RUNTIME_DLL}")
    add_custom_command(TARGET kmedia POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${LibMPV_RUNTIME_DLL}"
            "$<TARGET_FILE_DIR:kmedia>"
    )
endif()

if(KMEDIA_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
