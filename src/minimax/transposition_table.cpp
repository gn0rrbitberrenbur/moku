#include "../../include/minimax/transposition_table.hpp"

/**
 * This file implements the TranspositionTable class defined in ../include/agent/transposition_table.hpp, which
 * provides a hash-based caching mechanism for game states in a Gomoku AI. The class uses Zobrist hashing 
 * to efficiently compute and update hash values for board states, allowing for quick lookups and 
 * storage of evaluated positions.
 */

// initialize transposition table
TranspositionTable::TranspositionTable() {
    init_zobrist();
}

/**
 * This function initializes the Zobrist hashing arrays for both black and white pieces, 
 * as well as the turn hash.
 * It uses a fixed seed for reproducibility, and only initializes up to the current board size 
 * defined in the configuration.
 * @param None
 * @return void
 */
void TranspositionTable::init_zobrist() {
    std::mt19937_64 rng(12345);
    
    int current_squares = g_config.squares();
    
    for (int i = 0; i < current_squares; i++) {
        zobrist_black[i] = rng();
        zobrist_white[i] = rng();
    }
    zobrist_turn = rng();
}

/**
 * This function computes the Zobrist hash for a given board state and player turn. 
 * It iterates only up to the current number of squares defined in the configuration, 
 * ensuring that it works correctly with different board sizes.
 * @param board The current state of the game board.
 * @param is_black_turn A boolean indicating if it's the black player's turn.
 * @return The computed Zobrist hash for the given board state and player turn.
 */
uint64_t TranspositionTable::compute_hash(const Board& board, bool is_black_turn) const {
    uint64_t hash = 0;
    
    // iterate only up to the current number of squares defined in the configuration
    int current_squares = g_config.squares();
    
    for (int i = 0; i < current_squares; i++) {
        if (board.black[i]) hash ^= zobrist_black[i];
        if (board.white[i]) hash ^= zobrist_white[i];
    }
    
    if (is_black_turn) hash ^= zobrist_turn;
    
    return hash;
}

/**
 * This function updates the Zobrist hash for a given move and player turn. 
 * It XORs the hash with the corresponding Zobrist value for the move and player, 
 * as well as the turn hash to reflect the change in player turn.
 * @param hash The current Zobrist hash before the move is made.
 * @param move The position of the move being made.
 * @param is_black A boolean indicating if the move is made by the black player.
 * @return The updated Zobrist hash after applying the move and player turn change.
 */
uint64_t TranspositionTable::update_hash(uint64_t hash, int move, bool is_black) const {
    hash ^= is_black ? zobrist_black[move] : zobrist_white[move];
    hash ^= zobrist_turn; // player change
    return hash;
}

static constexpr float MATE = 90000.0f;

/**
 * This function probes the transposition table for a given hash and retrieves the corresponding entry 
 * if it exists.
 * @param hash The Zobrist hash of the board state to probe.
 * @param entry A reference to a TTEntry structure where the retrieved entry will be stored if found.
 * @return A boolean indicating whether the entry was found in the transposition table.
 */
bool TranspositionTable::probe(uint64_t hash, TTEntry &entry, int ply) const {
    const TTEntry &slot = table[hash & TT_MASK];
    if (slot.key != hash) return false;
    entry = slot;
    if (entry.score >= MATE) entry.score -= ply;
    else if (entry.score <= -MATE) entry.score += ply;
    return true;
}

/**
 * This function stores an entry in the transposition table. It checks if an entry 
 * with the same hash already exists, and only updates it if the new entry has a 
 * greater or equal depth, ensuring that deeper evaluations are not overwritten by shallower ones.
 * @param hash The Zobrist hash of the board state to store.
 * @param score The evaluation score of the board state.
 * @param depth The search depth at which the score was evaluated.
 * @param flag The type of entry (EXACT, LOWERBOUND, UPPERBOUND).
 * @param best_move The best move found for this board state.
 * @return void
 */
void TranspositionTable::store(uint64_t hash, float score, int depth,
                               int flag, int best_move, int ply) {
    if (score >= MATE) score += ply;
    else if (score <= -MATE) score -= ply;
    TTEntry &slot = table[hash & TT_MASK];
    if (slot.key != hash || slot.depth <= depth)
        slot = {hash, score, depth, flag, best_move};
}

/**
 * This function clears the transposition table by simply clearing the underlying unordered_map.
 * @param None
 * @return void
 */
void TranspositionTable::clear() {
    std::fill(table.begin(), table.end(), TTEntry{});
}