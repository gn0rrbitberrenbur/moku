cmake_minimum_required(VERSION 3.10)

project(Gomoku VERSION 1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Platform specific flags
if(WIN32)
    add_definitions(-D_WIN32)
elseif(UNIX AND NOT APPLE)
    add_definitions(-DLINUX)
elseif(APPLE)
    add_definitions(-DMACOS)
endif()

if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -pedantic)
endif()

# Add GoogleTest
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Enable testing
enable_testing()

add_executable(moku 
    main.cpp
    src/board.cpp
    src/gameloop.cpp
    src/utils.cpp
    src/engine.cpp
    src/agent/evaluation.cpp
    src/agent/minimax.cpp
)

target_include_directories(moku PRIVATE 
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/agents
)

find_package(Threads REQUIRED)
target_link_libraries(moku PRIVATE Threads::Threads)

# Tests
file(GLOB TEST_SOURCES "tests/*.cpp")
add_executable(run_tests 
    ${TEST_SOURCES}
    src/board.cpp
    src/utils.cpp
    src/agent/evaluation.cpp
    src/agent/minimax.cpp
)
target_link_libraries(run_tests PRIVATE gtest gtest_main Threads::Threads)

# Register tests
include(GoogleTest)
gtest_discover_tests(run_tests)

if(WIN32)
    set_target_properties(moku PROPERTIES OUTPUT_NAME "moku")
else()
    set_target_properties(moku PROPERTIES OUTPUT_NAME "moku")
endif()