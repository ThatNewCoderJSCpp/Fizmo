set(FIZMO_UNICODE_DATA_DIR "" CACHE PATH "Folder of Unicode Character Database files to build the Unicode tables from (empty uses the bundled tables)")
set(FIZMO_UNICODE_VERSION "" CACHE STRING "Unicode version to download from unicode.org and build the Unicode tables from, such as 17.0.0 (empty uses the bundled tables)")
option(FIZMO_UNICODE_REGENERATE "Build the Unicode tables again on the next configure" OFF)

set(FIZMO_UNICODE_HEADER "${FIZMO_GENERATED_DIR}/fizmo_unicode_data.hpp")
set(FIZMO_UNICODE_FILES
    UnicodeData.txt
    SpecialCasing.txt
    CaseFolding.txt
    DerivedNormalizationProps.txt
    DerivedCoreProperties.txt
    PropList.txt
    EastAsianWidth.txt
    auxiliary/GraphemeBreakProperty.txt
    auxiliary/WordBreakProperty.txt
    emoji/emoji-data.txt)

set(_fizmo_ucd_dir "${FIZMO_UNICODE_DATA_DIR}")

if(NOT _fizmo_ucd_dir AND FIZMO_UNICODE_VERSION)
    set(_fizmo_ucd_dir "${CMAKE_CURRENT_BINARY_DIR}/ucd-${FIZMO_UNICODE_VERSION}")
    foreach(_fizmo_file IN LISTS FIZMO_UNICODE_FILES)
        get_filename_component(_fizmo_name "${_fizmo_file}" NAME)
        if(NOT EXISTS "${_fizmo_ucd_dir}/${_fizmo_name}")
            message(STATUS "fizmo: downloading ${_fizmo_name} for Unicode ${FIZMO_UNICODE_VERSION}")
            file(DOWNLOAD "https://www.unicode.org/Public/${FIZMO_UNICODE_VERSION}/ucd/${_fizmo_file}"
                "${_fizmo_ucd_dir}/${_fizmo_name}.part" STATUS _fizmo_status TLS_VERIFY ON)
            list(GET _fizmo_status 0 _fizmo_code)
            if(NOT _fizmo_code EQUAL 0)
                list(GET _fizmo_status 1 _fizmo_msg)
                file(REMOVE "${_fizmo_ucd_dir}/${_fizmo_name}.part")
                message(FATAL_ERROR "fizmo: could not download ${_fizmo_file} for Unicode ${FIZMO_UNICODE_VERSION}: ${_fizmo_msg}")
            endif()
            file(RENAME "${_fizmo_ucd_dir}/${_fizmo_name}.part" "${_fizmo_ucd_dir}/${_fizmo_name}")
        endif()
    endforeach()
endif()

if(NOT _fizmo_ucd_dir)
    file(REMOVE "${FIZMO_UNICODE_HEADER}")
    unset(FIZMO_UNICODE_GENERATED_FROM CACHE)
elseif(CMAKE_CROSSCOMPILING)
    file(REMOVE "${FIZMO_UNICODE_HEADER}")
    unset(FIZMO_UNICODE_GENERATED_FROM CACHE)
    message(WARNING "fizmo: cannot run the Unicode table generator while cross-compiling, using the bundled tables. "
                    "Generate them on the host with tools/unicode/fizmo_unicode.cpp instead.")
else()
    foreach(_fizmo_file IN LISTS FIZMO_UNICODE_FILES)
        get_filename_component(_fizmo_name "${_fizmo_file}" NAME)
        if(NOT EXISTS "${_fizmo_ucd_dir}/${_fizmo_name}")
            message(FATAL_ERROR "fizmo: ${_fizmo_ucd_dir} is missing ${_fizmo_name}")
        endif()
    endforeach()
    if(NOT EXISTS "${FIZMO_UNICODE_HEADER}" OR FIZMO_UNICODE_REGENERATE OR NOT FIZMO_UNICODE_GENERATED_FROM STREQUAL _fizmo_ucd_dir)
        set(CMAKE_TRY_COMPILE_CONFIGURATION Release)
        try_run(_fizmo_ucd_run _fizmo_ucd_compiled
            "${CMAKE_CURRENT_BINARY_DIR}/fizmo_unicode"
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/unicode/fizmo_unicode.cpp"
            CXX_STANDARD 17
            CXX_STANDARD_REQUIRED ON
            COMPILE_OUTPUT_VARIABLE _fizmo_ucd_compile_log
            RUN_OUTPUT_VARIABLE _fizmo_ucd_run_log
            ARGS "${_fizmo_ucd_dir}" "${FIZMO_UNICODE_HEADER}")
        if(NOT _fizmo_ucd_compiled OR NOT _fizmo_ucd_run EQUAL 0 OR NOT EXISTS "${FIZMO_UNICODE_HEADER}")
            file(REMOVE "${FIZMO_UNICODE_HEADER}")
            message(FATAL_ERROR "fizmo: could not build the Unicode tables\n${_fizmo_ucd_compile_log}${_fizmo_ucd_run_log}")
        endif()
        string(STRIP "${_fizmo_ucd_run_log}" _fizmo_ucd_run_log)
        message(STATUS "${_fizmo_ucd_run_log}")
        set(FIZMO_UNICODE_GENERATED_FROM "${_fizmo_ucd_dir}" CACHE INTERNAL "")
        set(FIZMO_UNICODE_REGENERATE OFF CACHE BOOL "Build the Unicode tables again on the next configure" FORCE)
    else()
        message(STATUS "fizmo: using Unicode tables generated from ${_fizmo_ucd_dir}")
    endif()
endif()

if(EXISTS "${FIZMO_UNICODE_HEADER}")
    set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/Source/String/unicode.cpp" PROPERTIES
        COMPILE_DEFINITIONS FIZMO_UNICODE_GENERATED_DATA
        OBJECT_DEPENDS "${FIZMO_UNICODE_HEADER}"
        SKIP_PRECOMPILE_HEADERS ON
        SKIP_UNITY_BUILD_INCLUSION ON)
endif()
