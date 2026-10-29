#pragma once
#include <thread>
#include <filesystem>
#include <vector>
#include <bitset>
#include <cstdint>
#include <string>
#include "board.hpp"

// Fixed-point scale applied to int16 value/score targets.
constexpr int16_t NNUE_SCORE_SCALE = 100;
// Maximum number of policy entries stored per sample.
constexpr int NNUE_MAX_POLICY = 8;
// Dataset format identifiers.
constexpr uint32_t NNUE_DATASET_MAGIC = 0x314E4E47u; // 'GNN1' little-endian
constexpr uint32_t NNUE_DATASET_VERSION = 2u;

struct NNUEDatasetConfig {
    int board_size = 15;
    int max_depth = 12;
    int time_limit_ms = 20000;
    std::size_t target_positions = 1'000'000;
    unsigned seed = 1234;

    // temperature schedule for move sampling
    double open_temp = 1.5;   // temperature at ply 0
    double min_temp = 0.05;   // asymptotic minimum temperature
    // temperature used to convert candidate scores into the policy target
    double policy_temp = 1.0;
    // TD(lambda) weight for the blended value target
    double td_lambda = 0.7;
    // randomized (unstored) opening plies per game for position diversity
    int opening_min = 2;
    int opening_max = 8;
    // fraction of whole games routed to the validation split
    double val_fraction = 0.05;
    // samples per shard file
    std::size_t shard_size = 1'000'000;

    std::string dataset_path =
        std::filesystem::current_path().string() + "/nnue_dataset";

    int squares() const { return board_size * board_size; }

    unsigned int cores = std::thread::hardware_concurrency() == 0
                             ? 1
                             : std::thread::hardware_concurrency();
};

// One quantized policy target: move index and probability (sums to ~65535).
struct PolicyEntry {
    uint16_t move;
    uint16_t prob;
};

// Compact self-play sample. The board is stored as bitsets; feature planes are
// materialized from the side-to-move perspective only when writing to disk.
struct NNUESample {
    std::bitset<MAX_SQUARES> black;
    std::bitset<MAX_SQUARES> white;
    uint16_t ply = 0;
    uint8_t side_to_move = 1;          // 1 = black to move
    uint8_t policy_count = 0;
    int16_t search_score = 0;          // STM view, scaled by NNUE_SCORE_SCALE
    int16_t final_value = 0;           // STM game result, scaled
    int16_t td_value = 0;              // STM TD(lambda) target, scaled
    PolicyEntry policy[NNUE_MAX_POLICY] = {};
};

// On-disk header written at the start of every shard.
struct DatasetHeader {
    uint32_t magic;
    uint32_t version;
    uint16_t board_size;
    uint16_t rule;          // 0 = freestyle
    uint32_t sample_count;  // samples in this shard
    uint16_t feature_len;   // 2 * squares
    uint16_t score_scale;   // NNUE_SCORE_SCALE
    uint16_t max_policy;    // NNUE_MAX_POLICY
    uint16_t reserved0;
    uint64_t reserved[3];
};

class NNUEDataset {
public:
    void generate_dataset(const NNUEDatasetConfig& config);
    void save_dataset(const NNUEDatasetConfig& config);

private:
    std::vector<NNUESample> samples;
    std::vector<NNUESample> val_samples;
};