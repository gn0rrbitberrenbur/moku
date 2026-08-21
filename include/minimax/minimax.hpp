#pragma once
#include "../board.hpp"
#include "evaluation.hpp"
#include "transposition_table.hpp"
#include <chrono>
#include "../config.hpp"
#include <atomic>
#include <vector>

// One root move with its search score and node count (used as visit proxy).
struct RootCandidate {
    int move;
    float score;       // black-positive convention, same as evaluate_board
    long long nodes;   // nodes searched under this move
};

class MinimaxAgent {
public:
    MinimaxAgent(int depth = 6) : max_depth(depth) {}

    // existing function
    int get_best_move(Board &board, bool is_black);
    
    // new function with time limit
    int get_best_move_timed(Board &board, bool is_black, int time_limit_ms);
    int get_best_move_timed_smp(Board &board, bool is_black, int time_limit_ms);
    
    // setters and getters for max depth
    void set_max_depth(int depth) { if (depth > 0) max_depth = depth; }
    int get_max_depth() const { return max_depth; }

    // getter for last root score
    float get_last_root_score() const { return last_root_score; }

    // root move scores of the last completed depth (MultiPV/policy source)
    const std::vector<RootCandidate>& get_last_root_candidates() const { return last_root_candidates; }
    
    // statistics
    long long get_nodes_searched() const { return nodes_searched.load(std::memory_order_relaxed); }
    long long get_tt_hits() const { return tt_hits.load(std::memory_order_relaxed); }
    
    void clear_tt() { tt.clear(); }
    
private:
    int max_depth;
    std::atomic<long long> nodes_searched{0};
    std::atomic<long long> tt_hits{0};
    int current_depth = 0;
    
    // time control
    std::chrono::time_point<std::chrono::high_resolution_clock> search_start;
    int time_limit = 0;
    std::atomic<bool> search_aborted{false};
    
    Evaluator evaluator;
    TranspositionTable tt;
    
    float minimax(Board &board, int depth, float alpha, float beta, 
                  bool is_maximizing, uint64_t hash);

    float last_root_score = 0.0f;
    std::vector<RootCandidate> last_root_candidates;
    
    bool is_time_up();
};