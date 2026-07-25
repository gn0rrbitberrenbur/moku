#pragma once
#include <thread>

struct GameConfig {
    int board_size = 15;
    int max_depth = 12;
    int time_limit_ms = 20000;
    
    int squares() const { return board_size * board_size; }

    bool debug_output = false;

    std::string version = "0.0.2";

    unsigned int cores = std::thread::hardware_concurrency() == 0
                             ? 1
                             : std::thread::hardware_concurrency();
};

// global instance
inline GameConfig g_config;