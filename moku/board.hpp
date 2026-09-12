#pragma once
#include <bitset>
#include <iostream>
#include "config.hpp"

// max board size, compile time constant for bitset size
constexpr int MAX_SIZE = 19;
constexpr int MAX_SQUARES = MAX_SIZE * MAX_SIZE;  // 361

class Board {
public:
    // bitset with max size
    std::bitset<MAX_SQUARES> black;
    std::bitset<MAX_SQUARES> white;

    // dynamic bitset size based on config
    int size() const { return g_config.board_size; }
    int squares() const { return g_config.squares(); }
    
    // helper functions to convert between 2D and 1D positions
    int pos(int row, int col) const { return row * size() + col; }
    int row(int p) const { return p / size(); }
    int col(int p) const { return p % size(); }
    
    bool in_bounds(int row, int col) const {
        return row >= 0 && row < size() && col >= 0 && col < size();
    }

    void output_board() const;
    void make_move(int pos, bool is_black);
    void undo_move(int pos);
    bool test_pos(int pos) const;
    bool check_win() const;
    bool wins_at(int pos, bool is_black) const;

private:
    bool check_five(const std::bitset<MAX_SQUARES>& board) const;
};