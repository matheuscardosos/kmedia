find_package(Catch2 3 CONFIG REQUIRED)

qt_add_executable(kmedia_tests
    test_main.cpp
    test_smoke.cpp
    test_database_manager.cpp
    test_profile_manager.cpp
    test_settings_and_network.cpp
    test_thumbnail_cache.cpp
    test_playback_and_pdf.cpp
    test_background_music.cpp
    test_display_manager.cpp
)

kmedia_set_compiler_warnings(kmedia_tests)

target_link_libraries(kmedia_tests PRIVATE
    kmedia_core
    Catch2::Catch2
    Qt6::Core
    Qt6::Gui
    Qt6::Test
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
