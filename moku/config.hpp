#pragma once
#include <string>
#include <thread>

struct GameConfig
{

    std::string version = "0.0.6";

    int board_size = 15;

    int max_depth = 12;

    int time_limit_ms = 20000;
    int timeout_match = 0;
    int time_left = 2147483647;

    long long max_memory = 0;

    int game_type = 0;

    int rule = 0;

    std::string folder;

    int squares() const
    {
        return board_size * board_size;
    }

    bool debug_output = false;

    unsigned int cores =
        std::thread::hardware_concurrency() == 0 ? 1 : std::thread::hardware_concurrency();
};

// global instance
inline GameConfig g_config;