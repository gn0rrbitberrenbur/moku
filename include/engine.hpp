#pragma once
#include "board.hpp"
#include "agents/minimax.hpp"
#include <string>
#include <sstream>
#include <iostream>

class Engine {
private:
    Board board;
    MinimaxAgent agent;
    bool running = true;
    int search_depth = 4;

public:
    Engine() : agent(search_depth) {}
    
    void run();
    
private:
    void process_command(const std::string& line);
    void cmd_position(std::istringstream& iss);
    void cmd_go();
    void cmd_move(const std::string& move_str);
    void cmd_display();
    void cmd_help();
};