#pragma once

struct GameConfig {
    int board_size = 15;
    int max_depth = 12;
    int time_limit_ms = 20000;
    
    int squares() const { return board_size * board_size; }

    bool debug_output = false;

    std::string version = "0.0.1";
};

// global instance
inline GameConfig g_config;