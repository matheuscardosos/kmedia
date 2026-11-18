#[=======================================================================[.rst:
FindLibMPV
----------

Finds the libmpv client library and headers, defining the imported
target ``LibMPV::LibMPV``.

Hint variables:
  ``MPV_ROOT`` / ``ENV{MPV_ROOT}`` - root directory containing ``include/mpv/client.h``
#]=======================================================================]

find_package(PkgConfig QUIET)
if(PkgConfig_FOUND AND NOT WIN32)
    pkg_check_modules(PC_LibMPV QUIET mpv)
endif()

find_path(LibMPV_INCLUDE_DIR
    NAMES mpv/client.h
    HINTS
        ${MPV_ROOT}
        $ENV{MPV_ROOT}
        ${PC_LibMPV_INCLUDE_DIRS}
    PATH_SUFFIXES include
)

find_library(LibMPV_LIBRARY
    NAMES mpv libmpv mpv-2 libmpv-2
    HINTS
        ${MPV_ROOT}
        $ENV{MPV_ROOT}
        ${PC_LibMPV_LIBRARY_DIRS}
    PATH_SUFFIXES lib lib/x64 lib/x86_64
)

if(WIN32)
    find_file(LibMPV_RUNTIME_DLL
        NAMES libmpv-2.dll mpv-2.dll mpv-1.dll
        HINTS
            ${MPV_ROOT}
            $ENV{MPV_ROOT}
        PATH_SUFFIXES bin lib .
    )
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LibMPV
    REQUIRED_VARS LibMPV_LIBRARY LibMPV_INCLUDE_DIR
    VERSION_VAR PC_LibMPV_VERSION
)

if(LibMPV_FOUND AND NOT TARGET LibMPV::LibMPV)
    add_library(LibMPV::LibMPV UNKNOWN IMPORTED)
    set_target_properties(LibMPV::LibMPV PROPERTIES
        IMPORTED_LOCATION "${LibMPV_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${LibMPV_INCLUDE_DIR}"
    )
endif()

mark_as_advanced(LibMPV_INCLUDE_DIR LibMPV_LIBRARY LibMPV_RUNTIME_DLL)
