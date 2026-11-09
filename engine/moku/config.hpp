#pragma once
#include <string>
#include <thread>

#ifndef MOKU_VERSION
#define MOKU_VERSION "unknown"
#endif

struct GameConfig
{

    // version of the game engine
    std::string version = MOKU_VERSION;

    // must be between 5 and 19 inclusive
    int board_size = 15;

    // maximum search depth for the engine
    int max_depth = 12;

    // time limit for each move in milliseconds, 0 means no limit
    int time_limit_ms = 20000;

    // timeout for the entire match in milliseconds, 0 means no limit
    int timeout_match = 0;

    // time left for the current player in milliseconds
    int time_left = 2147483647;

    // memory limit in bytes; extended INFO bounds TT storage, 0 restores its default capacity
    long long max_memory = 0;

    // type of the game (e.g., 0 for standard, 1 for custom)
    int game_type = 0;

    // rule set for the game (e.g., 0 for standard, 1 for custom)
    int rule = 0;

    // enable or disable metrics logging
    bool debug_output = false;

    // interval for metrics logging in milliseconds
    int metrics_logging_interval = 10;

    // maximum number of nodes to explore, 0 means no limit
    int max_nodes = 0;

    // number of CPU cores to use
    unsigned int cores =
        std::thread::hardware_concurrency() == 0 ? 1 : std::thread::hardware_concurrency();

    // folder for storing game-related files
    std::string folder;

    int squares() const
    {
        return board_size * board_size;
    }
};

// global instance
inline GameConfig g_config;