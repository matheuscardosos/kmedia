cmake_minimum_required(VERSION 3.25)

project(KMedia
    VERSION 0.1.2 # x-release-please-version
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
    Multimedia
    Network
    OpenGL
    Pdf
    Quick
    QuickControls2
    Sql
    Svg
    Test
    WebSockets
)

qt_standard_project_setup(REQUIRES 6.8)

if(APPLE)
    include(AppleWorkarounds)
    kmedia_strip_missing_macos_agl()
endif()

find_package(LibMPV REQUIRED)
find_package(minizip-ng CONFIG REQUIRED)

include(CompilerWarnings)

qt_add_library(kmedia_core STATIC
    src/core/BoothSecurity.h
    src/core/BoothSecurity.cpp
    src/core/DatabaseManager.h
    src/core/DatabaseManager.cpp
    src/core/DatedFolderLayout.h
    src/core/DatedFolderLayout.cpp
    src/core/MeetingCalendarModel.h
    src/core/MeetingCalendarModel.cpp
    src/core/MeetingSections.h
    src/core/MeetingSections.cpp
    src/core/MeetingTimer.h
    src/core/MeetingTimer.cpp
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
    src/integrations/ObsWebSocketClient.h
    src/integrations/ObsWebSocketClient.cpp
    src/integrations/ProcessMonitor.h
    src/integrations/ProcessMonitor.cpp
    src/integrations/ZoomBridge.h
    src/integrations/ZoomBridge.cpp
    src/media/BackgroundMusicEngine.h
    src/media/BackgroundMusicEngine.cpp
    src/media/BackgroundMusicPlanner.h
    src/media/BackgroundMusicPlanner.cpp
    src/media/BackgroundMusicService.h
    src/media/BackgroundMusicService.cpp
    src/media/MpvRendererItem.h
    src/media/MpvRendererItem.cpp
    src/media/PdfViewerController.h
    src/media/PdfViewerController.cpp
    src/media/PdfPageImporter.h
    src/media/PdfPageImporter.cpp
    src/media/CameraController.h
    src/media/CameraController.cpp
    src/media/PlaybackController.h
    src/media/PlaybackController.cpp
    src/media/PlaylistExporter.h
    src/media/PlaylistExporter.cpp
    src/media/MediaAutoExport.h
    src/media/MediaAutoExport.cpp
    src/media/IconImageProvider.h
    src/media/IconImageProvider.cpp
    src/media/ThumbnailCache.h
    src/media/ThumbnailCache.cpp
    src/media/VideoFrameExtractor.h
    src/media/VideoFrameExtractor.cpp
    src/models/MeetingDayModel.h
    src/models/MeetingDayModel.cpp
    src/models/PlaylistModel.h
    src/models/PlaylistModel.cpp
    src/network/CatalogClient.h
    src/network/CatalogClient.cpp
    src/network/BackgroundMusicLibrary.h
    src/network/BackgroundMusicLibrary.cpp
    src/network/DownloadService.h
    src/network/DownloadService.cpp
    src/network/MeetingCatalogService.h
    src/network/MeetingCatalogService.cpp
    src/network/MeetingSyncEngine.h
    src/network/MeetingSyncEngine.cpp
    src/network/PublicTalkMedia.h
    src/network/PublicTalkMedia.cpp
    src/network/PublicationMediaPicker.h
    src/network/PublicationMediaPicker.cpp
    src/network/DocumentMediaImporter.h
    src/network/DocumentMediaImporter.cpp
    src/network/YeartextProvider.h
    src/network/YeartextProvider.cpp
    src/network/StudyBibleMedia.h
    src/network/StudyBibleMedia.cpp
    src/network/BibleMediaCatalog.h
    src/network/BibleMediaCatalog.cpp
    src/network/JwVideoCatalog.h
    src/network/JwVideoCatalog.cpp
    src/network/WatchedFolderMedia.h
    src/network/WatchedFolderMedia.cpp
    src/network/PublicationStore.h
    src/network/PublicationStore.cpp
    src/parsers/CloudSyncWatcher.h
    src/parsers/CloudSyncWatcher.cpp
    src/parsers/JwlPlaylistParser.h
    src/parsers/JwlPlaylistParser.cpp
    src/parsers/JwpubParser.h
    src/parsers/JwpubParser.cpp
    src/parsers/MeetingMediaBuilder.h
    src/parsers/MeetingMediaBuilder.cpp
    src/parsers/PublicationDatabase.h
    src/parsers/PublicationDatabase.cpp
    src/parsers/S34Parser.h
    src/parsers/S34Parser.cpp
    src/parsers/SqlitePackageUtils.h
    src/parsers/SqlitePackageUtils.cpp
    src/parsers/ZipArchiveReader.h
    src/parsers/ZipArchiveReader.cpp
)

kmedia_set_compiler_warnings(kmedia_core)

target_include_directories(kmedia_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(kmedia_core PUBLIC
    KMEDIA_APP_VERSION="${PROJECT_VERSION}"
)

qt6_add_translation(KMEDIA_QM_FILES
    resources/translations/kmedia_en.ts
    resources/translations/kmedia_pt.ts
)
qt6_add_resources(kmedia_core "kmedia_translations"
    PREFIX "/i18n"
    BASE "${CMAKE_CURRENT_BINARY_DIR}"
    FILES ${KMEDIA_QM_FILES}
)

target_link_libraries(kmedia_core
    PUBLIC
        Qt6::Core
        Qt6::Gui
        Qt6::Multimedia
        Qt6::Network
        Qt6::OpenGL
        Qt6::Pdf
        Qt6::Quick
        Qt6::Sql
        Qt6::WebSockets
    PRIVATE
        Qt6::Svg
        LibMPV::LibMPV
        MINIZIP::minizip-ng
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

# macOS asks before the presentation camera starts, with this reason.
set_target_properties(kmedia PROPERTIES
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Info.plist.in"
)

set_source_files_properties(resources/qml/Theme.qml PROPERTIES
    QT_QML_SINGLETON_TYPE TRUE
    QT_RESOURCE_ALIAS Theme.qml
)
set(KMEDIA_QML_ROOT_FILES
    resources/qml/Main.qml
    resources/qml/PresentationWindow.qml
)
set(KMEDIA_QML_SUBDIR_FILES
    resources/qml/controls/StudioIcon.qml
    resources/qml/controls/FocusRing.qml
    resources/qml/controls/StudioButton.qml
    resources/qml/controls/StudioSwitch.qml
    resources/qml/controls/StudioSlider.qml
    resources/qml/controls/StudioScrollBar.qml
    resources/qml/controls/StudioCard.qml
    resources/qml/controls/StudioBadge.qml
    resources/qml/controls/StudioMenu.qml
    resources/qml/controls/StudioMenuItem.qml
    resources/qml/controls/StudioMenuSeparator.qml
    resources/qml/components/NavRailButton.qml
    resources/qml/components/StatusRow.qml
    resources/qml/components/CameraLayer.qml
    resources/qml/components/LeftNavRail.qml
    resources/qml/components/CompactBottomNav.qml
    resources/qml/components/ToastBanner.qml
    resources/qml/components/CommandPalette.qml
    resources/qml/components/ActionPillBar.qml
    resources/qml/components/PillDisplayPopover.qml
    resources/qml/components/PillMusicPopover.qml
    resources/qml/components/PillTimerPopover.qml
    resources/qml/components/PillObsPopover.qml
    resources/qml/components/PillZoomPopover.qml
    resources/qml/components/PillDownloadsPopover.qml
    resources/qml/components/TransportBar.qml
    resources/qml/components/BoothChecklistPopup.qml
    resources/qml/components/MeetingMediaPage.qml
    resources/qml/components/DayDatePicker.qml
    resources/qml/components/SongPickerPopup.qml
    resources/qml/components/PublicTalkPickerPopup.qml
    resources/qml/components/PublicationMediaPopup.qml
    resources/qml/components/BiblePickerPopup.qml
    resources/qml/components/StudyBiblePickerPopup.qml
    resources/qml/components/PdfPagesPopup.qml
    resources/qml/components/JwVideoPickerPopup.qml
    resources/qml/components/FolderPathField.qml
    resources/qml/components/SharedFolderCard.qml
    resources/qml/components/MediaExportCard.qml
    resources/qml/components/TagEditPopup.qml
    resources/qml/components/RenamePopup.qml
    resources/qml/components/CalendarEventPopup.qml
    resources/qml/components/CustomSectionPopup.qml
    resources/qml/components/SettingsPinGate.qml
    resources/qml/components/SettingsSecurityCard.qml
    resources/qml/components/SettingsView.qml
    resources/qml/components/SettingsMeetingsPane.qml
    resources/qml/components/CenterWorkspace.qml
    resources/qml/components/InspectorPanel.qml
)
foreach(qml_file IN LISTS KMEDIA_QML_ROOT_FILES KMEDIA_QML_SUBDIR_FILES)
    file(RELATIVE_PATH qml_alias "${CMAKE_CURRENT_SOURCE_DIR}/resources/qml" "${CMAKE_CURRENT_SOURCE_DIR}/${qml_file}")
    set_source_files_properties("${qml_file}" PROPERTIES QT_RESOURCE_ALIAS "${qml_alias}")
endforeach()

file(GLOB KMEDIA_BUNDLED_ICONS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/lucide/*.svg"
    "${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/custom/*.svg"
)
file(GLOB KMEDIA_BUNDLED_FONTS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/resources/fonts/*.ttf"
)
set(KMEDIA_ASSET_RESOURCES
    resources/icons/icon.png
    resources/icons/logo.svg
)
foreach(abs_res IN LISTS KMEDIA_BUNDLED_ICONS KMEDIA_BUNDLED_FONTS)
    file(RELATIVE_PATH rel_from_root "${CMAKE_CURRENT_SOURCE_DIR}" "${abs_res}")
    list(APPEND KMEDIA_ASSET_RESOURCES "${rel_from_root}")
endforeach()
foreach(res_file IN LISTS KMEDIA_ASSET_RESOURCES)
    file(RELATIVE_PATH res_alias "${CMAKE_CURRENT_SOURCE_DIR}/resources" "${CMAKE_CURRENT_SOURCE_DIR}/${res_file}")
    set_source_files_properties("${res_file}" PROPERTIES QT_RESOURCE_ALIAS "${res_alias}")
endforeach()

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
        ${KMEDIA_QML_ROOT_FILES}
        ${KMEDIA_QML_SUBDIR_FILES}
    RESOURCES
        ${KMEDIA_ASSET_RESOURCES}
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
    Qt6::Svg
    LibMPV::LibMPV
)

if(WIN32 AND DEFINED LibMPV_RUNTIME_DLL AND EXISTS "${LibMPV_RUNTIME_DLL}")
    # One copy step for every executable next to it: two post-build copies of the same DLL
    # into the same folder raced when the app and the tests linked in parallel.
    add_custom_target(kmedia_mpv_runtime
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${LibMPV_RUNTIME_DLL}"
            "$<TARGET_FILE_DIR:kmedia>"
    )
    add_dependencies(kmedia kmedia_mpv_runtime)
endif()

if(KMEDIA_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
