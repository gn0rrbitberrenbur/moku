find_package(Threads REQUIRED)

# Core library (board, evaluation, search, transposition table, benchmark)
add_library(moku_core STATIC
    game/tools/algebraic_to_index.cpp
    game/board.cpp
    eval/evaluation.cpp
    search/minimax.cpp
    tt/transposition_table.cpp
    benchmark/benchmark.cpp
)
add_library(moku::core ALIAS moku_core)

target_include_directories(moku_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

# The version from the VERSION file is passed as a compile definition, so config.hpp can use
# MOKU_VERSION directly. PUBLIC makes it available to every target that includes config.hpp.
target_compile_definitions(moku_core PUBLIC MOKU_VERSION="${MOKU_VERSION}")
target_compile_features(moku_core PUBLIC cxx_std_17)
target_link_libraries(moku_core PUBLIC Threads::Threads)

# PIC is required because the static library is linked into the pymoku shared module.
set_target_properties(moku_core PROPERTIES POSITION_INDEPENDENT_CODE ON)

moku_enable_warnings(moku_core)

# Gomocup protocol engine (needed by tests)
if(MOKU_BUILD_ENGINE OR MOKU_BUILD_TESTS)
    add_library(moku_gomocup_engine STATIC engine/gomocup_protocol/gomocup_engine.cpp)
    target_link_libraries(moku_gomocup_engine PUBLIC moku::core)
    moku_enable_warnings(moku_gomocup_engine)
endif()

# Executables
if(MOKU_BUILD_ENGINE)
    add_executable(gomocup_moku main_gomocup.cpp)
    target_link_libraries(gomocup_moku PRIVATE moku_gomocup_engine)
    moku_enable_warnings(gomocup_moku)

    add_library(moku_extended_engine STATIC engine/extended_protocol/extended_protocol.cpp)
    target_link_libraries(moku_extended_engine PUBLIC moku::core)
    moku_enable_warnings(moku_extended_engine)

    add_executable(extended_moku main_extended_protocol.cpp)
    target_link_libraries(extended_moku PRIVATE moku_extended_engine)
    moku_enable_warnings(extended_moku)
endif()

# Tests
if(MOKU_BUILD_TESTS)
    include(GoogleTest)

    add_executable(moku_tests tests/tests.cpp)
    target_link_libraries(moku_tests PRIVATE moku_gomocup_engine GTest::gtest_main)
    moku_enable_warnings(moku_tests)
    gtest_discover_tests(moku_tests DISCOVERY_TIMEOUT 30)

    add_executable(gomocup_protocol_tests tests/gomocup_protocol_test.cpp)
    target_link_libraries(gomocup_protocol_tests PRIVATE moku_gomocup_engine GTest::gtest_main)
    moku_enable_warnings(gomocup_protocol_tests)
    gtest_discover_tests(gomocup_protocol_tests DISCOVERY_TIMEOUT 30)
endif()