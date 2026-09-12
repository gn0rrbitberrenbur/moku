cmake_minimum_required(VERSION 3.10)

project(moku VERSION 0.0.5)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
find_package(Threads REQUIRED)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

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

set(MOKU_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# build core library
add_library(moku_core STATIC
    game/tools/algebraic_to_index.cpp
    game/board.cpp
    eval/evaluation.cpp
    search/minimax.cpp
    tt/transposition_table.cpp
)

target_include_directories(moku_core PUBLIC
    ${MOKU_INCLUDE_DIRS}
)

target_compile_features(moku_core PUBLIC cxx_std_17)

if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    # build excutable (gomocup protocol)
    add_executable(gomocup_moku 
        game/tools/algebraic_to_index.cpp
        main_gomocup.cpp
        game/board.cpp
        engine/gomocup_protocol/gomocup_engine.cpp
        benchmark/benchmark.cpp
        eval/evaluation.cpp
        search/minimax.cpp
        tt/transposition_table.cpp
    )

    target_include_directories(gomocup_moku PRIVATE ${MOKU_INCLUDE_DIRS})

    find_package(Threads REQUIRED)
    target_link_libraries(gomocup_moku PRIVATE Threads::Threads)

    if(WIN32)
        set_target_properties(gomocup_moku PROPERTIES OUTPUT_NAME "gomocup_moku")
    else()
        set_target_properties(gomocup_moku PROPERTIES OUTPUT_NAME "gomocup_moku")
    endif()

    # build excutable (extended protocol)
    add_executable(extended_moku 
        game/tools/algebraic_to_index.cpp
        main_gomocup.cpp
        game/board.cpp
        engine/extended_protocol/extended_protocol.cpp
        benchmark/benchmark.cpp
        eval/evaluation.cpp
        search/minimax.cpp
        tt/transposition_table.cpp
    )

    target_include_directories(extended_moku PRIVATE ${MOKU_INCLUDE_DIRS})

    find_package(Threads REQUIRED)
    target_link_libraries(extended_moku PRIVATE Threads::Threads)

    if(WIN32)
        set_target_properties(extended_moku PROPERTIES OUTPUT_NAME "extended_moku")
    else()
        set_target_properties(extended_moku PROPERTIES OUTPUT_NAME "extended_moku")
    endif()

    # build tests
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.14.0
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    # Enable testing
    enable_testing()

    # Tests
    file(GLOB TEST_SOURCES "tests/tests.cpp")
    add_executable(tests 
        ${TEST_SOURCES}
        game/tools/algebraic_to_index.cpp
        benchmark/benchmark.cpp
        game/board.cpp
        eval/evaluation.cpp
        search/minimax.cpp
        tt/transposition_table.cpp
        engine/gomocup_protocol/gomocup_engine.cpp
    )

    target_include_directories(tests PRIVATE ${MOKU_INCLUDE_DIRS})
    target_link_libraries(tests PRIVATE gtest gtest_main Threads::Threads)

    file(GLOB TEST_SOURCES "tests/gomocup_protocol_test.cpp")

    add_executable(gomocup_protocol_tests
        ${TEST_SOURCES}
        game/board.cpp
        eval/evaluation.cpp
        search/minimax.cpp
        tt/transposition_table.cpp
        engine/gomocup_protocol/gomocup_engine.cpp
    )

    target_include_directories(gomocup_protocol_tests PRIVATE
    ${MOKU_INCLUDE_DIRS}
    )

    target_link_libraries(gomocup_protocol_tests PRIVATE
        gtest
        gtest_main
        Threads::Threads
    )

    # Register tests
    include(GoogleTest)
    gtest_discover_tests(tests
        DISCOVERY_TIMEOUT 30
        )
    
    gtest_discover_tests(gomocup_protocol_tests
    DISCOVERY_TIMEOUT 30
    )
endif()