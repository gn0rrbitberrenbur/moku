#include "../include/utils.hpp"
#include <string>
#include <iostream>

void print_version()
{
    std::cout << "moku Version 1.0\n";
}

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