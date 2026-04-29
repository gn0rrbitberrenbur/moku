#include "../include/utils.hpp"
#include <string>
#include <iostream>

/**
 * This file implements the Utils class defined in ../include/utils.hpp.
 * The Utils class provides various utility functions for the Gomoku engine.
 */

/**
 * Prints the version information of the Gomoku engine to the console.
 * @param None
 * @return void
 */
void print_version()
{
    std::cout << "moku Version 1.0\n";
}

/**
 * Converts a move from algebraic notation (e.g., H8) to a board index (0-224).
 * The function checks for valid input and returns -1 for invalid moves.
 * @param pos The move in algebraic notation to be converted.
 * @param board_size The size of the board (e.g., 15 for a 15x15 board).
 * @return int ; the corresponding board index for the given move, or -1 if the input is invalid.
 */
int algebraic_to_index(const std::string &pos, int board_size)
{
    if (pos.length() < 2) return -1;
    
    char col = std::toupper(pos[0]);
    int row = std::stoi(pos.substr(1)) - 1;
    int col_idx = col - 'A';
    
    if (col_idx < 0 || col_idx >= board_size || row < 0 || row >= board_size) {
        return -1;
    }
    
    return row * board_size + col_idx;
}