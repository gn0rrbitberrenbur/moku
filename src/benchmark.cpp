#include "../include/board.hpp"
#include "../include/minimax/minimax.hpp"
#include "../include/config.hpp"
#include "../include/utils.hpp"
#include <chrono>
#include <iostream>
#include <iomanip>
#include <vector>
#include <string>

struct BenchResult {
    std::string name;
    int reached_depth;
    long long nodes;
    long long tt_hits;
    size_t tt_size;
    double time_s;
    int best_move;
    float score;
};

static BenchResult run_position(const std::string &name,
                                const std::vector<std::pair<std::string, bool>> &setup,
                                bool side_to_move,
                                int depth,
                                int time_ms) {
    Board board;
    for (auto &[sq, blk] : setup)
        board.make_move(algebraic_to_index(sq), blk);

    MinimaxAgent agent(depth);
    agent.clear_tt();

    auto t0 = std::chrono::high_resolution_clock::now();
    int move = (time_ms > 0)
                 ? agent.get_best_move_timed_smp(board, side_to_move, time_ms)
                 : agent.get_best_move(board, side_to_move);
    auto t1 = std::chrono::high_resolution_clock::now();

    BenchResult r;
    r.name = name;
    r.reached_depth = agent.get_max_depth();
    r.nodes = agent.get_nodes_searched();
    r.tt_hits = agent.get_tt_hits();
    r.tt_size = 0;
    r.time_s = std::chrono::duration<double>(t1 - t0).count();
    r.best_move = move;
    r.score = 0;
    return r;
}

int main(int argc, char **argv) {
    std::cout << "moku (Version " << g_config.version << ")\n"
              << " Running Benchmark with depth " << (argc > 1 ? argv[1] : "default")
              << " and time limit " << (argc > 2 ? argv[2] : "default") << "ms" << std::endl;
    g_config.debug_output = false;

    int depth = 8;
    int time_ms = 0;
    if (argc > 1) depth = std::stoi(argv[1]);
    if (argc > 2) time_ms = std::stoi(argv[2]);

    std::vector<BenchResult> results;

    results.push_back(run_position(
        "empty_center", { {"H8", true} }, false, depth, time_ms));

    results.push_back(run_position(
        "open_three",
        { {"H8", true}, {"I8", false}, {"H9", true}, {"J9", false}, {"H10", true} },
        false, depth, time_ms));

    results.push_back(run_position(
        "block_four",
        { {"H8", true}, {"A1", false}, {"H9", true}, {"A2", false},
          {"H10", true}, {"A3", false}, {"H11", true} },
        false, depth, time_ms));

    results.push_back(run_position(
        "winning_four",
        { {"H8", false}, {"A1", true}, {"H9", false}, {"A2", true},
          {"H10", false}, {"A3", true}, {"H11", false} },
        false, depth, time_ms));

    std::cout << std::left
              << std::setw(16) << "position"
              << std::setw(8)  << "depth"
              << std::setw(14) << "nodes"
              << std::setw(12) << "nodes/s"
              << std::setw(12) << "tt_hits"
              << std::setw(10) << "tt_rate"
              << std::setw(10) << "time_s"
              << "best\n";
    std::cout << std::string(92, '-') << "\n";

    long long total_nodes = 0;
    double total_time = 0;
    for (auto &r : results) {
        double nps = r.time_s > 0 ? r.nodes / r.time_s : 0;
        double rate = r.nodes > 0 ? (double)r.tt_hits / r.nodes : 0;
        total_nodes += r.nodes;
        total_time += r.time_s;
        std::cout << std::left
                  << std::setw(16) << r.name
                  << std::setw(8)  << r.reached_depth
                  << std::setw(14) << r.nodes
                  << std::setw(12) << (long long)nps
                  << std::setw(12) << r.tt_hits
                  << std::setw(10) << std::fixed << std::setprecision(3) << rate
                  << std::setw(10) << std::fixed << std::setprecision(3) << r.time_s
                  << r.best_move << "\n";
    }

    std::cout << std::string(92, '-') << "\n";
    std::cout << "TOTAL" << "\n"
              << " Nodes searched: " << total_nodes << "\n"
              << " Time taken: " << std::fixed << std::setprecision(3) << total_time << "s" << "\n"
              << " Average Nodes per second: " << (long long)(total_time > 0 ? total_nodes / total_time : 0) << "\n"
              << "\n";
    return 0;
}