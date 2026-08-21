#include "../../include/minimax/minimax.hpp"
#include <limits>
#include <algorithm>
#include <iostream>
#include <chrono>
#include <thread>
#include <atomic>
#include <mutex>

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
        last_root_score = 0.0f;
        last_root_candidates = { RootCandidate{ moves[0], 0.0f, 0 } };
        return moves[0];
    }
    
    // set best_move to first move as fallback
    best_move = moves[0];
    last_root_candidates.clear();
    
    // initial hash
    uint64_t hash = tt.compute_hash(board, is_black);
    
    if (g_config.debug_output == true) {
        std::cerr << "[info] Starting iterative deepening (limit: " 
                << time_limit_ms << "ms, moves: " << moves.size() << ")" << std::endl;
    }
    
    nodes_searched = 0;
    tt_hits = 0;
     
    // starting iterative deepening loop
    for (int depth = 1; depth <= max_depth; depth++) {
        search_aborted = false;
        
        auto depth_start = std::chrono::high_resolution_clock::now();

        {
            std::vector<std::pair<int, int>> scored;
            scored.reserve(moves.size());
            for (int m : moves)
                scored.emplace_back(evaluator.move_priority(board, m, is_black), m);
            std::sort(scored.begin(), scored.end(),
                      [](const std::pair<int, int> &a, const std::pair<int, int> &b) {
                          return a.first > b.first;
                      });
            for (size_t k = 0; k < moves.size(); k++)
                moves[k] = scored[k].second;
        }

        int current_best_move = moves[0];
        float current_best_score = is_black ? -std::numeric_limits<float>::infinity() 
                                            : std::numeric_limits<float>::infinity();
        
        float alpha = -std::numeric_limits<float>::infinity();
        float beta = std::numeric_limits<float>::infinity();
        
        std::vector<RootCandidate> depth_candidates;
        depth_candidates.reserve(moves.size());

        for (size_t i = 0; i < moves.size(); i++) {
            int move = moves[i];
            
            if (is_time_up()) {
                search_aborted = true;
                break;
            }
            
            uint64_t new_hash = tt.update_hash(hash, move, is_black);
            
            long long nodes_before = nodes_searched.load(std::memory_order_relaxed);
            board.make_move(move, is_black);
            float score;
            if (board.wins_at(move, is_black)) {
                score = is_black ? 100000.0f - (max_depth - depth)
                                 : -100000.0f + (max_depth - depth);
            } else {
                score = minimax(board, depth - 1, alpha, beta, !is_black, new_hash);
            }
            board.undo_move(move);
            
            if (search_aborted) {
                break;
            }
            
            const long long move_nodes =
                nodes_searched.load(std::memory_order_relaxed) - nodes_before;
            depth_candidates.push_back(RootCandidate{ move, score, move_nodes });

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
            last_root_candidates = std::move(depth_candidates);
            
            if (g_config.debug_output == true) {
                std::cerr << "[info] Depth: " << depth 
                        << " | Score: " << best_score
                        << " | Best Move: " << best_move
                        << " | Nodes: " << nodes_searched 
                        << " | Time: " << (int)(depth_time * 1000) << "ms"
                        << std::endl;
            }
            
            // if winning move found, stop immediately
            if (std::abs(best_score) >= 90000) {
                if (g_config.debug_output == true) {
                    std::cerr << "[info] Winning move found at depth " << depth << std::endl;
                }
                break;
            }
        } else {
            if (g_config.debug_output == true) {
                std::cerr << "[info] Depth " << depth << " aborted (time)" << std::endl;
            }
            break;
        }
        
        // check if the time is almost up before starting the next depth
        auto total_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::high_resolution_clock::now() - search_start
        ).count();
        
        if (total_elapsed * 10 > time_limit_ms) {
            if (g_config.debug_output == true) {
                std::cerr << "[info] Time limit reached after depth " << depth << std::endl;
            }
            break;
        }
    }
    
    // print info
    auto total_end = std::chrono::high_resolution_clock::now();
    double total_time = std::chrono::duration<double>(total_end - search_start).count();
    
    if (g_config.debug_output == true) {
        std::cerr << "[info] Search complete: Depth: " << completed_depth 
                << " | Best Move: " << best_move
                << " | Time: " << (int)(total_time * 1000) << "ms"
                << " | Nodes: " << nodes_searched
                << " | TT Hits: " << tt_hits
                << " | TT Size: " << tt.size()
                << std::endl;
    }

    
    time_limit = 0;  // reset
    last_root_score = best_score;
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

    int ply = max_depth - depth;
    
    nodes_searched.fetch_add(1, std::memory_order_relaxed);
    current_depth = max_depth - depth;

    float original_alpha = alpha;

    // lookup in transposition table
    TTEntry entry = {0, -1, TTEntry::EXACT, -1};
    bool tt_hit = tt.probe(hash, entry, ply);

    if (tt_hit) {
        tt_hits.fetch_add(1, std::memory_order_relaxed);
        
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

    if (depth == 0) {
        float eval = evaluator.evaluate_board(board);
        tt.store(hash, eval, 0, TTEntry::EXACT, -1, ply);
        return eval;
    }

    std::vector<int> moves = evaluator.get_valid_moves(board);

    if (moves.empty()) {
        return 0.0f;
    }

    int tt_move = entry.best_move;

    {
        std::vector<std::pair<int, int>> scored;
        scored.reserve(moves.size());
        for (int m : moves) {
            if (m == tt_move) continue;
            scored.emplace_back(evaluator.move_priority(board, m, is_maximizing), m);
        }
        std::sort(scored.begin(), scored.end(),
                  [](const auto &a, const auto &b) { return a.first > b.first; });
        moves.clear();
        if (tt_move >= 0) moves.push_back(tt_move);
        for (auto &p : scored) moves.push_back(p.second);
    }

    int best_move = moves[0];

    if (is_maximizing) {
        float max_eval = -std::numeric_limits<float>::infinity();

        for (int move : moves) {
            if (search_aborted) break;

            uint64_t new_hash = tt.update_hash(hash, move, true);

            board.make_move(move, true);
            float eval;
            if (board.wins_at(move, true)) {
                eval = 100000.0f - (max_depth - depth);
            } else {
                eval = minimax(board, depth - 1, alpha, beta, false, new_hash);
            }
            board.undo_move(move);

            if (eval > max_eval) {
                max_eval = eval;
                best_move = move;
            }
            alpha = std::max(alpha, eval);

            if (beta <= alpha) break;
        }
        if (!search_aborted) {
            int flag = (max_eval <= original_alpha) ? TTEntry::UPPERBOUND :
                       (max_eval >= beta) ? TTEntry::LOWERBOUND : TTEntry::EXACT;
            tt.store(hash, max_eval, depth, flag, best_move, ply);
        }
        return max_eval;
    } else {
        float min_eval = std::numeric_limits<float>::infinity();

        for (int move : moves) {
            if (search_aborted) break;

            uint64_t new_hash = tt.update_hash(hash, move, false);

            board.make_move(move, false);
            float eval;
            if (board.wins_at(move, false)) {
                eval = -100000.0f + (max_depth - depth);
            } else {
                eval = minimax(board, depth - 1, alpha, beta, true, new_hash);
            }
            board.undo_move(move);

            if (eval < min_eval) {
                min_eval = eval;
                best_move = move;
            }
            beta = std::min(beta, eval);

            if (beta <= alpha) break;
        }
        if (!search_aborted) {
            int flag = (min_eval <= original_alpha) ? TTEntry::UPPERBOUND :
                       (min_eval >= beta) ? TTEntry::LOWERBOUND : TTEntry::EXACT;
            tt.store(hash, min_eval, depth, flag, best_move, ply);
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
    if (g_config.debug_output == true) {
        std::cerr << "[info] Search complete: Depth: " << max_depth 
                << " | Best Move: " << best_move
                << " | Time: " << (int)(duration.count() * 1000) << "ms"
                << " | Nodes: " << nodes_searched
                << " | TT Hits: " << tt_hits
                << " | TT Size: " << tt.size()
                << std::endl;
    }
    last_root_score = best_score;
    return best_move;
}

/**
 * Lazy SMP: run several iterative-deepening searches in parallel that share the
 * transposition table. The main thread (id 0) determines the returned move; the
 * helper threads only warm/fill the shared TT so that the main thread searches
 * deeper. Helper threads stagger their start depth to widen the search.
 * @param board The current board state.
 * @param is_black Whether the side to move is black.
 * @param time_limit_ms Time budget in milliseconds.
 * @return The best move found by the main thread.
 */
int MinimaxAgent::get_best_move_timed_smp(Board &board, bool is_black, int time_limit_ms) {
    unsigned int threads = g_config.cores;
    if (threads <= 1) {
        return get_best_move_timed(board, is_black, time_limit_ms);
    }

    // shared state
    search_start = std::chrono::high_resolution_clock::now();
    time_limit = time_limit_ms;
    search_aborted = false;
    nodes_searched = 0;
    tt_hits = 0;

    std::vector<int> root_moves = evaluator.get_valid_moves(board);
    if (root_moves.empty()) return -1;
    if (root_moves.size() == 1) return root_moves[0];

    std::atomic<int> shared_best_move{root_moves[0]};
    std::atomic<int> completed_depth{0};
    std::mutex result_mutex;

    auto worker = [&](unsigned int thread_id) {
        // each worker gets its own board copy so make/undo don't collide
        Board local = board;
        std::vector<int> moves = root_moves;
        uint64_t root_hash = tt.compute_hash(local, is_black);

        int local_best = moves[0];

        // helper threads skew starting depth to diversify the search
        int start_depth = 1 + (thread_id % 2);

        for (int depth = start_depth; depth <= max_depth; depth++) {
            if (is_time_up()) { search_aborted = true; break; }
            if (search_aborted) break;

            // order moves by priority (shared TT already influences deeper plies)
            {
                std::vector<std::pair<int, int>> scored;
                scored.reserve(moves.size());
                for (int m : moves)
                    scored.emplace_back(evaluator.move_priority(local, m, is_black), m);
                std::sort(scored.begin(), scored.end(),
                          [](const auto &a, const auto &b) { return a.first > b.first; });
                for (size_t k = 0; k < moves.size(); k++)
                    moves[k] = scored[k].second;
            }

            int current_best_move = moves[0];
            float current_best_score = is_black
                ? -std::numeric_limits<float>::infinity()
                :  std::numeric_limits<float>::infinity();

            float alpha = -std::numeric_limits<float>::infinity();
            float beta  =  std::numeric_limits<float>::infinity();
            bool aborted_this_depth = false;

            for (int move : moves) {
                if (is_time_up()) { search_aborted = true; aborted_this_depth = true; break; }
                if (search_aborted) { aborted_this_depth = true; break; }

                uint64_t new_hash = tt.update_hash(root_hash, move, is_black);
                local.make_move(move, is_black);
                float score;
                if (local.wins_at(move, is_black)) {
                    score = is_black ? 100000.0f - (max_depth - depth)
                                     : -100000.0f + (max_depth - depth);
                } else {
                    score = minimax(local, depth - 1, alpha, beta, !is_black, new_hash);
                }
                local.undo_move(move);

                if (search_aborted) { aborted_this_depth = true; break; }

                if (is_black) {
                    if (score > current_best_score) {
                        current_best_score = score; current_best_move = move;
                    }
                    alpha = std::max(alpha, score);
                } else {
                    if (score < current_best_score) {
                        current_best_score = score; current_best_move = move;
                    }
                    beta = std::min(beta, score);
                }
            }

            if (aborted_this_depth) break;

            local_best = current_best_move;

            {
                std::lock_guard<std::mutex> lock(result_mutex);
                if (depth > completed_depth.load()) {
                    completed_depth = depth;
                    shared_best_move = local_best;
                }
            }

            if (thread_id == 0 && g_config.debug_output) {
                auto now = std::chrono::high_resolution_clock::now();
                double elapsed = std::chrono::duration<double>(now - search_start).count();
                std::lock_guard<std::mutex> lock(result_mutex);
                std::cerr << "[info] depth=" << depth
                          << " | best=" << current_best_move
                          << " | score=" << current_best_score
                          << " | time=" << (int)(elapsed * 1000) << "ms"
                          << " | nodes=" << nodes_searched.load(std::memory_order_relaxed)
                          << " | tt_hits=" << tt_hits.load(std::memory_order_relaxed)
                          << std::endl;
            }

            if (std::abs(current_best_score) >= 90000) {
                search_aborted = true;
                break;
            }
        }
    };

    std::vector<std::thread> pool;
    pool.reserve(threads - 1);
    for (unsigned int t = 1; t < threads; t++)
        pool.emplace_back(worker, t);

    worker(0);  // run one search on the calling thread

    search_aborted = true;  // signal helpers to stop
    for (auto &th : pool) th.join();

    if (g_config.debug_output) {
        auto total_end = std::chrono::high_resolution_clock::now();
        double total_time = std::chrono::duration<double>(total_end - search_start).count();
        std::cerr << "[info] SMP search: threads=" << threads
                  << " | depth=" << completed_depth.load()
                  << " | best=" << shared_best_move.load()
                  << " | time=" << (int)(total_time * 1000) << "ms"
                  << " | nodes=" << nodes_searched
                  << " | tt_hits=" << tt_hits
                  << std::endl;
    }

    time_limit = 0;
    return shared_best_move.load();
}