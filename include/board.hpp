#pragma once
#include <bitset>
#include <iostream>

class Board {
public:
    static constexpr int SIZE = 15;
    std::bitset<SIZE * SIZE> black;
    std::bitset<SIZE * SIZE> white;

    void output_board() const;
    void make_move(int pos, bool is_black);
    void undo_move(int pos);
    bool test_pos(int pos) const;
    bool check_win() const;

private:
    bool check_five(const std::bitset<SIZE * SIZE>& board) const;
};