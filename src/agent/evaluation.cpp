#include "../../include/agents/evaluation.hpp"
#include <cmath>

int Evaluator::score_pattern(int count, int open_ends) const {
    if (count >= 5) return 100000;
    if (count == 4) return open_ends == 2 ? 10000 : 1000;
    if (count == 3) return open_ends == 2 ? 1000 : 100;
    if (count == 2) return open_ends == 2 ? 50 : 10;
    return 0;
}

Evaluator::LineInfo Evaluator::analyze_line(const Board &board, int start_pos, int dx, int dy, bool is_black) const {
    const auto &own = is_black ? board.black : board.white;
    const auto &opp = is_black ? board.white : board.black;

    int start_row = start_pos / SIZE;
    int start_col = start_pos % SIZE;

    int count = 0;
    int open_ends = 0;

    int row = start_row, col = start_col;

    while (row >= 0 && row < SIZE && col >= 0 && col < SIZE) {
        int pos = row * SIZE + col;
        if (own[pos]) count++;
        else break;
        row += dy;
        col += dx;
    }

    if (row >= 0 && row < SIZE && col >= 0 && col < SIZE) {
        int pos = row * SIZE + col;
        if (!opp[pos]) open_ends++;
    }

    row = start_row - dy;
    col = start_col - dx;

    while (row >= 0 && row < SIZE && col >= 0 && col < SIZE) {
        int pos = row * SIZE + col;
        if (own[pos]) count++;
        else break;
        row -= dy;
        col -= dx;
    }

    if (row >= 0 && row < SIZE && col >= 0 && col < SIZE) {
        int pos = row * SIZE + col;
        if (!opp[pos]) open_ends++;
    }

    return {count, open_ends};
}

int Evaluator::position_score(int pos) const {
    int row = pos / SIZE;
    int col = pos % SIZE;
    int center = SIZE / 2;
    int dist = std::abs(row - center) + std::abs(col - center);
    return std::max(0, (SIZE - dist));
}

float Evaluator::evaluate_board(const Board& board) const {
    if (board.check_win()) {
        return board.black.count() > board.white.count() ? 100000.0f : -100000.0f;
    }
    
    float score = 0;

    for (int pos = 0; pos < SIZE * SIZE; pos++) {
        if (board.black[pos]) {
            score += position_score(pos);
            for (auto [dx, dy] : directions) {
                auto info = analyze_line(board, pos, dx, dy, true);
                score += score_pattern(info.count, info.open_ends);
            }
        }
        if (board.white[pos]) {
            score -= position_score(pos);
            for (auto [dx, dy] : directions) {
                auto info = analyze_line(board, pos, dx, dy, false);
                score -= score_pattern(info.count, info.open_ends);
            }
        }
    }
    return score;
}

std::vector<int> Evaluator::get_valid_moves(const Board &board) const {
    std::vector<int> moves;
    auto occupied = board.black | board.white;

    if (occupied.none()) {
        moves.push_back((SIZE / 2) * SIZE + (SIZE / 2));
        return moves;
    }

    std::bitset<SIZE * SIZE> candidates;

    for (int pos = 0; pos < SIZE * SIZE; pos++) {
        if (occupied[pos]) continue;

        int row = pos / SIZE;
        int col = pos % SIZE;

        for (int dr = -2; dr <= 2; dr++) {
            for (int dc = -2; dc <= 2; dc++) {
                int nr = row + dr;
                int nc = col + dc;

                if (nr >= 0 && nr < SIZE && nc >= 0 && nc < SIZE) {
                    int npos = nr * SIZE + nc;
                    if (occupied[npos]) {
                        candidates.set(pos);
                    }
                }
            }
        }
    }

    for (int pos = 0; pos < SIZE * SIZE; pos++) {
        if (candidates[pos]) {
            moves.push_back(pos);
        }
    }
    return moves;
}