#include "../include/benchmark.hpp"
#include "../include/board.hpp"
#include "../include/minimax/minimax.hpp"
#include "../include/config.hpp"
#include "../include/utils.hpp"

/**
 * This function runs a benchmark for a given board position.
 * It sets up the board according to the provided moves, initializes a MinimaxAgent,
 * and measures the time taken to compute the best move.
 * @param name A descriptive name for the benchmark position.
 * @param setup A vector of pairs representing the moves to set up the board.
 * Each pair consists of a string (the move in algebraic notation) and a boolean (true for black, false for white).
 * @param side_to_move A boolean indicating which side is to move next (true for black, false for white).
 * @param depth The maximum depth for the Minimax search.
 * @param time_ms The time limit in milliseconds for the search. If 0, no time limit is applied.
 * @return A BenchResult struct containing the results of the benchmark, including the best move found, 
 * nodes searched, and time taken.
 */
BenchResult run_position(const std::string &name,
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

/**
 * This is the main function for the benchmark program. It initializes the benchmark environment,
 * runs a series of predefined benchmark positions, and outputs the results in a formatted table.
 * @param argc The number of command-line arguments.
 * @param argv An array of command-line argument strings. The first argument can specify the search depth, 
 * and the second argument can specify the time limit in milliseconds.
 * @return An integer status code (0 for success).
 */
int run_benchmark(int depth, int time_ms) {
    std::cout << "moku (Version " << g_config.version << ")\n"
              << " Running Benchmark with depth " << depth
              << " and time limit " << time_ms << "ms" << std::endl;
    g_config.debug_output = false;

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