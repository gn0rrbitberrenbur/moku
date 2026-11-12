#include "benchmark/benchmark.hpp"
#include "config.hpp"
#include "engine/extended_protocol/extended_protocol.hpp"

#include <gtest/gtest.h>

#include <iostream>
#include <sstream>
#include <string>

class TestExtendedEngine : public Engine
{
public:
    std::string execute(const std::string& command)
    {
        std::ostringstream output;
        std::streambuf* old_cout = std::cout.rdbuf(output.rdbuf());
        process_command(command);
        std::cout.rdbuf(old_cout);
        return output.str();
    }

    Board& get_board()
    {
        return board;
    }

    int search(bool is_black, int time_limit_ms)
    {
        return agent.get_best_move_timed_smp(board, is_black, time_limit_ms);
    }

    int search_fixed_depth(bool is_black)
    {
        return agent.get_best_move(board, is_black);
    }

    int search_timed(bool is_black, int time_limit_ms)
    {
        return agent.get_best_move_timed(board, is_black, time_limit_ms);
    }

    size_t tt_size() const
    {
        return agent.get_tt_size();
    }

    size_t tt_allocated_bytes() const
    {
        return agent.get_tt_allocated_bytes();
    }

    long long nodes_searched() const
    {
        return agent.get_nodes_searched();
    }
};

class ExtendedProtocolTest : public ::testing::Test
{
protected:
    GameConfig saved_config;
    std::ostringstream diagnostics;
    std::streambuf* saved_cerr = nullptr;

    void SetUp() override
    {
        saved_config = g_config;
        g_config.board_size = 5;
        g_config.max_depth = 8;
        g_config.max_nodes = 0;
        g_config.max_memory = 0;
        g_config.cores = 1;
        g_config.debug_output = false;
        saved_cerr = std::cerr.rdbuf(diagnostics.rdbuf());
    }

    void TearDown() override
    {
        std::cerr.rdbuf(saved_cerr);
        g_config = saved_config;
    }
};

TEST_F(ExtendedProtocolTest, InfoMaxMemoryResizesImmediatelyAndPreservesBoard)
{
    TestExtendedEngine engine;
    engine.get_board().make_move(engine.get_board().pos(2, 2), true);
    const size_t bytes = 7 * sizeof(TTEntry);

    EXPECT_TRUE(engine.execute("INFO max_memory " + std::to_string(bytes)).empty());
    EXPECT_EQ(g_config.max_memory, bytes);
    EXPECT_EQ(engine.tt_size(), 4u);
    EXPECT_EQ(engine.tt_allocated_bytes(), 4 * sizeof(TTEntry));
    EXPECT_LE(engine.tt_allocated_bytes(), bytes);
    EXPECT_TRUE(engine.get_board().black.test(engine.get_board().pos(2, 2)));
    EXPECT_EQ(engine.get_board().white.count(), 0u);
    EXPECT_NE(diagnostics.str().find("[info] TT capacity set to 4 entries"), std::string::npos);

    EXPECT_EQ(engine.execute("RESTART"), "OK\n");
    EXPECT_EQ(engine.tt_size(), 4u);
    EXPECT_EQ(engine.execute("START 5"), "OK\n");
    EXPECT_EQ(engine.tt_size(), 4u);

    EXPECT_TRUE(engine.execute("INFO max_memory 0").empty());
    EXPECT_EQ(g_config.max_memory, 0);
    EXPECT_EQ(engine.tt_size(), size_t(1) << 23);
}

TEST_F(ExtendedProtocolTest, InfoMaxMemoryRejectsInvalidValuesWithoutChangingState)
{
    TestExtendedEngine engine;
    const size_t bytes = 4 * sizeof(TTEntry);
    engine.execute("INFO max_memory " + std::to_string(bytes));

    for (const std::string value :
         {"", "-1", "invalid", "12suffix", "128 256", "9223372036854775808", "1"})
    {
        diagnostics.str("");
        EXPECT_TRUE(engine.execute("INFO max_memory " + value).empty());
        EXPECT_EQ(g_config.max_memory, bytes);
        EXPECT_EQ(engine.tt_size(), 4u);
        EXPECT_EQ(engine.tt_allocated_bytes(), bytes);
        EXPECT_NE(diagnostics.str().find("[error] INFO max_memory"), std::string::npos);
    }
}

TEST_F(ExtendedProtocolTest, InfoMaxMemoryAppliesToAllSearchPathsAndMetrics)
{
    TestExtendedEngine engine;
    engine.execute("INFO max_memory " + std::to_string(sizeof(TTEntry)));
    engine.execute("INFO max_nodes 7");
    engine.get_board().make_move(engine.get_board().pos(2, 2), true);
    g_config.debug_output = true;

    for (int path = 0; path < 4; ++path)
    {
        diagnostics.str("");
        g_config.cores = path == 3 ? 2 : 1;
        const int move = path == 0 ? engine.search_fixed_depth(false)
                         : path == 1 ? engine.search_timed(false, 60000)
                                     : engine.search(false, 60000);
        EXPECT_GE(move, 0);
        EXPECT_FALSE(engine.get_board().test_pos(move));
        EXPECT_EQ(engine.get_board().black.count(), 1u);
        EXPECT_EQ(engine.get_board().white.count(), 0u);
        const std::string output = diagnostics.str();
        EXPECT_NE(output.find("\"event\":\"search_finished\""), std::string::npos);
        EXPECT_NE(output.find("\"tt_capacity_entries\":1,"), std::string::npos);
        EXPECT_NE(output.find("\"tt_allocated_bytes\":" + std::to_string(sizeof(TTEntry)) + ","),
                  std::string::npos);
    }
}

TEST_F(ExtendedProtocolTest, BenchmarkPositionUsesExplicitMemoryBudget)
{
    g_config.debug_output = true;
    g_config.max_nodes = 7;
    const BenchResult result =
        run_position("limited", {{"C3", true}}, false, 8, 0, 5, sizeof(TTEntry));
    EXPECT_GE(result.best_move, 0);
    EXPECT_NE(diagnostics.str().find("\"tt_capacity_entries\":1,"), std::string::npos);
    EXPECT_NE(diagnostics.str().find("\"tt_allocated_bytes\":" +
                                    std::to_string(sizeof(TTEntry)) + ","),
              std::string::npos);
}

TEST_F(ExtendedProtocolTest, HelpDescribesMemoryLimit)
{
    TestExtendedEngine engine;
    EXPECT_NE(engine.execute("HELP").find("INFO max_memory bytes"), std::string::npos);
}

TEST_F(ExtendedProtocolTest, MetricsCommandTogglesOutputAndValidatesInterval)
{
    TestExtendedEngine engine;

    EXPECT_EQ(engine.execute("METRICS 25"), "METRICS True\n");
    EXPECT_TRUE(g_config.debug_output);
    EXPECT_EQ(g_config.metrics_logging_interval, 25);

    EXPECT_EQ(engine.execute("METRICS"), "METRICS False\n");
    EXPECT_FALSE(g_config.debug_output);
    EXPECT_EQ(g_config.metrics_logging_interval, 25);

    EXPECT_EQ(engine.execute("METRICS 0"), "ERROR METRICS expects a positive interval in ms\n");
    EXPECT_FALSE(g_config.debug_output);
    EXPECT_EQ(engine.execute("METRICS 10 20"),
              "ERROR METRICS expects at most one argument\n");
    EXPECT_FALSE(g_config.debug_output);
    EXPECT_NE(engine.execute("HELP").find("METRICS [interval_ms]"), std::string::npos);
}

TEST_F(ExtendedProtocolTest, InfoMaxNodesAcceptsNonnegativeValues)
{
    TestExtendedEngine engine;

    EXPECT_TRUE(engine.execute("INFO max_nodes 100").empty());
    EXPECT_EQ(g_config.max_nodes, 100);

    engine.execute("INFO max_nodes 0");
    EXPECT_EQ(g_config.max_nodes, 0);

    engine.execute("INFO max_nodes -1");
    EXPECT_EQ(g_config.max_nodes, 0);

    engine.execute("INFO max_nodes invalid");
    EXPECT_EQ(g_config.max_nodes, 0);

    engine.execute("INFO max_nodes 12suffix");
    EXPECT_EQ(g_config.max_nodes, 0);

    engine.execute("INFO max_nodes 12 13");
    EXPECT_EQ(g_config.max_nodes, 0);
}

TEST_F(ExtendedProtocolTest, NodeLimitIsSharedAcrossSmpWorkers)
{
    TestExtendedEngine engine;
    engine.get_board().make_move(engine.get_board().pos(2, 2), true);

    for (unsigned int cores : {1u, 2u})
    {
        g_config.cores = cores;
        engine.execute("INFO max_nodes 7");

        EXPECT_GE(engine.search(false, 60000), 0);
        EXPECT_EQ(engine.nodes_searched(), 7);
    }
}

TEST_F(ExtendedProtocolTest, FixedDepthSearchHonorsNodeLimit)
{
    TestExtendedEngine engine;
    engine.get_board().make_move(engine.get_board().pos(2, 2), true);
    engine.execute("INFO max_nodes 7");

    EXPECT_GE(engine.search_fixed_depth(false), 0);
    EXPECT_EQ(engine.nodes_searched(), 7);
    EXPECT_EQ(engine.get_board().black.count(), 1);
    EXPECT_EQ(engine.get_board().white.count(), 0);
}
