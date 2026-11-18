# Work around QTBUG-137687 on macOS SDKs (Xcode 26+) where Apple removed
# AGL.framework while Qt 6.8 imported targets and FindWrapOpenGL still link it.
function(kmedia_strip_missing_macos_agl)
    if(NOT APPLE)
        return()
    endif()

    get_directory_property(targets_to_clean IMPORTED_TARGETS)
    foreach(known_target IN ITEMS
        WrapOpenGL::WrapOpenGL
        OpenGL::GL
        Qt6::Core
        Qt6::Gui
        Qt6::GuiPrivate
        Qt6::OpenGL
        Qt6::OpenGLPrivate
        Qt6::Quick
        Qt6::QuickControls2
        Qt6::Pdf
        Qt6::Test
    )
        if(TARGET "${known_target}")
            list(APPEND targets_to_clean "${known_target}")
        endif()
    endforeach()
    list(REMOVE_DUPLICATES targets_to_clean)

    foreach(target_name IN LISTS targets_to_clean)
        get_target_property(aliased_target "${target_name}" ALIASED_TARGET)
        if(aliased_target)
            continue()
        endif()

        foreach(prop_name IN ITEMS INTERFACE_LINK_LIBRARIES IMPORTED_LINK_INTERFACE_LIBRARIES INTERFACE_LINK_OPTIONS)
            get_target_property(link_items "${target_name}" "${prop_name}")
            if(NOT link_items)
                continue()
            endif()

            set(filtered_items "")
            set(modified FALSE)
            set(skip_next FALSE)
            list(LENGTH link_items items_len)
            if(items_len GREATER 0)
                math(EXPR last_idx "${items_len} - 1")
                foreach(idx RANGE 0 ${last_idx})
                    if(skip_next)
                        set(skip_next FALSE)
                        continue()
                    endif()

                    list(GET link_items ${idx} item)
                    if(item STREQUAL "-framework" AND idx LESS ${last_idx})
                        math(EXPR next_idx "${idx} + 1")
                        list(GET link_items ${next_idx} next_item)
                        if(next_item MATCHES "(^|/)AGL(\\.framework|\\.tbd)?$")
                            set(skip_next TRUE)
                            set(modified TRUE)
                            continue()
                        endif()
                    endif()

                    if(item MATCHES "(^|[:;/< ])AGL(\\.framework|\\.tbd|>|$)")
                        set(modified TRUE)
                        continue()
                    endif()

                    list(APPEND filtered_items "${item}")
                endforeach()
            endif()

            if(modified)
                message(STATUS "Removed deprecated AGL.framework link entry from ${target_name} (${prop_name})")
                set_target_properties("${target_name}" PROPERTIES
                    "${prop_name}" "${filtered_items}"
                )
            endif()
        endforeach()
    endforeach()
endfunction()
