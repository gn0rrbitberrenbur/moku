#include "config.hpp"
#include "nnue.hpp"

#include <chrono>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <limits>
#include <random>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

namespace
{
class DatasetQuality : public ::testing::Test
{
protected:
    NNUEDatasetConfig config;
    std::mt19937 rng{1234};

    void SetUp() override
    {
        g_config.board_size = 15;
        g_config.max_nodes = 0;
        config.explore_probability = 1.0;
    }

    void TearDown() override
    {
        g_config.board_size = 15;
        g_config.max_nodes = 0;
    }
};

TEST_F(DatasetQuality, AlwaysTakesImmediateWinForEitherColor)
{
    for (bool black : {false, true})
    {
        Board board;
        for (int col = 0; col < 4; ++col)
        {
            board.make_move(75 + col, black);
        }
        const auto before_black = board.black;
        const auto before_white = board.white;
        const std::vector<RootCandidate> candidates{{79, 100000.0f, 1}, {100, 0.0f, 1}};
        for (int i = 0; i < 50; ++i)
        {
            const auto selection =
                nnue_quality::select_move(board, black, candidates, 100, 4, config, rng);
            EXPECT_EQ(selection.move, 79);
            EXPECT_TRUE(selection.flags & IMMEDIATE_WIN);
            EXPECT_EQ(selection.tactical, 1);
        }
        EXPECT_EQ(board.black, before_black);
        EXPECT_EQ(board.white, before_white);
    }
}

TEST_F(DatasetQuality, AlwaysBlocksUniqueImmediateThreat)
{
    Board board;
    for (int col = 0; col < 4; ++col)
    {
        board.make_move(75 + col, false);
    }
    const std::vector<RootCandidate> candidates{{79, 0.0f, 1}, {100, 200.0f, 1}};
    const auto selection = nnue_quality::select_move(board, true, candidates, 100, 4, config, rng);
    EXPECT_EQ(selection.move, 79);
    EXPECT_TRUE(selection.flags & FORCED_DEFENSE);
    EXPECT_FALSE(selection.flags & EXPLORED);
    EXPECT_EQ(selection.tactical, 0);
}

TEST_F(DatasetQuality, MarksUnavoidableImmediateLossWithoutPretendingItIsProvenByScore)
{
    Board board;
    for (int col = 3; col < 7; ++col)
    {
        board.make_move(75 + col, false);
    }
    const std::vector<RootCandidate> candidates{{77, -30000.0f, 1}, {82, -100000.0f, 1}};
    const auto selection = nnue_quality::select_move(board, true, candidates, 77, 4, config, rng);
    EXPECT_EQ(selection.tactical, -1);
    EXPECT_EQ(selection.move, 77);
    EXPECT_FALSE(selection.flags & EXPLORED);
}

TEST_F(DatasetQuality, ExplorationUsesRawLossAndEndsAtConfiguredPly)
{
    Board board;
    board.make_move(112, true);
    const std::vector<RootCandidate> candidates{
        {113, 30000.0f, 1}, {114, 30200.0f, 1}, {115, 31000.0f, 1}};
    // White minimizes: tanh saturation must not hide an excessive score loss.
    config.explore_top_k = 1;
    EXPECT_EQ(nnue_quality::select_move(board, false, candidates, 113, 1, config, rng).move, 113);
    config.explore_top_k = 4;
    for (int i = 0; i < 100; ++i)
    {
        const auto selection =
            nnue_quality::select_move(board, false, candidates, 113, 1, config, rng);
        EXPECT_NE(selection.move, 115);
        EXPECT_EQ(bool(selection.flags & EXPLORED), selection.move != 113);
    }
    EXPECT_EQ(
        nnue_quality::select_move(board, false, candidates, 113, config.explore_plies, config, rng)
            .move,
        113);
}

TEST_F(DatasetQuality, CanonicalKeysPreserveColorAndSideAndIgnoreSymmetry)
{
    Board a;
    Board b;
    a.make_move(2 * 15 + 3, true);
    a.make_move(4 * 15 + 7, false);
    b.make_move(3 * 15 + 12, true);
    b.make_move(7 * 15 + 10, false);
    EXPECT_EQ(nnue_quality::canonical_key(a, true), nnue_quality::canonical_key(b, true));
    EXPECT_NE(nnue_quality::canonical_key(a, true), nnue_quality::canonical_key(a, false));
    std::swap(b.black, b.white);
    EXPECT_NE(nnue_quality::canonical_key(a, true), nnue_quality::canonical_key(b, true));
    EXPECT_EQ(nnue_quality::key_hash(""), 14695981039346656037ull);
    EXPECT_EQ(nnue_quality::key_hash("a"), 12638187200555641996ull);
}

TEST_F(DatasetQuality, OpeningSplitsAreStableUnderSymmetry)
{
    Board a;
    Board b;
    a.make_move(2 * 15 + 3, true);
    b.make_move(3 * 15 + 12, true);
    const auto key = nnue_quality::canonical_key(a, false);
    const auto rotated = nnue_quality::canonical_key(b, false);
    EXPECT_FALSE(nnue_quality::validation_opening(key, config.split_seed, 0));
    EXPECT_TRUE(nnue_quality::validation_opening(key, config.split_seed, 1));
    const bool expected = nnue_quality::validation_opening(key, config.split_seed, 0.5);
    EXPECT_EQ(nnue_quality::validation_opening(rotated, config.split_seed, 0.5), expected);
}

TEST_F(DatasetQuality, RejectsInvalidConfigurationAndInsufficientSearch)
{
    NNUEDataset dataset;
    config.score_norm = std::numeric_limits<double>::quiet_NaN();
    EXPECT_THROW(dataset.generate_dataset(config), std::invalid_argument);
    config.score_norm = 1000;
    config.target_positions = 1;
    config.cores = 1;
    config.opening_max = 0;
    config.max_depth = 2;
    config.tt_memory_bytes = 1024;
    g_config.max_nodes = 1;
    EXPECT_THROW(dataset.generate_dataset(config), std::runtime_error);
}

TEST_F(DatasetQuality, ValidatesOpeningPoolMovesAndTerminalStates)
{
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto path = std::filesystem::temp_directory_path() /
                      ("moku-opening-test-" + std::to_string(stamp) + ".txt");
    {
        std::ofstream out(path);
        out << "# opening\n112 113 127\n";
    }
    const auto pool = nnue_quality::load_opening_pool(path.string(), 15);
    ASSERT_EQ(pool.size(), 1u);
    EXPECT_EQ(pool.front(), (std::vector<int>{112, 113, 127}));
    {
        std::ofstream out(path);
        out << "112 112\n";
    }
    EXPECT_THROW(nnue_quality::load_opening_pool(path.string(), 15), std::runtime_error);
    {
        std::ofstream out(path);
        out << "0 30 1 31 2 32 3 33 4\n";
    }
    EXPECT_THROW(nnue_quality::load_opening_pool(path.string(), 15), std::runtime_error);
    std::filesystem::remove(path);
}

TEST_F(DatasetQuality, WritesPackedV3QualityAndRelabeledTargets)
{
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto prefix =
        std::filesystem::temp_directory_path() / ("moku-dataset-test-" + std::to_string(stamp));
    config.dataset_path = prefix.string();
    config.board_size = 5;
    config.cores = 1;
    config.target_positions = 12;
    config.max_depth = 1;
    config.time_limit_ms = 100;
    config.tt_memory_bytes = 4096;
    config.val_fraction = 0;
    config.sample_stride = 1;
    config.max_samples_per_game = 3;
    config.relabel_every = 1;
    config.relabel_depth = 2;
    config.relabel_time_ms = 200;
    NNUEDataset dataset;
    dataset.generate_dataset(config);
    dataset.save_dataset(config);
    const std::string shard = prefix.string() + "-00000.bin";
    EXPECT_EQ(std::filesystem::file_size(shard), 48u + 12u * 140u);
    std::ifstream input(shard, std::ios::binary);
    const auto read = [&](auto& value)
    { input.read(reinterpret_cast<char*>(&value), sizeof(value)); };
    uint32_t magic = 0;
    uint32_t version = 0;
    read(magic);
    read(version);
    EXPECT_EQ(magic, NNUE_DATASET_MAGIC);
    EXPECT_EQ(version, 3u);
    std::unordered_map<uint64_t, unsigned> samples_per_game;
    std::unordered_set<std::string> positions;
    for (int i = 0; i < 12; ++i)
    {
        std::vector<char> record(140);
        input.seekg(48 + i * 140);
        input.read(record.data(), static_cast<std::streamsize>(record.size()));
        EXPECT_TRUE(input.good());
        uint16_t flags = 0;
        uint16_t depth = 0;
        float raw = 0;
        int16_t normalized = 0;
        std::memcpy(&flags, record.data() + 136, sizeof(flags));
        std::memcpy(&depth, record.data() + 132, sizeof(depth));
        std::memcpy(&raw, record.data() + 116, sizeof(raw));
        std::memcpy(&normalized, record.data() + 53, sizeof(normalized));
        EXPECT_TRUE(flags & SEARCH_VALID);
        EXPECT_TRUE(flags & RELABELLED);
        EXPECT_GE(depth, 1);
        EXPECT_EQ(normalized,
                  std::lround(10000 * static_cast<float>(std::tanh(raw / config.score_norm))));
        EXPECT_TRUE(record[138] == 1 || record[138] == 2);
        uint64_t game_id = 0;
        uint64_t position_hash = 0;
        std::memcpy(&game_id, record.data() + 92, sizeof(game_id));
        std::memcpy(&position_hash, record.data() + 108, sizeof(position_hash));
        ++samples_per_game[game_id];
        EXPECT_EQ(game_id >> 32, config.seed);
        Board position;
        const bool black_to_move = record[50] == 1;
        for (int move = 0; move < 25; ++move)
        {
            if (record[move] == 1)
            {
                position.make_move(move, black_to_move);
            }
            if (record[25 + move] == 1)
            {
                position.make_move(move, !black_to_move);
            }
        }
        const auto key = nnue_quality::canonical_key(position, black_to_move);
        EXPECT_EQ(nnue_quality::key_hash(key), position_hash);
        EXPECT_TRUE(positions.insert(key).second);
        const auto count = static_cast<unsigned char>(record[59]);
        EXPECT_GT(count, 0);
        EXPECT_LE(count, 8);
        unsigned probability_sum = 0;
        for (unsigned j = 0; j < count; ++j)
        {
            uint16_t probability = 0;
            std::memcpy(&probability, record.data() + 62 + 4 * j, sizeof(probability));
            EXPECT_GT(probability, 0);
            probability_sum += probability;
        }
        EXPECT_EQ(probability_sum, 65535u);
    }
    for (const auto& entry : samples_per_game)
    {
        EXPECT_LE(entry.second, 3u);
    }
    input.close();
    std::filesystem::remove(shard);
    std::filesystem::remove(prefix.string() + ".manifest.json");
}

TEST_F(DatasetQuality, OpeningPoolKeepsItsSplitAcrossGenerationSeeds)
{
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto prefix =
        std::filesystem::temp_directory_path() / ("moku-pool-split-test-" + std::to_string(stamp));
    const std::string pool_path = prefix.string() + ".txt";
    {
        std::ofstream out(pool_path);
        out << "12 13 7 8\n";
    }
    config.board_size = 5;
    config.max_depth = 1;
    config.time_limit_ms = 100;
    config.tt_memory_bytes = 4096;
    config.cores = 1;
    config.target_positions = 4;
    config.sample_stride = 1;
    config.explore_probability = 0;
    config.opening_pool_path = pool_path;
    config.val_fraction = 0.5;
    g_config.board_size = 5;
    Board opening;
    opening.make_move(12, true);
    opening.make_move(13, false);
    opening.make_move(7, true);
    opening.make_move(8, false);
    const bool validation = nnue_quality::validation_opening(
        nnue_quality::canonical_key(opening, true), config.split_seed, config.val_fraction);
    for (unsigned seed : {123u, 456u})
    {
        config.seed = seed;
        config.dataset_path = prefix.string() + "-" + std::to_string(seed);
        NNUEDataset dataset;
        dataset.generate_dataset(config);
        dataset.save_dataset(config);
        const auto train = config.dataset_path + "-00000.bin";
        const auto val = config.dataset_path + "_val-00000.bin";
        EXPECT_EQ(std::filesystem::exists(train), !validation);
        EXPECT_EQ(std::filesystem::exists(val), validation);
        EXPECT_EQ(std::filesystem::file_size(validation ? val : train), 48u + 4u * 140u);
        std::filesystem::remove(validation ? val : train);
        std::filesystem::remove(config.dataset_path + ".manifest.json");
    }
    std::filesystem::remove(pool_path);
}
} // namespace
