#include "config.hpp"
#include "tt/transposition_table.hpp"

#include <gtest/gtest.h>

// Test cases for the transposition table
TEST(TranspositionTableTest, SizeIsPowerOfTwo)
{
    TranspositionTable tt;
    EXPECT_EQ(tt.size(), size_t(1) << 23);
}

TEST(TranspositionTableTest, MemoryBudgetRoundsDownAndCapsDefaultCapacity)
{
    TranspositionTable tt(7 * sizeof(TTEntry));
    EXPECT_EQ(tt.size(), 4u);
    EXPECT_EQ(tt.allocated_bytes(), 4 * sizeof(TTEntry));
    EXPECT_LE(tt.allocated_bytes(), 7 * sizeof(TTEntry));

    tt.set_max_memory(sizeof(TTEntry));
    EXPECT_EQ(tt.size(), 1u);
    EXPECT_EQ(tt.allocated_bytes(), sizeof(TTEntry));
    tt.store(0, 95000.0f, 2, TTEntry::EXACT, 1, 3);
    TTEntry entry;
    ASSERT_TRUE(tt.probe(0, entry, 3));
    EXPECT_FLOAT_EQ(entry.score, 95000.0f);
    EXPECT_EQ(tt.occupancy_permille(), 1000u);
    tt.store(1, 12.0f, 1, TTEntry::LOWERBOUND, 2, 0);
    EXPECT_FALSE(tt.probe(0, entry, 0));
    EXPECT_TRUE(tt.probe(1, entry, 0));
    EXPECT_EQ(tt.occupancy_permille(), 1000u);

    tt.set_max_memory(UINT64_MAX);
    EXPECT_EQ(tt.size(), size_t(1) << 23);
    tt.set_max_memory(sizeof(TTEntry));
    tt.set_max_memory(0);
    EXPECT_EQ(tt.size(), size_t(1) << 23);
}

TEST(TranspositionTableTest, ResizeClearsEntriesAndPreservesZobristKeys)
{
    TranspositionTable tt(4 * sizeof(TTEntry));
    Board board;
    board.make_move(0, true);
    const uint64_t hash = tt.compute_hash(board, false);
    tt.store(hash, 42.0f, 5, TTEntry::EXACT, 1, 0);

    tt.set_max_memory(5 * sizeof(TTEntry));
    TTEntry entry;
    EXPECT_TRUE(tt.probe(hash, entry, 0));

    for (uint64_t bytes : {2 * sizeof(TTEntry), 8 * sizeof(TTEntry)})
    {
        tt.set_max_memory(bytes);
        EXPECT_EQ(tt.allocated_bytes(), bytes);
        EXPECT_EQ(tt.occupancy_permille(), 0u);
        EXPECT_FALSE(tt.probe(hash, entry, 0));
        EXPECT_EQ(tt.compute_hash(board, false), hash);
        tt.store(hash, 42.0f, 5, TTEntry::EXACT, 1, 0);
        ASSERT_TRUE(tt.probe(hash, entry, 0));
        EXPECT_FLOAT_EQ(entry.score, 42.0f);
    }
}

TEST(TranspositionTableTest, InvalidMemoryBudgetPreservesTable)
{
    TranspositionTable tt(2 * sizeof(TTEntry));
    tt.store(1, 42.0f, 5, TTEntry::EXACT, 1, 0);
    EXPECT_THROW(tt.set_max_memory(sizeof(TTEntry) - 1), std::invalid_argument);
    EXPECT_EQ(tt.size(), 2u);
    EXPECT_EQ(tt.occupancy_permille(), 500u);
    TTEntry entry;
    ASSERT_TRUE(tt.probe(1, entry, 0));
    EXPECT_FLOAT_EQ(entry.score, 42.0f);
}

TEST(TranspositionTableTest, SmallTableSynchronizesCollidingSlots)
{
    TranspositionTable tt(sizeof(TTEntry));
    std::vector<std::thread> workers;
    for (uint64_t key = 0; key < 4; ++key)
    {
        workers.emplace_back(
            [&, key]()
            {
                for (int i = 0; i < 2000; ++i)
                {
                    tt.store(key, static_cast<float>(key), 2, TTEntry::EXACT, static_cast<int>(key),
                             0);
                    TTEntry entry;
                    if (tt.probe(key, entry, 0))
                    {
                        EXPECT_EQ(entry.key, key);
                        EXPECT_FLOAT_EQ(entry.score, static_cast<float>(key));
                        EXPECT_EQ(entry.best_move, static_cast<int>(key));
                    }
                }
            });
    }
    for (auto& worker : workers)
    {
        worker.join();
    }
    EXPECT_EQ(tt.occupancy_permille(), 1000u);
}

TEST(TranspositionTableTest, HashEmptyBoardDeterministic)
{
    g_config.board_size = 15;
    TranspositionTable tt;
    Board board;
    uint64_t h1 = tt.compute_hash(board, true);
    uint64_t h2 = tt.compute_hash(board, true);
    EXPECT_EQ(h1, h2);
}

TEST(TranspositionTableTest, HashDiffersByTurn)
{
    g_config.board_size = 15;
    TranspositionTable tt;
    Board board;
    EXPECT_NE(tt.compute_hash(board, true), tt.compute_hash(board, false));
}

TEST(TranspositionTableTest, HashDiffersByStone)
{
    g_config.board_size = 15;
    TranspositionTable tt;
    Board board;
    uint64_t empty = tt.compute_hash(board, true);
    board.make_move(0, true);
    EXPECT_NE(empty, tt.compute_hash(board, true));
}

TEST(TranspositionTableTest, HashDiffersByColorOnSameSquare)
{
    g_config.board_size = 15;
    TranspositionTable tt;
    Board black_board;
    black_board.make_move(0, true);
    Board white_board;
    white_board.make_move(0, false);
    EXPECT_NE(tt.compute_hash(black_board, true), tt.compute_hash(white_board, true));
}

TEST(TranspositionTableTest, IncrementalUpdateMatchesFullHash)
{
    g_config.board_size = 15;
    TranspositionTable tt;
    Board board;

    uint64_t incremental = tt.compute_hash(board, true);
    incremental = tt.update_hash(incremental, 0, true); // black at 0, turn -> white
    board.make_move(0, true);

    uint64_t full = tt.compute_hash(board, false);
    EXPECT_EQ(incremental, full);
}

TEST(TranspositionTableTest, IncrementalUpdateMultipleMoves)
{
    g_config.board_size = 15;
    TranspositionTable tt;
    Board board;

    uint64_t inc = tt.compute_hash(board, true);
    inc = tt.update_hash(inc, 5, true);  // black
    inc = tt.update_hash(inc, 6, false); // white
    board.make_move(5, true);
    board.make_move(6, false);

    EXPECT_EQ(inc, tt.compute_hash(board, true));
}

TEST(TranspositionTableTest, StoreAndProbe)
{
    TranspositionTable tt;
    uint64_t hash = 0xABCDEF123456ULL;
    tt.store(hash, 42.0f, 5, TTEntry::EXACT, 112, 0);

    TTEntry entry;
    ASSERT_TRUE(tt.probe(hash, entry, 0));
    EXPECT_EQ(entry.key, hash);
    EXPECT_FLOAT_EQ(entry.score, 42.0f);
    EXPECT_EQ(entry.depth, 5);
    EXPECT_EQ(entry.flag, TTEntry::EXACT);
    EXPECT_EQ(entry.best_move, 112);
}

TEST(TranspositionTableTest, ProbeMissReturnsFalse)
{
    TranspositionTable tt;
    TTEntry entry;
    EXPECT_FALSE(tt.probe(0xDEADBEEFULL, entry, 0));
}

TEST(TranspositionTableTest, DeeperDoesNotOverwriteSameHash)
{
    TranspositionTable tt;
    uint64_t hash = 0x1111ULL;
    tt.store(hash, 10.0f, 8, TTEntry::EXACT, 1, 0);
    tt.store(hash, 20.0f, 3, TTEntry::EXACT, 2, 0); // shallower, same key

    TTEntry entry;
    ASSERT_TRUE(tt.probe(hash, entry, 0));
    EXPECT_EQ(entry.depth, 8);
    EXPECT_FLOAT_EQ(entry.score, 10.0f);
    EXPECT_EQ(entry.best_move, 1);
}

TEST(TranspositionTableTest, DeeperOverwritesSameHash)
{
    TranspositionTable tt;
    uint64_t hash = 0x2222ULL;
    tt.store(hash, 10.0f, 3, TTEntry::EXACT, 1, 0);
    tt.store(hash, 20.0f, 8, TTEntry::EXACT, 2, 0); // deeper, same key

    TTEntry entry;
    ASSERT_TRUE(tt.probe(hash, entry, 0));
    EXPECT_EQ(entry.depth, 8);
    EXPECT_FLOAT_EQ(entry.score, 20.0f);
    EXPECT_EQ(entry.best_move, 2);
}

TEST(TranspositionTableTest, MateScoreAdjustedByPly)
{
    TranspositionTable tt;
    uint64_t hash = 0x3333ULL;
    // store a mate score found at ply 4
    tt.store(hash, 95000.0f, 5, TTEntry::EXACT, 0, 4);

    TTEntry entry;
    ASSERT_TRUE(tt.probe(hash, entry, 4));
    // store adds ply, probe subtracts ply -> round trip
    EXPECT_FLOAT_EQ(entry.score, 95000.0f);
}

TEST(TranspositionTableTest, NegativeMateScoreAdjustedByPly)
{
    TranspositionTable tt;
    uint64_t hash = 0x4444ULL;
    tt.store(hash, -95000.0f, 5, TTEntry::EXACT, 0, 3);

    TTEntry entry;
    ASSERT_TRUE(tt.probe(hash, entry, 3));
    EXPECT_FLOAT_EQ(entry.score, -95000.0f);
}

TEST(TranspositionTableTest, ClearRemovesEntries)
{
    TranspositionTable tt;
    uint64_t hash = 0x5555ULL;
    tt.store(hash, 1.0f, 1, TTEntry::EXACT, 0, 0);

    TTEntry entry;
    ASSERT_TRUE(tt.probe(hash, entry, 0));

    tt.clear();
    EXPECT_FALSE(tt.probe(hash, entry, 0));
}

TEST(TranspositionTableTest, OccupancyTracksSlotsAndClear)
{
    TranspositionTable tt;
    EXPECT_EQ(tt.allocated_bytes(), tt.size() * sizeof(TTEntry));
    EXPECT_EQ(tt.occupancy_permille(), 0u);
    TTEntry entry;
    EXPECT_FALSE(tt.probe(0, entry, 0));

    const size_t count = (tt.size() + 999) / 1000;
    for (size_t i = 0; i < count; ++i)
        tt.store(i, 1.0f, 2, TTEntry::EXACT, 1, 0);
    EXPECT_TRUE(tt.probe(0, entry, 0));
    EXPECT_EQ(tt.occupancy_permille(), 1u);
    for (size_t i = 0; i < count; ++i)
    {
        tt.store(i, 2.0f, 1, TTEntry::EXACT, 2, 0);
        tt.store(i + tt.size(), 3.0f, 1, TTEntry::EXACT, 3, 0);
    }
    EXPECT_EQ(tt.occupancy_permille(), 1u);
    tt.clear();
    EXPECT_EQ(tt.occupancy_permille(), 0u);
    EXPECT_EQ(tt.allocated_bytes(), tt.size() * sizeof(TTEntry));
    EXPECT_FALSE(tt.probe(0, entry, 0));
}

TEST(TranspositionTableTest, ConcurrentStoresCountEachSlotOnce)
{
    TranspositionTable tt;
    const size_t count = (tt.size() + 999) / 1000;
    std::vector<std::thread> workers;
    for (int worker = 0; worker < 4; ++worker)
    {
        workers.emplace_back(
            [&]()
            {
                for (size_t i = 0; i < count; ++i)
                {
                    tt.store(i, 7.0f, 2, TTEntry::EXACT, 1, 0);
                    TTEntry entry;
                    EXPECT_TRUE(tt.probe(i, entry, 0));
                    EXPECT_EQ(entry.depth, 2);
                    EXPECT_FLOAT_EQ(entry.score, 7.0f);
                }
            });
    }
    for (auto& worker : workers)
        worker.join();
    EXPECT_EQ(tt.occupancy_permille(), 1u);
}