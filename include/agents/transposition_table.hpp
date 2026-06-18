#pragma once
#include "../board.hpp"
#include <unordered_map>
#include <cstdint>
#include <random>
#include "../config.hpp"

struct TTEntry {
    float score;
    int depth;
    int flag;
    int best_move;
    
    static const int EXACT = 0;
    static const int LOWERBOUND = 1;
    static const int UPPERBOUND = 2;
};

class TranspositionTable {
public:
    TranspositionTable();
    uint64_t compute_hash(const Board& board, bool is_black_turn) const;
    uint64_t update_hash(uint64_t hash, int move, bool is_black_turn) const;
    bool probe(uint64_t hash, TTEntry& entry) const;
    void store(uint64_t hash, float score, int depth, int flag, int best_move);
    void clear();
    size_t size() const { return table.size(); };

private:
    // use MAX_SQUARES from board.hpp for zobrist arrays
    uint64_t zobrist_black[MAX_SQUARES];  // 361 (19x19)
    uint64_t zobrist_white[MAX_SQUARES];
    uint64_t zobrist_turn;

    std::unordered_map<uint64_t, TTEntry> table;

    void init_zobrist();
};