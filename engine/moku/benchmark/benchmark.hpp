#pragma once
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

struct BenchResult
{
    std::string name;
    int reached_depth;
    long long nodes;
    long long tt_hits;
    size_t tt_size;
    double time_s;
    int best_move;
    float score;
};

BenchResult run_position(const std::string& name,
                         const std::vector<std::pair<std::string, bool>>& setup, bool side_to_move,
                         int depth, int time_ms, int board_size = 15,
                         uint64_t max_memory_bytes = 0);

int run_benchmark(int depth, int time_ms, uint64_t max_memory_bytes = 0);