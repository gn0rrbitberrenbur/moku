#include "../../include/agents/evaluation.hpp"
#include <cmath>

/**
 * This file implements the Evaluation class defined in ../include/agents/evaluation.hpp.
 * It provides methods to evaluate the board state, analyze patterns, and generate valid moves for the game.
 */

/**
 * Scores a pattern based on the count of pieces and the number of open ends.
 * The scoring is designed to prioritize winning patterns and potential threats.
 * @param count The number of pieces in the pattern.
 * @param open_ends The number of open ends in the pattern.
 * @return int ; The score for the given pattern.
 */
int Evaluator::score_pattern(int count, int open_ends) const {
    if (count >= 5) return 100000;
    if (count == 4) return open_ends == 2 ? 10000 : 1000;
    if (count == 3) return open_ends == 2 ? 1000 : 100;
    if (count == 2) return open_ends == 2 ? 50 : 10;
    return 0;
}

/**
 * Analyzes a line of pieces starting from a given position in a specified direction.
 * It counts the number of pieces in the line and the number of open ends.
 * @param board The current state of the board.
 * @param start_pos The starting position for the analysis.
 * @param dx The change in x-coordinate for the direction.
 * @param dy The change in y-coordinate for the direction.
 * @param is_black A boolean indicating whether to analyze for black pieces or white pieces.
 * @return LineInfo ; struct ; a struct containing the count of pieces and the number of open ends.
 */
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

/**
 * Calculates a positional score for a given position on the board.
 * The score is based on the distance from the center of the board, with closer positions being
 * scored higher to encourage control of the central area.
 * @param pos The position on the board to be scored.
 * @return int ; The positional score for the given position.
 */
int Evaluator::position_score(int pos) const {
    int row = pos / SIZE;
    int col = pos % SIZE;
    int center = SIZE / 2;
    int dist = std::abs(row - center) + std::abs(col - center);
    return std::max(0, (SIZE - dist));
}

/**
 * Evaluates the current state of the board and returns a score representing the advantage for black or white.
 * A positive score indicates an advantage for black, 
 * while a negative score indicates an advantage for white
 * The evaluation considers winning conditions, positional advantages, 
 * and potential threats based on patterns of pieces on the board.
 * @param board The current state of the board to be evaluated.
 * @return float ; The evaluation score for the board state, where positive values favor black and negative values favor white.
 */
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

/**
 * Generates a list of valid moves for the current board state.
 * The function identifies empty positions that are adjacent to occupied positions,
 * and returns a vector of these valid move positions.
 * @param board The current state of the board.
 * @return std::vector<int> ; A list of valid move positions.
 */
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