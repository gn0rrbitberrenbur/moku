add_executable(nnue_gen
    src/main.cpp
    src/nnue.cpp
)
target_include_directories(nnue_gen PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_SOURCE_DIR}/engine/moku"
    "${CMAKE_SOURCE_DIR}/engine/moku/game"
)

target_link_libraries(nnue_gen PRIVATE moku::core)
moku_enable_warnings(nnue_gen)

if(MOKU_BUILD_TESTS)
    include(GoogleTest)

    add_executable(nnue_tests tests/test_moku.cpp)
    target_include_directories(nnue_tests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
        "${CMAKE_SOURCE_DIR}/engine/moku"
        "${CMAKE_SOURCE_DIR}/engine/moku/game"
    )
    target_link_libraries(nnue_tests PRIVATE moku::core GTest::gtest_main)
    moku_enable_warnings(nnue_tests)
    gtest_discover_tests(nnue_tests DISCOVERY_TIMEOUT 30)
endif()