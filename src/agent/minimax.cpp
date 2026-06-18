#include "../../include/agents/minimax.hpp"
#include <limits>
#include <algorithm>
#include <iostream>
#include <chrono>

/**
 * This file implements the Minimax class defined in ../include/agent/minimax.hpp, which uses the minimax algorithm 
 * with alpha-beta pruning to determine the best move for a given board state.
 */

/**
 * This function checks if the time limit for the search has been reached.
 * @param None
 * @return bool Returns true if the time limit has been reached, false otherwise.
 */
bool MinimaxAgent::is_time_up() {
    if (time_limit <= 0) return false;
    
    auto now = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - search_start
    ).count();
    return elapsed >= time_limit;
}

/**
 * This function initiates the iterative deepening search to find the best move 
 * for the current board state within a specified time limit.
 * It uses a loop to incrementally increase the search depth and calls the minimax function
 * to evaluate moves at each depth. The search can be aborted if the time limit is reached.
 * @param board A reference to the current Board object representing the game state.
 * @param is_black A boolean indicating whether the current player is black (true) or white (false).
 * @param time_limit_ms The time limit for the search in milliseconds.
 * @return int Returns the index of the best move found within the time limit, 
 * or -1 if no valid moves are available.
 */
int MinimaxAgent::get_best_move_timed(Board &board, bool is_black, int time_limit_ms) {
    search_start = std::chrono::high_resolution_clock::now();
    time_limit = time_limit_ms;
    search_aborted = false;
    
    int best_move = -1;
    float best_score = 0;
    int completed_depth = 0;
    
    // get moves
    std::vector<int> moves = evaluator.get_valid_moves(board);
    
    if (moves.empty()) {
        return -1;
    }
    
    // if only one move possible, return immediately
    if (moves.size() == 1) {
        return moves[0];
    }
    
    // set best_move to first move as fallback
    best_move = moves[0];
    
    // initial hash
    uint64_t hash = tt.compute_hash(board, is_black);
    
    std::cerr << "[info] Starting iterative deepening (limit: " 
              << time_limit_ms << "ms, moves: " << moves.size() << ")" << std::endl;
    
    // starting iterative deepening loop
    for (int depth = 1; depth <= max_depth; depth++) {
        nodes_searched = 0;
        tt_hits = 0;
        search_aborted = false;
        
        auto depth_start = std::chrono::high_resolution_clock::now();
        
        // order moves by priority for this depth
        std::sort(moves.begin(), moves.end(), [&](int a, int b) {
            return evaluator.move_priority(board, a, is_black) > 
                   evaluator.move_priority(board, b, is_black);
        });
        
        int current_best_move = moves[0];
        float current_best_score = is_black ? -std::numeric_limits<float>::infinity() 
                                            : std::numeric_limits<float>::infinity();
        
        float alpha = -std::numeric_limits<float>::infinity();
        float beta = std::numeric_limits<float>::infinity();
        
        for (size_t i = 0; i < moves.size(); i++) {
            int move = moves[i];
            
            if (is_time_up()) {
                search_aborted = true;
                break;
            }
            
            uint64_t new_hash = tt.update_hash(hash, move, is_black);
            
            board.make_move(move, is_black);
            float score = minimax(board, depth - 1, alpha, beta, !is_black, new_hash);
            board.undo_move(move);
            
            if (search_aborted) {
                break;
            }
            
            if (is_black) {
                if (score > current_best_score) {
                    current_best_score = score;
                    current_best_move = move;
                }
                alpha = std::max(alpha, score);
            } else {
                if (score < current_best_score) {
                    current_best_score = score;
                    current_best_move = move;
                }
                beta = std::min(beta, score);
            }
        }
        
        auto depth_end = std::chrono::high_resolution_clock::now();
        double depth_time = std::chrono::duration<double>(depth_end - depth_start).count();
        
        // only if search was not aborted, update best move and print info
        if (!search_aborted) {
            best_move = current_best_move;
            best_score = current_best_score;
            completed_depth = depth;
            
            std::cerr << "[info] Depth: " << depth 
                      << " | Score: " << best_score
                      << " | Best Move: " << best_move
                      << " | Nodes: " << nodes_searched 
                      << " | Time: " << (int)(depth_time * 1000) << "ms"
                      << std::endl;
            
            // if winning move found, stop immediately
            if (std::abs(best_score) >= 90000) {
                std::cerr << "[info] Winning move found!" << std::endl;
                break;
            }
        } else {
            std::cerr << "[info] Depth " << depth << " aborted (time)" << std::endl;
            break;
        }
        
        // check if the time is almost up before starting the next depth
        auto total_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::high_resolution_clock::now() - search_start
        ).count();
        
        if (total_elapsed * 10 > time_limit_ms) {
            std::cerr << "[info] Not enough time for depth " << (depth + 1) << std::endl;
            break;
        }
    }
    
    // print info
    auto total_end = std::chrono::high_resolution_clock::now();
    double total_time = std::chrono::duration<double>(total_end - search_start).count();
    
    std::cerr << "[info] Search complete: Depth: " << completed_depth 
              << " | Best Move: " << best_move
              << " | Time: " << (int)(total_time * 1000) << "ms"
              << " | Nodes: " << nodes_searched
              << " | TT Hits: " << tt_hits
              << " | TT Size: " << tt.size()
              << std::endl;
    
    time_limit = 0;  // reset
    return best_move;
}

/**
 * This function implements the minimax algorithm with alpha-beta pruning.
 * It recursively evaluates the game tree to determine the best move for the current player.
 * @param board ; int ; A reference to the current Board object representing the game state.
 * @param depth ; int ; The current depth of the search.
 * @param alpha ; float ; The alpha value for alpha-beta pruning.
 * @param beta ; float ; The beta value for alpha-beta pruning.
 * @param is_maximizing ; bool ; A boolean indicating whether the current player is the maximizing player.
 * @param hash The Zobrist hash of the current board state.
 */
float MinimaxAgent::minimax(Board &board, int depth, float alpha, float beta, 
                            bool is_maximizing, uint64_t hash)
{
    // check if theres enough time left every 1000 nodes
    if (time_limit > 0 && nodes_searched % 1000 == 0) {
        if (is_time_up()) {
            search_aborted = true;
            return 0;
        }
    }
    if (search_aborted) {
        return 0;
    }
    
    nodes_searched++;
    current_depth = max_depth - depth;
    
    float original_alpha = alpha;

    // lookup in transposition table
    TTEntry entry = {0, -1, TTEntry::EXACT, -1};  // score, depth, flag, best_move
    bool tt_hit = tt.probe(hash, entry);

    if (tt_hit) {
        tt_hits++;
        
        if (entry.depth >= depth) {
            if (entry.flag == TTEntry::EXACT) {
                return entry.score;
            }
            if (entry.flag == TTEntry::LOWERBOUND) {
                alpha = std::max(alpha, entry.score);
            }
            if (entry.flag == TTEntry::UPPERBOUND) {
                beta = std::min(beta, entry.score);
            }
            
            if (alpha >= beta) {
                return entry.score;
            }
        }
    }
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

    // move ordering: if tt entry has a move, try it first
    int tt_move = entry.best_move;
    if (tt_move >= 0) {
        auto it = std::find(moves.begin(), moves.end(), tt_move);
        if (it != moves.end()) {
            moves.erase(it);
            moves.insert(moves.begin(), tt_move);
        }
    }

    // order moves by priority
    size_t start_idx = (tt_move >= 0) ? 1 : 0;
    std::sort(moves.begin() + start_idx, moves.end(), [&](int a, int b) {
        return evaluator.move_priority(board, a, is_maximizing) > 
               evaluator.move_priority(board, b, is_maximizing);
    });

    int best_move = moves[0];

    if (is_maximizing) {
        float max_eval = -std::numeric_limits<float>::infinity();

        for (int move : moves) {
            if (search_aborted) break;
            
            uint64_t new_hash = tt.update_hash(hash, move, true);
            
            board.make_move(move, true);
            float eval = minimax(board, depth - 1, alpha, beta, false, new_hash);
            board.undo_move(move);

            if (eval > max_eval) {
                max_eval = eval;
                best_move = move;
            }
            alpha = std::max(alpha, eval);

            if (beta <= alpha) {
                break;
            }
        }
        if (!search_aborted) {
            int flag = (max_eval <= original_alpha) ? TTEntry::UPPERBOUND :
                       (max_eval >= beta) ? TTEntry::LOWERBOUND : TTEntry::EXACT;
            tt.store(hash, max_eval, depth, flag, best_move);
        }
        return max_eval;
    } else {
        float min_eval = std::numeric_limits<float>::infinity();

        for (int move : moves) {
            if (search_aborted) break;
            
            uint64_t new_hash = tt.update_hash(hash, move, false);
            
            board.make_move(move, false);
            float eval = minimax(board, depth - 1, alpha, beta, true, new_hash);
            board.undo_move(move);

            if (eval < min_eval) {
                min_eval = eval;
                best_move = move;
            }
            beta = std::min(beta, eval);

            if (beta <= alpha) {
                break;
            }
        }
        if (!search_aborted) {
            int flag = (min_eval <= original_alpha) ? TTEntry::UPPERBOUND :
                       (min_eval >= beta) ? TTEntry::LOWERBOUND : TTEntry::EXACT;
            tt.store(hash, min_eval, depth, flag, best_move);
        }
        return min_eval;
    }
}

/**
 * Determines the best move for the current player using the minimax algorithm.
 * @param board The current state of the board.
 * @param is_black A boolean indicating whether the current player is black.
 * @return int ; The position of the best move, or -1 if no valid moves.
 */
int MinimaxAgent::get_best_move(Board &board, bool is_black)
{
    nodes_searched = 0;
    current_depth = 0;
    tt_hits = 0;

    auto begin = std::chrono::high_resolution_clock::now();
    std::vector<int> moves = evaluator.get_valid_moves(board);
    
    if (moves.empty()) {
        return -1;
    }

    // calculate initial hash
    uint64_t hash = tt.compute_hash(board, is_black);

    std::sort(moves.begin(), moves.end(), [&](int a, int b) {
        return evaluator.position_score(a) > evaluator.position_score(b);
    });

    int best_move = moves[0];
    float best_score = is_black ? -std::numeric_limits<float>::infinity() 
                                : std::numeric_limits<float>::infinity();
    
    float alpha = -std::numeric_limits<float>::infinity();
    float beta = std::numeric_limits<float>::infinity();

    for (int move : moves) {
        uint64_t new_hash = tt.update_hash(hash, move, is_black);
        
        board.make_move(move, is_black);
        float score = minimax(board, max_depth - 1, alpha, beta, !is_black, new_hash);
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

    std::cerr << "Depth: " << max_depth 
              << " | Nodes: " << nodes_searched 
              << " | TT Hits: " << tt_hits
              << " | TT Size: " << tt.size()
              << " | Score: " << best_score
              << " | Time: " << duration.count() << " s" << std::endl;

    return best_move;
}