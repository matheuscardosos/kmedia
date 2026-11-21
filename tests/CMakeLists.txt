find_package(Catch2 3 CONFIG REQUIRED)

qt_add_executable(kmedia_tests
    TestZipFixtures.h
    test_main.cpp
    test_smoke.cpp
    test_booth_security.cpp
    test_database_manager.cpp
    test_meeting_calendar.cpp
    test_meeting_timer.cpp
    test_profile_manager.cpp
    test_settings_and_network.cpp
    test_thumbnail_cache.cpp
    test_playback_and_pdf.cpp
    test_background_music.cpp
    test_display_manager.cpp
    test_jwpub_and_s34.cpp
    test_jwlplaylist_parser.cpp
    test_download_service.cpp
    test_meeting_catalog.cpp
    test_meeting_sync_engine.cpp
    test_cloud_sync_watcher.cpp
    test_dated_folders_and_export.cpp
    test_publication_queries.cpp
    test_process_monitor.cpp
    test_obs_and_zoom.cpp
    test_playlist_and_demo.cpp
)

kmedia_set_compiler_warnings(kmedia_tests)

target_link_libraries(kmedia_tests PRIVATE
    kmedia_core
    Catch2::Catch2
    MINIZIP::minizip-ng
    Qt6::Core
    Qt6::Gui
    Qt6::Network
    Qt6::Sql
    Qt6::Test
    Qt6::WebSockets
    LibMPV::LibMPV
)

if(WIN32 AND DEFINED LibMPV_RUNTIME_DLL AND EXISTS "${LibMPV_RUNTIME_DLL}")
    add_custom_command(TARGET kmedia_tests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${LibMPV_RUNTIME_DLL}"
            "$<TARGET_FILE_DIR:kmedia_tests>"
    )
endif()

include(Catch)
catch_discover_tests(kmedia_tests)
