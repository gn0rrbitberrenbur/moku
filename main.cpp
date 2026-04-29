#include "include/board.hpp"
#include "include/gameloop.hpp"
#include "include/engine.hpp"
#include "utils.hpp"
#include <iostream>
#include <cstring>

/**
 * This file contains the main function for the moku engine and serves as the entry point
 * of the program.
 */

/**
 * This function prints the usage information for the program.
 * It is called by using the --help or -h command line options.
 * @param None
 * @return void
 */
void print_usage() {
    std::cout << "Usage: moku [options]" << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  --engine, -e    Run as engine (protocol mode)" << std::endl;
    std::cout << "  --pvp           Player vs Player" << std::endl;
    std::cout << "  --minimax       Player vs Minimax Agent (default)" << std::endl;
    std::cout << "  --version, -v   Show version information" << std::endl;
    std::cout << "  --help, -h      Show this help" << std::endl;
}

/**
 * The main function is the entry point of the program. 
 * It parses command line arguments to determine the mode of operation 
 * (engine, player vs player, or player vs minimax agent) and then 
 * starts the appropriate game loop or engine.
 * @param argc The number of command line arguments
 * @param argv The array of command line arguments
 * @return int Returns 0 on successful execution, or a non-zero value on error
 */
int main(int argc, char *argv[])
{
    // Parse arguments
    bool engine_mode = false;
    bool pvp_mode = false;
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--engine") == 0 || strcmp(argv[i], "-e") == 0) {
            engine_mode = true;
        }
        else if (strcmp(argv[i], "--pvp") == 0) {
            pvp_mode = true;
        }
        else if (strcmp(argv[i], "--version") == 0 || strcmp(argv[i], "-v") == 0) {
            print_version();
            return 0;
        }
        else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage();
            return 0;
        }
    }
    
    if (engine_mode) {
        Engine engine;
        engine.run();
    }
    else if (pvp_mode) {
        Board board;
        game_loop_pvp(board);
    }
    else {
        Board board;
        game_loop_minimax(board);
    }
    
    return 0;
}