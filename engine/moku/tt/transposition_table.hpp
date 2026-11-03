#pragma once
#include "config.hpp"
#include "game/board.hpp"

#include <array>
#include <atomic>
#include <cstdint>
#include <mutex>
#include <random>
#include <vector>

struct TTEntry
{
    uint64_t key = 0;
    float score = 0;
    int depth = -1;
    int flag = 0;
    int best_move = -1;
    enum
    {
        EXACT,
        LOWERBOUND,
        UPPERBOUND
    };
};

class TranspositionTable
{
    static constexpr size_t TT_BITS = 23;
    static constexpr size_t TT_SIZE = size_t(1) << TT_BITS;
    static constexpr uint64_t TT_MASK = TT_SIZE - 1;
    std::vector<TTEntry> table = std::vector<TTEntry>(TT_SIZE);
    static constexpr size_t LOCK_COUNT = 256;
    mutable std::array<std::mutex, LOCK_COUNT> slot_mutexes;
    std::atomic<size_t> occupied_entries{0};

public:
    TranspositionTable();
    uint64_t compute_hash(const Board& board, bool is_black_turn) const;
    uint64_t update_hash(uint64_t hash, int move, bool is_black_turn) const;
    bool probe(uint64_t hash, TTEntry& entry, int ply) const;
    void store(uint64_t hash, float score, int depth, int flag, int best_move, int ply);
    void clear();
    size_t size() const
    {
        return table.size();
    };
    size_t allocated_bytes() const
    {
        return table.capacity() * sizeof(TTEntry);
    }
    size_t occupancy_permille() const
    {
        return occupied_entries.load(std::memory_order_relaxed) * 1000 / table.size();
    }

private:
    // use MAX_SQUARES from board.hpp for zobrist arrays
    uint64_t zobrist_black[MAX_SQUARES]; // 361 (19x19)
    uint64_t zobrist_white[MAX_SQUARES];
    uint64_t zobrist_turn;

    void init_zobrist();
};