#pragma once
#include "../board.hpp"
#include "evaluation.hpp"
#include "transposition_table.hpp"
#include <chrono>
#include "../config.hpp"

class MinimaxAgent {
public:
    MinimaxAgent(int depth = 6) : max_depth(depth) {}

    // existing function
    int get_best_move(Board &board, bool is_black);
    
    // new function with time limit
    int get_best_move_timed(Board &board, bool is_black, int time_limit_ms);
    
    // setters and getters for max depth
    void set_max_depth(int depth) { if (depth > 0) max_depth = depth; }
    int get_max_depth() const { return max_depth; }
    
    // statistics
    int get_nodes_searched() const { return nodes_searched; }
    int get_tt_hits() const { return tt_hits; }
    
    void clear_tt() { tt.clear(); }
    
private:
    int max_depth;
    int nodes_searched = 0;
    int current_depth = 0;
    int tt_hits = 0;
    
    // time control
    std::chrono::time_point<std::chrono::high_resolution_clock> search_start;
    int time_limit = 0;
    bool search_aborted = false;
    
    Evaluator evaluator;
    TranspositionTable tt;
    
    float minimax(Board &board, int depth, float alpha, float beta, 
                  bool is_maximizing, uint64_t hash);
    
    bool is_time_up();
};