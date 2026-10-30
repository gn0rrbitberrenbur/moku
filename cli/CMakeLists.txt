cmake_minimum_required(VERSION 3.15)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(moku_cli VERSION 1.0.0 LANGUAGES CXX)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)

add_library(cli_core
    moku-cli/board/board.cpp
    moku-cli/engine/engine.cpp
    moku-cli/engine/debug.cpp
    moku-cli/file_handling/file_io.cpp
    moku-cli/file_handling/json.cpp
    moku-cli/process/process.cpp
    moku-cli/settings/settings.cpp
    moku-cli/tests/test_report.cpp
    moku-cli/tests/test_runner.cpp
)

target_include_directories(cli_core PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/moku-cli"
)

target_link_libraries(cli_core PUBLIC Threads::Threads)

if(WIN32)
    target_compile_definitions(cli_core PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

add_executable(moku_cli
    moku-cli/main.cpp
    moku-cli/ui/application.cpp
    moku-cli/ui/terminal.cpp
    moku-cli/tests/test_mode.cpp
    moku-cli/ui/ui_helpers.cpp
)
target_link_libraries(moku_cli PRIVATE cli_core)

if(MINGW)
    set_property(TARGET moku_cli APPEND_STRING PROPERTY LINK_FLAGS " -municode")
endif()

if(MSVC)
    target_compile_options(cli_core PRIVATE /W4 /permissive-)
    target_compile_options(moku_cli PRIVATE /W4 /permissive-)
else()
    target_compile_options(cli_core PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(moku_cli PRIVATE -Wall -Wextra -Wpedantic)
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(cli_mock_engine tests/mock_engine.cpp)
    add_executable(cli_tests tests/tests.cpp)
    target_link_libraries(cli_tests PRIVATE cli_core)
    add_test(NAME cli_tests COMMAND cli_tests $<TARGET_FILE:cli_mock_engine>)
    set_tests_properties(cli_tests PROPERTIES TIMEOUT 90)

    if(WIN32)
        add_executable(cli_terminal_tests tests/terminal_tests.cpp)
        target_link_libraries(cli_terminal_tests PRIVATE cli_core)
        add_test(NAME cli_terminal_tests COMMAND cli_terminal_tests
            $<TARGET_FILE:moku_cli> $<TARGET_FILE:cli_mock_engine>)
        set_tests_properties(cli_terminal_tests PROPERTIES TIMEOUT 45)
    endif()
endif()