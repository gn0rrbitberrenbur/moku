#include "config.hpp"
#include "engine/gomocup_protocol/gomocup_engine.hpp"
#include "eval/evaluation.hpp"
#include "game/board.hpp"
#include "tt/transposition_table.hpp"

#include <algorithm>
#include <gtest/gtest.h>
#include <iostream>
#include <limits>
#include <regex>
#include <sstream>

class SearchMetricsTest : public ::testing::Test
{
protected:
    GameConfig saved_config;
    std::ostringstream diagnostics;
    std::streambuf* saved_cerr = nullptr;

    void SetUp() override
    {
        saved_config = g_config;
        g_config.board_size = 5;
        g_config.cores = 1;
        g_config.debug_output = true;
        g_config.metrics_logging_interval = 60000;
        saved_cerr = std::cerr.rdbuf(diagnostics.rdbuf());
    }

    void TearDown() override
    {
        std::cerr.rdbuf(saved_cerr);
        g_config = saved_config;
    }

    std::string final_metrics()
    {
        const std::string output = diagnostics.str();
        const size_t start = output.rfind("[METRICS] ");
        EXPECT_NE(start, std::string::npos);
        if (start == std::string::npos)
            return "";
        const std::string metrics = output.substr(start, output.find('\n', start) - start);
        EXPECT_NE(metrics.find("\"event\":\"search_finished\""), std::string::npos);
        return metrics;
    }

    long long metric(const std::string& json, const std::string& name)
    {
        std::smatch match;
        const std::regex pattern("\"" + name + "\":([0-9]+)([,}])");
        EXPECT_TRUE(std::regex_search(json, match, pattern)) << name << ": " << json;
        if (match.empty())
            return -1;
        return std::stoll(match[1].str());
    }

    void check_metrics(const std::string& json, const MinimaxAgent& agent, int candidates)
    {
        EXPECT_EQ(metric(json, "tt_capacity_entries"), 1 << 23);
        EXPECT_EQ(metric(json, "tt_allocated_bytes"), (1 << 23) * sizeof(TTEntry));
        EXPECT_GE(metric(json, "tt_occupancy_permille"), 0);
        EXPECT_LE(metric(json, "tt_occupancy_permille"), 1000);
        EXPECT_EQ(metric(json, "tt_probes_total"), agent.get_nodes_searched());
        EXPECT_EQ(metric(json, "tt_hits_total"), agent.get_tt_hits());
        EXPECT_LE(metric(json, "tt_hits_total"), metric(json, "tt_probes_total"));
        EXPECT_LE(metric(json, "alpha_beta_cutoffs_total"), metric(json, "tt_probes_total"));
        EXPECT_EQ(metric(json, "workers_active"), 0);
        EXPECT_EQ(metric(json, "root_candidates_count"), candidates);
    }
};

TEST_F(SearchMetricsTest, FixedDepthBothColorsAndCutoffs)
{
    Board board;
    board.make_move(board.pos(2, 2), true);
    MinimaxAgent agent(3);
    const auto black = board.black;
    const auto white = board.white;
    for (bool is_black : {true, false})
    {
        EXPECT_GE(agent.get_best_move(board, is_black), 0);
        const std::string json = final_metrics();
        check_metrics(json, agent, 24);
        EXPECT_EQ(metric(json, "depth_completed"), 3);
        EXPECT_GT(metric(json, "alpha_beta_cutoffs_total"), 0);
        EXPECT_EQ(board.black, black);
        EXPECT_EQ(board.white, white);
    }
}

TEST_F(SearchMetricsTest, TimedSerialAndSmpCompletedDepth)
{
    Board board;
    board.make_move(board.pos(2, 2), true);
    MinimaxAgent agent(2);
    for (unsigned int cores : {1u, 2u})
    {
        g_config.cores = cores;
        EXPECT_GE(agent.get_best_move_timed_smp(board, false, 60000), 0);
        const std::string json = final_metrics();
        check_metrics(json, agent, 24);
        EXPECT_EQ(metric(json, "depth_completed"), 2);
    }
}

TEST_F(SearchMetricsTest, TimedSerialScoresSingleRootCandidate)
{
    Board board;
    const int winning_move = board.pos(2, 4);
    for (int col = 0; col < 4; ++col)
        board.make_move(board.pos(2, col), true);
    for (int pos = 0; pos < board.squares(); ++pos)
    {
        if (pos != winning_move && !board.test_pos(pos))
            board.make_move(pos, false);
    }

    const auto black = board.black;
    const auto white = board.white;
    MinimaxAgent agent(3);

    EXPECT_EQ(agent.get_best_move_timed(board, true, 60000), winning_move);
    ASSERT_EQ(agent.get_last_root_candidates().size(), 1u);
    EXPECT_EQ(agent.get_last_root_candidates()[0].move, winning_move);
    EXPECT_GT(agent.get_last_root_candidates()[0].score, 90000.0f);
    EXPECT_GT(agent.get_last_root_score(), 90000.0f);
    EXPECT_EQ(board.black, black);
    EXPECT_EQ(board.white, white);
}

TEST_F(SearchMetricsTest, TimedRootCandidatesHaveIndependentExactScores)
{
    Board board;
    board.make_move(board.pos(2, 2), true);
    const auto black = board.black;
    const auto white = board.white;
    Evaluator evaluator;
    constexpr int depth = 2;
    MinimaxAgent agent(depth, 4096);

    for (bool black_to_move : {true, false})
    {
        agent.clear_tt();
        ASSERT_GE(agent.get_best_move_timed(board, black_to_move, 60000), 0);
        const std::vector<RootCandidate>& candidates = agent.get_last_root_candidates();
        ASSERT_EQ(candidates.size(), evaluator.get_valid_moves(board).size());

        for (const RootCandidate& candidate : candidates)
        {
            Board after_root = board;
            after_root.make_move(candidate.move, black_to_move);
            float expected;
            if (after_root.wins_at(candidate.move, black_to_move))
            {
                expected = black_to_move ? 100000.0f : -100000.0f;
            }
            else
            {
                const bool opponent_is_black = !black_to_move;
                const float initial = opponent_is_black ? -std::numeric_limits<float>::infinity()
                                                        : std::numeric_limits<float>::infinity();
                expected = initial;
                const std::vector<int> replies = evaluator.get_valid_moves(after_root);
                for (int reply : replies)
                {
                    after_root.make_move(reply, opponent_is_black);
                    const float score = after_root.wins_at(reply, opponent_is_black)
                                            ? (opponent_is_black ? 99999.0f : -99999.0f)
                                            : evaluator.evaluate_board(after_root);
                    after_root.undo_move(reply);
                    expected =
                        opponent_is_black ? std::max(expected, score) : std::min(expected, score);
                }
                if (replies.empty())
                    expected = 0.0f;
            }
            EXPECT_FLOAT_EQ(candidate.score, expected) << "move " << candidate.move;
        }

        EXPECT_EQ(board.black, black);
        EXPECT_EQ(board.white, white);
    }
}

TEST_F(SearchMetricsTest, EarlyReturnsResetCounters)
{
    MinimaxAgent agent(2);
    Board board;
    board.make_move(board.pos(2, 2), true);
    agent.get_best_move(board, false);
    EXPECT_GT(agent.get_nodes_searched(), 0);
    for (unsigned int cores : {1u, 2u})
    {
        g_config.cores = cores;
        Board empty;
        EXPECT_EQ(agent.get_best_move_timed_smp(empty, true, 100), empty.pos(2, 2));
        std::string json = final_metrics();
        check_metrics(json, agent, 1);
        if (cores == 1)
        {
            EXPECT_GT(metric(json, "tt_probes_total"), 0);
            EXPECT_EQ(metric(json, "depth_completed"), agent.get_max_depth());
        }
        else
        {
            EXPECT_EQ(metric(json, "tt_probes_total"), 0);
            EXPECT_EQ(metric(json, "depth_completed"), 0);
        }

        Board full;
        for (int i = 0; i < g_config.squares(); ++i)
            full.make_move(i, true);
        EXPECT_EQ(agent.get_best_move_timed_smp(full, false, 100), -1);
        json = final_metrics();
        check_metrics(json, agent, 0);
        EXPECT_EQ(metric(json, "depth_completed"), 0);
        EXPECT_NE(json.find("\"best_move\":null"), std::string::npos);
    }
    g_config.debug_output = false;
    diagnostics.str("");
    Board empty;
    agent.get_best_move_timed(empty, true, 100);
    EXPECT_TRUE(diagnostics.str().empty());
}

TEST_F(SearchMetricsTest, LiveWorkersAndTimedDepth)
{
    g_config.board_size = 7;
    g_config.metrics_logging_interval = 1;
    Board board;
    board.make_move(board.pos(3, 3), true);
    MinimaxAgent agent(20);
    for (unsigned int cores : {1u, 2u})
    {
        g_config.cores = cores;
        diagnostics.str("");
        EXPECT_GE(agent.get_best_move_timed_smp(board, false, 200), 0);
        const std::string json = final_metrics();
        check_metrics(json, agent, 24);
        EXPECT_LT(metric(json, "depth_completed"), 20);
        if (cores == 1)
        {
            EXPECT_LE(metric(json, "depth_completed"), metric(json, "depth_current"));
        }

        std::istringstream lines(diagnostics.str());
        std::string line;
        int progress_events = 0;
        while (std::getline(lines, line))
        {
            if (line.find("\"event\":\"search_progress\"") == std::string::npos)
                continue;
            ++progress_events;
            EXPECT_GE(metric(line, "workers_active"), 1);
            EXPECT_LE(metric(line, "workers_active"), cores);
            EXPECT_EQ(metric(line, "root_candidates_count"), 24);
            EXPECT_LT(metric(line, "depth_completed"), 20);
            EXPECT_GE(metric(line, "tt_probes_total"), 0);
            EXPECT_GE(metric(line, "alpha_beta_cutoffs_total"), 0);
        }
        EXPECT_GT(progress_events, 0);
    }
}