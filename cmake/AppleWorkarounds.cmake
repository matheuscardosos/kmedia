# Work around QTBUG-137687 on newer macOS SDKs (Xcode 26+) where Apple
# removed AGL.framework while Qt 6.8 imported targets still reference it.
function(kmedia_strip_missing_macos_agl)
    if(NOT APPLE)
        return()
    endif()

    find_library(KMEDIA_AGL_FRAMEWORK NAMES AGL)
    if(KMEDIA_AGL_FRAMEWORK)
        return()
    endif()

    get_directory_property(imported_targets IMPORTED_TARGETS)
    foreach(target_name IN LISTS imported_targets)
        get_target_property(aliased_target "${target_name}" ALIASED_TARGET)
        if(aliased_target)
            continue()
        endif()

        get_target_property(link_libs "${target_name}" INTERFACE_LINK_LIBRARIES)
        if(NOT link_libs)
            continue()
        endif()

        set(filtered_libs "")
        set(modified FALSE)
        set(skip_next FALSE)
        list(LENGTH link_libs libs_len)
        if(libs_len GREATER 0)
            math(EXPR last_idx "${libs_len} - 1")
            foreach(idx RANGE 0 ${last_idx})
                if(skip_next)
                    set(skip_next FALSE)
                    continue()
                endif()

                list(GET link_libs ${idx} item)
                if(item STREQUAL "-framework" AND idx LESS ${last_idx})
                    math(EXPR next_idx "${idx} + 1")
                    list(GET link_libs ${next_idx} next_item)
                    if(next_item STREQUAL "AGL")
                        set(skip_next TRUE)
                        set(modified TRUE)
                        continue()
                    endif()
                endif()

                if(item MATCHES "(^-framework AGL$|^\\$<LINK_ONLY:-framework AGL>$|/AGL\\.framework$|/AGL\\.tbd$)")
                    set(modified TRUE)
                    continue()
                endif()

                list(APPEND filtered_libs "${item}")
            endforeach()
        endif()

        if(modified)
            message(STATUS "Removed unavailable AGL.framework link entry from ${target_name}")
            set_target_properties("${target_name}" PROPERTIES
                INTERFACE_LINK_LIBRARIES "${filtered_libs}"
            )
        endif()
    endforeach()
endfunction()
