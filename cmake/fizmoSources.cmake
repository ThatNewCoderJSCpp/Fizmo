file(GLOB_RECURSE FIZMO_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_LIST_DIR}/../Source/*.cpp")

set(FIZMO_GRAPHICS_SOURCE_DIRS GPU Graphics Images Input OpenGL Text UI Vulkan Windows)
set(FIZMO_NET_SOURCE_DIRS Networking)
set(FIZMO_GRAPHICS_SOURCE_FILES System/hot_reload.cpp)

set(FIZMO_CORE_SOURCES "")
set(FIZMO_NET_SOURCES "")
set(FIZMO_GRAPHICS_SOURCES "")

get_filename_component(_fizmo_source_root "${CMAKE_CURRENT_LIST_DIR}/../Source" ABSOLUTE)

foreach(_fizmo_source IN LISTS FIZMO_SOURCES)
    get_filename_component(_fizmo_source "${_fizmo_source}" ABSOLUTE)
    file(RELATIVE_PATH _fizmo_rel "${_fizmo_source_root}" "${_fizmo_source}")
    string(REGEX REPLACE "/.*$" "" _fizmo_top "${_fizmo_rel}")

    if(_fizmo_rel IN_LIST FIZMO_GRAPHICS_SOURCE_FILES OR _fizmo_top IN_LIST FIZMO_GRAPHICS_SOURCE_DIRS)
        list(APPEND FIZMO_GRAPHICS_SOURCES "${_fizmo_source}")
    elseif(_fizmo_top IN_LIST FIZMO_NET_SOURCE_DIRS)
        list(APPEND FIZMO_NET_SOURCES "${_fizmo_source}")
    else()
        list(APPEND FIZMO_CORE_SOURCES "${_fizmo_source}")
    endif()
endforeach()

unset(_fizmo_source_root)
unset(_fizmo_source)
unset(_fizmo_rel)
unset(_fizmo_top)
