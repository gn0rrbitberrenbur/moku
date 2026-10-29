cmake_minimum_required(VERSION 3.15)

# The VERSION file is the single source of truth for the project version.
file(STRINGS "${CMAKE_CURRENT_LIST_DIR}/VERSION" MOKU_VERSION LIMIT_COUNT 1)
string(STRIP "${MOKU_VERSION}" MOKU_VERSION)
if(NOT MOKU_VERSION MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
    message(FATAL_ERROR "VERSION must have the form x.y.z, got '${MOKU_VERSION}'")
endif()

project(moku VERSION ${MOKU_VERSION} LANGUAGES CXX)

# Re-run configure when VERSION changes so the generated version.hpp never goes stale.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_LIST_DIR}/VERSION")

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(MOKU_IS_TOP_LEVEL ON)
else()
    set(MOKU_IS_TOP_LEVEL OFF)
endif()

if(MOKU_IS_TOP_LEVEL)
    if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
        set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
    endif()
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
endif()

# Engine and tests default to ON only for a standalone build. When moku is pulled in via
# FetchContent or add_subdirectory, only the moku_core library is built unless requested.
option(MOKU_BUILD_ENGINE "Build the gomocup_moku and extended_moku executables" ${MOKU_IS_TOP_LEVEL})
option(MOKU_BUILD_TESTS  "Build the GoogleTest suites"                          ${MOKU_IS_TOP_LEVEL})
option(MOKU_BUILD_PYTHON "Build the pymoku pybind11 module"                     OFF)
option(MOKU_BUILD_NNUE   "Build the nnue_gen dataset generator"                 OFF)

include("${CMAKE_CURRENT_LIST_DIR}/cmake/CompilerWarnings.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/cmake/Dependencies.cmake")

if(MOKU_BUILD_TESTS)
    enable_testing()
    moku_fetch_googletest()
endif()

add_subdirectory(moku)

if(MOKU_BUILD_PYTHON)
    add_subdirectory(bindings/python)
endif()

if(MOKU_BUILD_NNUE)
    add_subdirectory(tools/nnue)
endif()

message(STATUS "moku ${MOKU_VERSION} | engine=${MOKU_BUILD_ENGINE} tests=${MOKU_BUILD_TESTS} "
               "python=${MOKU_BUILD_PYTHON} nnue=${MOKU_BUILD_NNUE}")