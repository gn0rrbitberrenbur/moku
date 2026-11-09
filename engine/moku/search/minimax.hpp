#pragma once
#include "config.hpp"
#include "eval/evaluation.hpp"
#include "game/board.hpp"
#include "tt/transposition_table.hpp"

#include <atomic>
#include <chrono>
#include <mutex>
#include <vector>

// One root move with its search score and node count (used as visit proxy).
struct RootCandidate
{
    int move;
    float score;     // black-positive convention, same as evaluate_board
    long long nodes; // nodes searched under this move
};

class MinimaxAgent
{
public:
    MinimaxAgent(int depth, uint64_t max_memory_bytes = 0)
        : max_depth(depth), tt(max_memory_bytes)
    {
    }

    // existing function
    int get_best_move(Board& board, bool is_black);

    // new function with time limit
    int get_best_move_timed(Board& board, bool is_black, int time_limit_ms);
    int get_best_move_timed_smp(Board& board, bool is_black, int time_limit_ms);

    // setters and getters for max depth
    void set_max_depth(int depth)
    {
        if (depth > 0)
            max_depth = depth;
    }
    int get_max_depth() const
    {
        return max_depth;
    }

    // getter for last root score
    float get_last_root_score() const
    {
        return last_root_score;
    }

    // root move scores of the last completed depth (MultiPV/policy source)
    const std::vector<RootCandidate>& get_last_root_candidates() const
    {
        return last_root_candidates;
    }

    // statistics
    long long get_nodes_searched() const
    {
        return nodes_searched.load(std::memory_order_relaxed);
    }
    long long get_tt_hits() const
    {
        return tt_hits.load(std::memory_order_relaxed);
    }

    void clear_tt()
    {
        tt.clear();
    }

    void set_tt_max_memory(uint64_t max_memory_bytes)
    {
        tt.set_max_memory(max_memory_bytes);
    }
    size_t get_tt_size() const
    {
        return tt.size();
    }
    size_t get_tt_allocated_bytes() const
    {
        return tt.allocated_bytes();
    }

private:
    int max_depth;
    std::atomic<long long> nodes_searched{0};
    std::atomic<long long> tt_hits{0};
    std::atomic<long long> tt_probes{0};
    std::atomic<long long> alpha_beta_cutoffs{0};
    std::atomic<int> depth_completed{0};
    std::atomic<unsigned int> workers_active{0};
    size_t root_candidates_count = 0;

    // time control
    std::chrono::steady_clock::time_point search_start;
    int time_limit = 0;
    long long node_limit = 0;
    std::atomic<bool> search_aborted{false};

    Evaluator evaluator;
    TranspositionTable tt;

    float minimax(Board& board, int depth, float alpha, float beta, bool is_maximizing,
                  uint64_t hash);

    float last_root_score = 0.0f;
    std::vector<RootCandidate> last_root_candidates;

    bool is_time_up();
    bool count_node();

    // mutex for thread-safe access to search metrics
    std::atomic<int> progress_best_move{-1};
    std::atomic<int> progress_depth{0};
    std::atomic<float> progress_score{0.0f};
    std::atomic<bool> progress_score_valid{false};
    std::chrono::steady_clock::time_point last_progress_report;
    std::mutex progress_mutex;

    void reset_search_metrics(int time_limit_ms);
    void finish_search_metrics(int best_move);
    void report_progress(bool finished = false);
};
