#include "config.hpp"
#include "helpers.hpp"
#include <cctype>
#include <string>
#include <iostream>

/**
 * Prints the version information of the Gomoku engine to the console.
 * @param None
 * @return void
 */
void print_version()
{
    std::cout << "moku Version " << g_config.version << std::endl;
}

/**
 * This function prints the usage information for the program.
 * It is called by using the --help or -h command line options.
 * @param None
 * @return void
 */
void print_help() {
    std::cout << "Usage: moku [options]" << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  --engine, -e      Run as engine (protocol mode)" << std::endl;
    std::cout << "  --pvp, -p         Player vs Player" << std::endl;
    std::cout << "  --minimax, -m     Player vs Minimax Agent (default)" << std::endl;
    std::cout << "  --size, -s <n>    Board size (5-19, default: 15)" << std::endl;
    std::cout << "  --depth, -d <n>   Max search depth (default: 12)" << std::endl;
    std::cout << "  --time, -t <ms>   Time limit in ms (default: 20000)" << std::endl;
    std::cout << "  --cores, -c <n>   Number of CPU cores to use (default: max available)" << std::endl;
    std::cout << "  --debug           Enable debug output" << std::endl;
    std::cout << "  --benchmark, -b   Run benchmark tests" << std::endl;
    std::cout << "  --version, -v     Show version information" << std::endl;
    std::cout << "  --help, -h        Show this help" << std::endl;
}