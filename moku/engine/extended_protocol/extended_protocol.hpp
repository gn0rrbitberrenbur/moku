#ifndef EXTENDED_PROTOCOL_HPP
#define EXTENDED_PROTOCOL_HPP

#include "config.hpp"
#include "game/board.hpp"
#include "search/minimax.hpp"

#include <sstream>
#include <string>

class Engine
{
protected:
    Board board;
    MinimaxAgent agent;
    bool running = true;

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

    // todo
    void cmd_benchmark(std::istringstream& iss);
    void cmd_analyze(std::istringstream& iss);

public:
    Engine() : agent(g_config.max_depth) {}

    void run();
};

#endif