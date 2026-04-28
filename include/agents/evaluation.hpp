#pragma once
#include "../board.hpp"
#include <array>
#include <vector>

class Evaluator {
public:
    static constexpr int SIZE = 15;
    static constexpr std::array<std::pair<int, int>, 4> directions = {{
        {0, 1}, {1, 0}, {1, 1}, {1, -1}
    }};

    struct LineInfo {
        int count;
        int open_ends;
    };

    int score_pattern(int count, int open_ends) const;
    LineInfo analyze_line(const Board &board, int start_pos, int dx, int dy, bool is_black) const;
    int position_score(int pos) const;
    float evaluate_board(const Board& board) const;
    std::vector<int> get_valid_moves(const Board &board) const;
};