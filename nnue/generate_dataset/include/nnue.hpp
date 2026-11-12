#pragma once
#include "game/board.hpp"
#include "search/minimax.hpp"

#include <bitset>
#include <cstdint>
#include <filesystem>
#include <random>
#include <string>
#include <thread>
#include <vector>

// Fixed-point scale applied to int16 value/score targets.
constexpr int16_t NNUE_SCORE_SCALE = 10000;
// Maximum number of policy entries stored per sample.
constexpr int NNUE_MAX_POLICY = 8;
// Dataset format identifiers.
constexpr uint32_t NNUE_DATASET_MAGIC = 0x314E4E47u; // 'GNN1' little-endian
constexpr uint32_t NNUE_DATASET_VERSION = 3u;

enum NNUESampleFlags : uint16_t
{
    SEARCH_VALID = 1,
    EXPLORED = 2,
    IMMEDIATE_WIN = 4,
    FORCED_DEFENSE = 8,
    RELABELLED = 16
};

struct NNUEDatasetConfig
{
    int board_size = 15;
    int max_depth = 12;
    int time_limit_ms = 20000;
    std::size_t target_positions = 1'000'000;
    unsigned seed = 1234;

    unsigned split_seed = 2026;
    int explore_plies = 16;
    int explore_top_k = 4;
    double explore_probability = 0.15;
    double explore_max_loss = 200.0;
    double sampling_temperature = 100.0;
    double policy_temp = 200.0;
    double score_norm = 1000.0;
    int min_depth = 1;
    int sample_stride = 2;
    int max_samples_per_game = 32;
    // Zero disables selective higher-budget relabeling.
    int relabel_every = 0;
    int relabel_depth = 14;
    int relabel_time_ms = 80000;
    uint64_t tt_memory_bytes = 16 * 1024 * 1024;
    std::string opening_pool_path;
    // TD(lambda) weight for the blended value target
    double td_lambda = 0.7;
    // randomized (unstored) opening plies per game for position diversity
    int opening_min = 2;
    int opening_max = 8;
    // Fraction of canonical opening hash space assigned to validation.
    double val_fraction = 0.05;
    // samples per shard file
    std::size_t shard_size = 1'000'000;

    std::string dataset_path = std::filesystem::current_path().string() + "/nnue_dataset";

    int squares() const
    {
        return board_size * board_size;
    }

    unsigned int cores =
        std::thread::hardware_concurrency() == 0 ? 1 : std::thread::hardware_concurrency();
};

// One quantized policy target; active probabilities sum to 65535.
struct PolicyEntry
{
    uint16_t move;
    uint16_t prob;
};

// Compact self-play sample. The board is stored as bitsets; feature planes are
// materialized from the side-to-move perspective only when writing to disk.
struct NNUESample
{
    std::bitset<MAX_SQUARES> black;
    std::bitset<MAX_SQUARES> white;
    uint16_t ply = 0;
    uint8_t side_to_move = 1; // 1 = black to move
    uint8_t policy_count = 0;
    int16_t search_score = 0; // STM view, scaled by NNUE_SCORE_SCALE
    int16_t final_value = 0;  // STM game result, scaled
    int16_t td_value = 0;     // STM TD(lambda) target, scaled
    PolicyEntry policy[NNUE_MAX_POLICY] = {};
    uint64_t game_id = 0;
    uint64_t opening_id = 0;
    uint64_t position_key = 0;
    float raw_search = 0.0f;
    float played_score = 0.0f;
    uint64_t nodes = 0;
    uint16_t depth = 0;
    uint16_t played_move = 0;
    uint16_t flags = SEARCH_VALID;
    uint8_t end_reason = 0; // 1 = five in a row, 2 = full board
    int8_t tactical = 0;    // 1 = immediate win, -1 = unavoidable immediate loss
};

namespace nnue_quality
{
struct Selection
{
    int move;
    uint16_t flags;
    int8_t tactical;
};

// Canonical absolute-color board key under all eight D4 symmetries.
std::string canonical_key(const Board& board, bool black_to_move);
uint64_t key_hash(const std::string& key);
bool validation_opening(const std::string& canonical_opening, unsigned split_seed,
                        double validation_fraction);
// Protect one-ply wins and defenses before exploring plausible searched moves.
Selection select_move(Board& board, bool black_to_move,
                      const std::vector<RootCandidate>& candidates, int best_move, int ply,
                      const NNUEDatasetConfig& config, std::mt19937& rng);
std::vector<std::vector<int>> load_opening_pool(const std::string& path, int board_size);
} // namespace nnue_quality

// On-disk header written at the start of every shard.
struct DatasetHeader
{
    uint32_t magic;
    uint32_t version;
    uint16_t board_size;
    uint16_t rule;         // 0 = freestyle
    uint32_t sample_count; // samples in this shard
    uint16_t feature_len;  // 2 * squares
    uint16_t score_scale;  // NNUE_SCORE_SCALE
    uint16_t max_policy;   // NNUE_MAX_POLICY
    uint16_t reserved0;
    uint64_t reserved[3];
};

class NNUEDataset
{
public:
    void generate_dataset(const NNUEDatasetConfig& config);
    void save_dataset(const NNUEDatasetConfig& config);

private:
    std::vector<NNUESample> samples;
    std::vector<NNUESample> val_samples;
    uint64_t generated_games = 0;
    uint64_t duplicate_positions = 0;
    std::vector<std::vector<int>> used_openings;
};