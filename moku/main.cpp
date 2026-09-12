#include "benchmark/benchmark.hpp"
#include "config.hpp"
#include "engine/engine.hpp"
#include "game/board.hpp"
#include "game/gameloop.hpp"
#include "helpers/helpers.hpp"

#include <cstring>
#include <iostream>

/**
 * This file contains the main function for the moku engine and serves as the entry point
 * of the program.
 */

/**
 * The main function is the entry point of the program.
 * It parses command line arguments to determine the mode of operation
 * (engine, player vs player, or player vs minimax agent) and then
 * starts the appropriate game loop or engine.
 * @param argc The number of command line arguments
 * @param argv The array of command line arguments
 * @return int Returns 0 on successful execution, or a non-zero value on error
 */
int main()
{
    Engine engine;
    engine.run();
}