#ifndef ENGINE_HPP
#define ENGINE_HPP

#include "board.hpp"
#include "minimax/minimax.hpp"
#include <string>
#include <sstream>

class Engine {
protected:
    Board board;
    MinimaxAgent agent;
    bool running = true;
    
    // Gomocup Protocol Info
    int timeout_turn = 30000;  // ms
    int timeout_match = 0;
    int time_left = 2147483647;
    long long max_memory = 0;
    int game_type = 0;
    int rule = 0;
    std::string folder;
    
    // Engine settings
    int search_depth = 6;
    
    void process_command(const std::string& line);
    
    // Mandatory commands
    void cmd_start(std::istringstream& iss);
    void cmd_begin();
    void cmd_turn(std::istringstream& iss);
    void cmd_board();
    void cmd_info(std::istringstream& iss);
    void cmd_about();
    
    // Optional commands
    void cmd_takeback(std::istringstream& iss);
    
    // Debug commands
    void cmd_display();
    void cmd_help();
    
public:
    Engine() = default;
    void run();
};

#endif