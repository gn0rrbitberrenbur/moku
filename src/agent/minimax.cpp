#include "../../include/agents/minimax.hpp"
#include <limits>
#include <algorithm>
#include <iostream>
#include <chrono>

/**
 * This file implements the MinimaxAgent class defined in ../include/agents/minimax.hpp.
 * It provides methods to play games against an agent that uses the minimax algorithm with
 * alpha-beta pruning to determine the best moves.
 */

/**
 * The minimax function recursively evaluates the game tree to determine the best move for the current player.
 * It uses alpha-beta pruning to optimize the search by eliminating branches that won't influence the final decision
 * @param board The current state of the board.
 * @param depth The current depth in the game tree.
 * @param alpha The best score that the maximizing player can guarantee at this level or above.
 * @param beta The best score that the minimizing player can guarantee at this level or above.
 * @param is_maximizing A boolean indicating whether the current player is the maximizing player (true for black, false for white).
 * @return float ; The evaluation score for the current board state, where positive values favor black and negative values favor white.
 */
float MinimaxAgent::minimax(Board &board, int depth, float alpha, float beta, bool is_maximizing)
{
    nodes_searched++;
    current_depth = max_depth - depth;

    if (board.check_win()) {
        return is_maximizing ? -100000.0f : 100000.0f;
    }

    if (depth == 0) {
        return evaluator.evaluate_board(board);
    }

    std::vector<int> moves = evaluator.get_valid_moves(board);

    if (moves.empty()) {
        return 0.0f;
    }

    if (is_maximizing) {
        float max_eval = -std::numeric_limits<float>::infinity();

        for (int move : moves) {
            board.make_move(move, true);
            float eval = minimax(board, depth - 1, alpha, beta, false);
            board.undo_move(move);

            max_eval = std::max(max_eval, eval);
            alpha = std::max(alpha, eval);

            if (beta <= alpha) {
                break;
            }
        }
        return max_eval;
    } else {
        float min_eval = std::numeric_limits<float>::infinity();

        for (int move : moves) {
            board.make_move(move, false);
            float eval = minimax(board, depth - 1, alpha, beta, true);
            board.undo_move(move);

            min_eval = std::min(min_eval, eval);
            beta = std::min(beta, eval);

            if (beta <= alpha) {
                break;
            }
        }
        return min_eval;
    }
}

/**
 * Determines the best move for the current player using the minimax algorithm with alpha-beta pruning.
 * It evaluates all valid moves and selects the one with the best score based on the minimax evaluation.
 * The function also measures the time taken to compute, the number of nodes searched, and the best score found for the move.
 * @param board The current state of the board.
 * @param is_black A boolean indicating whether the current player is black (true) or white (false).
 * @return int ; The position of the best move for the current player, or -1 if no valid moves are available.
 */
int MinimaxAgent::get_best_move(Board &board, bool is_black)
{
    nodes_searched = 0;
    current_depth = 0;

    auto begin = std::chrono::high_resolution_clock::now();
    std::vector<int> moves = evaluator.get_valid_moves(board);
    
    if (moves.empty()) {
        return -1;
    }

    int best_move = moves[0];
    float best_score = is_black ? -std::numeric_limits<float>::infinity() 
                                : std::numeric_limits<float>::infinity();
    
    float alpha = -std::numeric_limits<float>::infinity();
    float beta = std::numeric_limits<float>::infinity();

    for (int move : moves) {
        board.make_move(move, is_black);
        float score = minimax(board, max_depth - 1, alpha, beta, !is_black);
        board.undo_move(move);

        if (is_black && score > best_score) {
            best_score = score;
            best_move = move;
            alpha = std::max(alpha, score);
        } else if (!is_black && score < best_score) {
            best_score = score;
            best_move = move;
            beta = std::min(beta, score);
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration = end - begin;


    std::cout << "Depth: " << max_depth 
              << " | Nodes searched: " << nodes_searched 
              << " | Best Score: " << best_score
              << " | Time taken: " << duration.count() << " s" << std::endl;

    return best_move;
}