#include "../include/utils.hpp"
#include "../include/config.hpp"
#include <cctype>
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
 * @param size The size of the board (e.g., 15 for a 15x15 board).
 * @return int ; the corresponding board index for the given move, or -1 if the input is invalid.
 */
int algebraic_to_index(const std::string& input, int size)
{
    if (input.length() < 2) return -1;
    
    if (size <= 0) size = g_config.board_size;
    
    char col_char = std::toupper(input[0]);
    int col = col_char - 'A';
    
    int row;
    try {
        row = std::stoi(input.substr(1)) - 1;
    } catch (...) {
        return -1;
    }
    
    if (col < 0 || col >= size || row < 0 || row >= size) {
        return -1;
    }
    
    return row * size + col;
}