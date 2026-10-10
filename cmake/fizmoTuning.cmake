set(FIZMO_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(FIZMO_TUNING_HEADER "${FIZMO_GENERATED_DIR}/fizmo_mul_tuning.hpp")
file(MAKE_DIRECTORY "${FIZMO_GENERATED_DIR}")

option(FIZMO_TUNE "Measure the multiplication cutoffs on this machine when configuring" ON)
option(FIZMO_RETUNE "Measure the multiplication cutoffs again on the next configure" OFF)
set(FIZMO_MUL_THRESHOLDS "" CACHE STRING "Fixed multiplication cutoffs as karatsuba;toom3;toom4;ntt (overrides tuning)")

function(fizmo_write_tuning_header k t3 t4 ntt)
    file(WRITE "${FIZMO_TUNING_HEADER}"
        "#ifndef FIZMO_MUL_TUNING_HPP\n"
        "#define FIZMO_MUL_TUNING_HPP\n\n"
        "#define FIZMO_MUL_KARATSUBA_THRESHOLD ${k}\n"
        "#define FIZMO_MUL_TOOM3_THRESHOLD     ${t3}\n"
        "#define FIZMO_MUL_TOOM4_THRESHOLD     ${t4}\n"
        "#define FIZMO_MUL_NTT_THRESHOLD       ${ntt}\n\n"
        "#endif // FIZMO_MUL_TUNING_HPP\n")
endfunction()

if(FIZMO_MUL_THRESHOLDS)
    list(LENGTH FIZMO_MUL_THRESHOLDS _fizmo_count)
    if(NOT _fizmo_count EQUAL 4)
        message(FATAL_ERROR "FIZMO_MUL_THRESHOLDS needs four values: karatsuba;toom3;toom4;ntt")
    endif()
    list(GET FIZMO_MUL_THRESHOLDS 0 _k)
    list(GET FIZMO_MUL_THRESHOLDS 1 _t3)
    list(GET FIZMO_MUL_THRESHOLDS 2 _t4)
    list(GET FIZMO_MUL_THRESHOLDS 3 _ntt)
    fizmo_write_tuning_header(${_k} ${_t3} ${_t4} ${_ntt})
    message(STATUS "fizmo: multiplication cutoffs set to ${FIZMO_MUL_THRESHOLDS}")
elseif(NOT FIZMO_TUNE OR CMAKE_CROSSCOMPILING)
    file(REMOVE "${FIZMO_TUNING_HEADER}")
    message(STATUS "fizmo: using the default multiplication cutoffs")
elseif(NOT EXISTS "${FIZMO_TUNING_HEADER}" OR FIZMO_RETUNE)
    message(STATUS "fizmo: measuring multiplication cutoffs for this machine (about 15 seconds)")
    if(MSVC)
        set(_fizmo_opt /O2)
    else()
        set(_fizmo_opt -O3)
    endif()
    set(CMAKE_TRY_COMPILE_CONFIGURATION Release)
    try_run(_fizmo_run _fizmo_compiled
        "${CMAKE_CURRENT_BINARY_DIR}/fizmo_tune"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/tune/fizmo_tune.cpp"
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
        COMPILE_DEFINITIONS ${_fizmo_opt}
        COMPILE_OUTPUT_VARIABLE _fizmo_compile_log
        RUN_OUTPUT_VARIABLE _fizmo_run_log
        ARGS "${FIZMO_TUNING_HEADER}" --quiet)
    if(NOT _fizmo_compiled OR NOT _fizmo_run EQUAL 0 OR NOT EXISTS "${FIZMO_TUNING_HEADER}")
        file(REMOVE "${FIZMO_TUNING_HEADER}")
        message(WARNING "fizmo: could not measure the multiplication cutoffs, using the defaults\n${_fizmo_compile_log}${_fizmo_run_log}")
    else()
        string(STRIP "${_fizmo_run_log}" _fizmo_run_log)
        message(STATUS "${_fizmo_run_log}")
    endif()
    set(FIZMO_RETUNE OFF CACHE BOOL "Measure the multiplication cutoffs again on the next configure" FORCE)
else()
    file(STRINGS "${FIZMO_TUNING_HEADER}" _fizmo_lines REGEX "#define FIZMO_MUL_.*_THRESHOLD")
    string(REGEX REPLACE "#define FIZMO_MUL_([A-Z0-9]+)_THRESHOLD +([0-9]+)" "\\1=\\2" _fizmo_lines "${_fizmo_lines}")
    message(STATUS "fizmo: multiplication cutoffs from an earlier measurement: ${_fizmo_lines}")
endif()

target_include_directories(fizmo PUBLIC
    $<BUILD_INTERFACE:${FIZMO_GENERATED_DIR}>
    $<INSTALL_INTERFACE:include/fizmo/generated>)
