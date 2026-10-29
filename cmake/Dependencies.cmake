# External dependencies, fetched on demand.
#
# Both helpers are macros on purpose: FetchContent_MakeAvailable and find_package must run in
# the scope of the caller so that the resulting targets and functions are visible there.
#
# Offline builds: point CMake to a local checkout, e.g.
#   -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=/path/to/googletest
#   -DFETCHCONTENT_SOURCE_DIR_PYBIND11=/path/to/pybind11

include(FetchContent)

# Provides the target GTest::gtest_main. Call once from the top-level CMakeLists.txt.
macro(moku_fetch_googletest)
    if(NOT TARGET GTest::gtest_main)
        FetchContent_Declare(
            googletest
            GIT_REPOSITORY https://github.com/google/googletest.git
            GIT_TAG        v1.14.0
            GIT_SHALLOW    TRUE
        )
        set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
        set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
        set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
        FetchContent_MakeAvailable(googletest)
    endif()
endmacro()

# Provides pybind11_add_module(). Prefers an installed pybind11 (scikit-build-core supplies
# the one from the build environment) and falls back to fetching it for manual builds.
macro(moku_find_pybind11)
    find_package(pybind11 CONFIG QUIET)
    if(NOT pybind11_FOUND)
        message(STATUS "pybind11 not found, fetching it")
        FetchContent_Declare(
            pybind11
            GIT_REPOSITORY https://github.com/pybind/pybind11.git
            GIT_TAG        v3.0.1
            GIT_SHALLOW    TRUE
        )
        FetchContent_MakeAvailable(pybind11)
    endif()
endmacro()