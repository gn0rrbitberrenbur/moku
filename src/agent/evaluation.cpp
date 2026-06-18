#include "../../include/agents/evaluation.hpp"
#include "../../include/config.hpp"
#include "../../include/board.hpp"
#include <cmath>

/**
 * This file implements the Evaluation class defined in ../include/agent/evaluation.hpp, which takes 
 * a boardstate and evaluates it.
 * The evaluation function considers various factors such as the number of pieces in a row, open ends, 
 * and positional advantages 
 * to calculate a score for the board state.
 */

// lookup tables that store precomputed values for position scores and pattern scores to speed up evaluation.
namespace {
    int position_scores[400];
    bool tables_initialized = false;
    
    constexpr int DX[] = {1, 0, 1, 1};
    constexpr int DY[] = {0, 1, 1, -1};
    
    void init_tables(int size) {
        if (tables_initialized) return;
        int center = size / 2;
        for (int pos = 0; pos < size * size; pos++) {
            int row = pos / size;
            int col = pos % size;
            int dist = std::abs(row - center) + std::abs(col - center);
            position_scores[pos] = std::max(0, size - dist);
        }
        tables_initialized = true;
    }
}

// pattern scores based on count of pieces in a row and number of open ends
static constexpr int PATTERN_SCORES[6][3] = {
    // [count][open_ends] - open_ends: 0, 1, 2
    {0, 0, 0},           // count = 0
    {0, 0, 3},           // count = 1
    {2, 10, 50},         // count = 2
    {20, 200, 2000},     // count = 3
    {500, 5000, 50000},  // count = 4
    {100000, 100000, 100000}  // count >= 5
};

/**
 * This function calculates the score for a given pattern based on the number of pieces in
 * a row and the number of open ends.
 * @param count ; int ; the number of pieces in a row
 * @param open_ends ; int ; the number of open ends (0, 1, or 2)
 * @return int ; the score for the given pattern based on precomputed values in PATTERN_SCORES
 */
inline int fast_score_pattern(int count, int open_ends) {
    if (count >= 5) return 100000;
    if (count == 0) return 0;
    return PATTERN_SCORES[count][std::min(open_ends, 2)];
}

/**
 * This function performs a fast analysis of a line on the board starting from a given 
 * position and moving in a specified direction.
 * It counts the number of pieces in a row for the specified color and the number of open ends.
 * @param board ; const Board& ; the current state of the board
 * @param pos ; int ; the starting position for the line analysis
 * @param dx ; int ; the x-direction to move (e.g., 1 for horizontal, 0 for vertical)
 * @param dy ; int ; the y-direction to move (e.g., 1 for vertical, 0 for horizontal)
 * @param is_black ; bool ; true if analyzing for black pieces, false for white pieces
 * @return LineInfo ; a struct containing the count of pieces in a row and the number of 
 * open ends for the analyzed line
 */
inline Evaluator::LineInfo Evaluator::analyze_line_fast(
    const Board &board, int pos, int dx, int dy, bool is_black) const {
    
    const int SIZE = g_config.board_size;
    const auto &own = is_black ? board.black : board.white;
    const auto &opp = is_black ? board.white : board.black;
    
    int row = pos / SIZE;
    int col = pos % SIZE;
    int count = 0;
    int open_ends = 0;
    
    for (int i = 0; i < 5; i++) {
        if (row < 0 || row >= SIZE || col < 0 || col >= SIZE) break;
        int p = row * SIZE + col;
        if (!own[p]) {
            if (!opp[p]) open_ends++;
            break;
        }
        count++;
        row += dy;
        col += dx;
    }
    int back_row = (pos / SIZE) - dy;
    int back_col = (pos % SIZE) - dx;
    if (back_row >= 0 && back_row < SIZE && back_col >= 0 && back_col < SIZE) {
        int p = back_row * SIZE + back_col;
        if (!opp[p] && !own[p]) open_ends++;
    }
    return {count, open_ends};
}

/**
 * This function evaluates the current state of the board and returns a score representing how favorable 
 * the position is for the black player.
 * A positive score indicates an advantage for black, while a negative score indicates an advantage for white
 * The evaluation considers the following factors:
 * - Winning conditions (five in a row)
 * - Threats (open threes and fours)
 * - Positional advantages (proximity to the center)
 * @param board ; const Board& ; the current state of the board to be evaluated
 * @return float ; a score representing the favorability of the board state for the black player, where 
 * higher values indicate a better position for black and lower values indicate a better position for white
 */
float Evaluator::evaluate_board(const Board& board) const {
    const int SIZE = g_config.board_size;
    const int SQUARES = SIZE * SIZE;
    
    init_tables(SIZE);
    
    // check for win first
    bool black_moved_last = board.black.count() > board.white.count();
    if (board.check_win()) {
        return black_moved_last ? 100000.0f : -100000.0f;
    }
    
    int score = 0;
    int black_threats = 0;
    int white_threats = 0;
    int black_winning = 0;
    int white_winning = 0;
    
    // combined iteration over occupied squares for both colors to speed up evaluation.
    
    // === BLACK PIECES ===
    for (int pos = 0; pos < SQUARES; pos++) {
        if (!board.black[pos]) continue;
        
        score += position_scores[pos];
        
        int row = pos / SIZE;
        int col = pos % SIZE;

        // check all 4 directions from this piece
        for (int d = 0; d < 4; d++) {
            int dx = DX[d];
            int dy = DY[d];

            // skip if this is not the start of a line (to avoid double counting)
            int prev_row = row - dy;
            int prev_col = col - dx;
            if (prev_row >= 0 && prev_row < SIZE &&
                prev_col >= 0 && prev_col < SIZE) {
                if (board.black[prev_row * SIZE + prev_col]) continue;
            }

            auto info = analyze_line_fast(board, pos, dx, dy, true);

            if (info.count >= 2) {

                bool dead = is_line_dead(board, pos, dx, dy, info.count, true);

                int pattern_score = fast_score_pattern(info.count, info.open_ends);
                if (!dead) {
                    score += pattern_score;
                    if (info.count >= 3 && info.open_ends >= 1) black_threats++;
                    if (info.count >= 4 && info.open_ends >= 1) black_winning++;
                }
            }

            score += score_window(board, pos, dx, dy, true);
        }
    }
    
    // === WHITE PIECES ===
    for (int pos = 0; pos < SQUARES; pos++) {
        if (!board.white[pos]) continue;
        
        score -= position_scores[pos];

        int row = pos / SIZE;
        int col = pos % SIZE;

        for (int d = 0; d < 4; d++) {
            int dx = DX[d];
            int dy = DY[d];

            int prev_row = row - dy;
            int prev_col = col - dx;
            if (prev_row >= 0 && prev_row < SIZE &&
                prev_col >= 0 && prev_col < SIZE) {
                if (board.white[prev_row * SIZE + prev_col]) continue;
            }

            auto info = analyze_line_fast(board, pos, dx, dy, false);

            if (info.count >= 2) {

                bool dead = is_line_dead(board, pos, dx, dy, info.count, false);

                int pattern_score = fast_score_pattern(info.count, info.open_ends);
                if (!dead) {
                    score -= pattern_score;
                    if (info.count >= 3 && info.open_ends >= 1) white_threats++;
                    if (info.count >= 4 && info.open_ends >= 1) white_winning++;
                }
            }

            score -= score_window(board, pos, dx, dy, false);
        }
    }
    
    // check for winning threats
    if (black_winning > 0) score += 30000 * black_winning;
    if (white_winning > 0) score -= 30000 * white_winning;
    
    // tempo bonus
    score += (black_threats - white_threats);
    
    return static_cast<float>(score);
}

/**
 * Checks whether a consecutive line of given length is dead, i.e. blocked by the
 * opponent (or the board edge) on BOTH ends, making five-in-a-row impossible.
 * Efficient: only inspects the two cells directly before and after the run.
 * @param board ; const Board& ; the current state of the board
 * @param pos ; int ; the starting position of the run
 * @param dx ; int ; x-direction of the line
 * @param dy ; int ; y-direction of the line
 * @param count ; int ; number of consecutive own stones in the run
 * @param is_black ; bool ; true for black, false for white
 * @return bool ; true if the line is blocked on both ends (dead), false otherwise
 */
bool Evaluator::is_line_dead(const Board &board, int pos, int dx, int dy,
                                    int count, bool is_black) const {
    const int SIZE = g_config.board_size;
    const auto &own = is_black ? board.black : board.white;
    const auto &opp = is_black ? board.white : board.black;

    int row = pos / SIZE;
    int col = pos % SIZE;

    // cell directly BEFORE the run
    int br = row - dy;
    int bc = col - dx;
    bool front_blocked = true;  // edge counts as blocked
    if (br >= 0 && br < SIZE && bc >= 0 && bc < SIZE) {
        int p = br * SIZE + bc;
        // open if empty and not occupied by opponent
        if (!opp[p] && !own[p]) front_blocked = false;
    }

    // cell directly AFTER the run
    int ar = row + dy * count;
    int ac = col + dx * count;
    bool back_blocked = true;
    if (ar >= 0 && ar < SIZE && ac >= 0 && ac < SIZE) {
        int p = ar * SIZE + ac;
        if (!opp[p] && !own[p]) back_blocked = false;
    }

    return front_blocked && back_blocked;
}

/**
 * Scores a single window of 5 cells starting at pos in direction (dx, dy).
 * A window containing any opponent stone is worthless (returns 0), which is how
 * DEAD lines are pruned. Gaps inside the window do NOT block, so broken patterns
 * like X X _ X X are captured naturally (own_count == 4).
 * @param board ; const Board& ; the current state of the board
 * @param pos ; int ; window start position
 * @param dx ; int ; x-direction
 * @param dy ; int ; y-direction
 * @param is_black ; bool ; true for black, false for white
 * @return int ; the score contribution of this window (0 if blocked or empty)
 */
int Evaluator::score_window(const Board &board, int pos, int dx, int dy,
                                   bool is_black) const {
    const int SIZE = g_config.board_size;
    const auto &own = is_black ? board.black : board.white;
    const auto &opp = is_black ? board.white : board.black;

    int row = pos / SIZE;
    int col = pos % SIZE;

    int own_count = 0;

    for (int i = 0; i < 5; i++) {
        // window must fit fully on the board to be a potential five
        if (row < 0 || row >= SIZE || col < 0 || col >= SIZE) return 0;
        int p = row * SIZE + col;
        if (opp[p]) return 0;       // opponent stone -> dead window
        if (own[p]) own_count++;
        row += dy;
        col += dx;
    }

    if (own_count == 0) return 0;

    // score by number of own stones; gaps are implicitly rewarded since a
    // broken four (X X _ X X) yields own_count == 4 just like a solid four.
    static constexpr int WINDOW_SCORE[6] = {
        0,       // 0 stones
        1,       // 1 stone
        10,      // 2 stones
        120,     // 3 stones
        1200,    // 4 stones (one move from winning, incl. broken fours)
        100000   // 5 stones (win)
    };
    return WINDOW_SCORE[own_count];
}

/**
 * This function calculates the positional score for a given position on the board 
 * based on precomputed values in the position_scores table.
 * The positional score is higher for positions closer to the center of the board, 
 * which are generally more advantageous in Gomoku.
 * @param pos ; int ; the position on the board (0-224) for which to calculate the positional score
 * @return int ; the positional score for the given position, where higher values indicate 
 * a more favorable position (closer to the center) and lower values indicate a 
 * less favorable position (closer to the edges)
 */
int Evaluator::position_score(int pos) const {
    init_tables(g_config.board_size);
    return position_scores[pos];
}

/**
 * This function calculates the priority of a potential move based on its positional score 
 * and tactical considerations.
 * The priority is higher for moves that are closer to the center of the board and for 
 * moves that create or block threats (e.g., open threes and fours).
 * @param board ; const Board& ; the current state of the board
 * @param move ; int ; the position of the potential move to be evaluated
 * @param is_black ; bool ; true if evaluating the move for the black player, false for the white player
 * @return int ; a priority score for the given move, where higher values indicate
 * a more favorable move based on positional and tactical factors
 */
int Evaluator::move_priority(const Board& board, int move, bool is_black) const {
    const int SIZE = g_config.board_size;
    init_tables(SIZE);
    
    int priority = 0;
    
    // 1. position-score (favor center)
    priority += position_scores[move];
    
    // 2. tactical considerations: check if this move creates or blocks threats
    int row = move / SIZE;
    int col = move % SIZE;
    
    const auto& own = is_black ? board.black : board.white;
    const auto& opp = is_black ? board.white : board.black;
    
    for (int d = 0; d < 4; d++) {
        int dx = DX[d];
        int dy = DY[d];
        
        int own_count = 0;
        int opp_count = 0;
        int own_open = 0;
        int opp_open = 0;
        
        // count own pieces in both directions
        for (int dir = -1; dir <= 1; dir += 2) {
            for (int i = 1; i <= 4; i++) {
                int nr = row + dy * i * dir;
                int nc = col + dx * i * dir;
                if (nr < 0 || nr >= SIZE || nc < 0 || nc >= SIZE) break;
                int np = nr * SIZE + nc;
                
                if (own[np]) {
                    own_count++;
                } else if (opp[np]) {
                    break;
                } else {
                    own_open++;
                    break;
                }
            }
        }
        
        // count opponent pieces in both directions
        for (int dir = -1; dir <= 1; dir += 2) {
            for (int i = 1; i <= 4; i++) {
                int nr = row + dy * i * dir;
                int nc = col + dx * i * dir;
                if (nr < 0 || nr >= SIZE || nc < 0 || nc >= SIZE) break;
                int np = nr * SIZE + nc;
                
                if (opp[np]) {
                    opp_count++;
                } else if (own[np]) {
                    break;
                } else {
                    opp_open++;
                    break;
                }
            }
        }
        
        // own winning patterns (high priority)
        if (own_count >= 4) priority += 100000;
        else if (own_count == 3 && own_open >= 1) priority += 10000;
        else if (own_count == 2 && own_open >= 2) priority += 1000;
        
        // opponent threats (must block)
        if (opp_count >= 4) priority += 50000;
        else if (opp_count == 3 && opp_open >= 1) priority += 5000;
        else if (opp_count == 2 && opp_open >= 2) priority += 500;
    }
    return priority;
}

/**
 * This function generates a list of valid moves for the current board state by finding all 
 * empty positions that are within a certain radius of existing pieces.
 * @param board ; const Board& ; the current state of the board
 * @return std::vector<int> ; a vector containing the positions of valid moves, 
 * where each position is an index (0-224) corresponding to an empty square on the board that is near existing pieces
 */
std::vector<int> Evaluator::get_valid_moves(const Board& board) const {
    const int SIZE = g_config.board_size;
    const int SQUARES = SIZE * SIZE;
    std::vector<int> moves;
    
    // edge case: if board is empty, return center move
    if (board.black.count() == 0 && board.white.count() == 0) {
        int center = (SIZE / 2) * SIZE + (SIZE / 2);
        moves.push_back(center);
        return moves;
    }
    
    std::bitset<MAX_SQUARES> candidates;
    
    // find all empty positions that are within a 2-square radius of existing pieces to limit the search space to relevant moves.
    for (int pos = 0; pos < SQUARES; pos++) {
        if (!board.black.test(pos) && !board.white.test(pos)) continue;
        
        int x = pos % SIZE;
        int y = pos / SIZE;
        
        for (int dy = -2; dy <= 2; dy++) {
            for (int dx = -2; dx <= 2; dx++) {
                int nx = x + dx;
                int ny = y + dy;
                if (nx >= 0 && nx < SIZE && ny >= 0 && ny < SIZE) {
                    int npos = ny * SIZE + nx;
                    if (!board.black.test(npos) && !board.white.test(npos)) {
                        candidates.set(npos);
                    }
                }
            }
        }
    }
    
    moves.reserve(candidates.count());
    for (int pos = 0; pos < SQUARES; pos++) {
        if (candidates.test(pos)) {
            moves.push_back(pos);
        }
    }
    return moves;
}

/**
 * ===== LEGACY =====
 * This function is a wrapper for the pattern scoring that calls the faster version of the pattern scoring function.
 * It is provided for compatibility with tests that may call the original score_pattern function, 
 * but it simply delegates to the optimized version.
 * @param count ; int ; the number of pieces in a row
 * @param open_ends ; int ; the number of open ends (0, 1, or 2)
 * @return int ; the score for the given pattern based on precomputed values in PATTERN_SCORES
 */
int Evaluator::score_pattern(int count, int open_ends) const {
    return fast_score_pattern(count, open_ends);
}

/**
 * ===== LEGACY =====
 * This function is a wrapper for the line analysis that calls the faster version of the line analysis function.
 * It is provided for compatibility with tests that may call the original analyze_line function, 
 * but it simply delegates to the optimized version.
 * @param board ; const Board& ; the current state of the board
 * @param pos ; int ; the starting position for the line analysis
 * @param dx ; int ; the x-direction to move (e.g., 1 for horizontal, 0 for vertical)
 * @param dy ; int ; the y-direction to move (e.g., 1 for vertical, 0 for horizontal)
 * @param is_black ; bool ; true if analyzing for black pieces, false for white pieces
 * @return LineInfo ; a struct containing the count of pieces in a row and the number of open ends for the analyzed line, 
 * as calculated by the faster version of the line analysis function
 */
Evaluator::LineInfo Evaluator::analyze_line(const Board& board, int pos, int dx, int dy, bool is_black) const {
    return analyze_line_fast(board, pos, dx, dy, is_black);
}