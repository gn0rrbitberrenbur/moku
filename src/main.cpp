#include "../include/board.hpp"
#include "../include/gameloop.hpp"
#include "../include/engine.hpp"
#include "../include/config.hpp"
#include "../include/utils.hpp"
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
    std::cout << "  --engine, -e      Run as engine (protocol mode)" << std::endl;
    std::cout << "  --pvp, -p         Player vs Player" << std::endl;
    std::cout << "  --minimax, -m     Player vs Minimax Agent (default)" << std::endl;
    std::cout << "  --size, -s <n>    Board size (5-19, default: 15)" << std::endl;
    std::cout << "  --depth, -d <n>   Max search depth (default: 12)" << std::endl;
    std::cout << "  --time, -t <ms>   Time limit in ms (default: 20000)" << std::endl;
    std::cout << "  --debug           Enable debug output" << std::endl;
    std::cout << "  --version, -v     Show version information" << std::endl;
    std::cout << "  --help, -h        Show this help" << std::endl;
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
        else if (strcmp(argv[i], "--pvp") == 0 || strcmp(argv[i], "-p") == 0) {
            pvp_mode = true;
        }
        else if (strcmp(argv[i], "--minimax") == 0 || strcmp(argv[i], "-m") == 0) {
            // minimax mode is the default, so no need to set a flag
        }
        else if (strcmp(argv[i], "--benchmark") == 0 || strcmp(argv[i], "-b") == 0) {
            // benchmark mode is not implemented yet, so no need to set a flag
        }
        else if (strcmp(argv[i], "--version") == 0 || strcmp(argv[i], "-v") == 0) {
            print_version();
            return 0;
        }
        else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage();
            return 0;
        }
        // setting overwriting config parameters
        else if ((strcmp(argv[i], "--size") == 0 || strcmp(argv[i], "-s") == 0) && i + 1 < argc) {
            g_config.board_size = std::stoi(argv[++i]);
            if (g_config.board_size < 5 || g_config.board_size > 19) {
                std::cerr << "[error] Board size must be between 5 and 19" << std::endl;
                g_config.board_size = 15;
            }
        }
        else if ((strcmp(argv[i], "--depth") == 0 || strcmp(argv[i], "-d") == 0) && i + 1 < argc) {
            g_config.max_depth = std::stoi(argv[++i]);
        }
        else if ((strcmp(argv[i], "--time") == 0 || strcmp(argv[i], "-t") == 0) && i + 1 < argc) {
            g_config.time_limit_ms = std::stoi(argv[++i]);
        }
        else if (strcmp(argv[i], "--debug") == 0) {
            g_config.debug_output = true;
        }
    }
    
    // print config info in non-engine modes
    if (!engine_mode) {
        std::cout << "[info] Config: size=" << g_config.board_size 
                  << "x" << g_config.board_size
                  << ", depth=" << g_config.max_depth
                  << ", time=" << g_config.time_limit_ms << "ms" << std::endl;
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