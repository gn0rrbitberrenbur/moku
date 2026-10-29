#include "nnue.hpp"
#include "board.hpp"
#include "config.hpp"
#include "minimax/minimax.hpp"
#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <vector>

namespace {

// Pins a known board size so every test runs against a deterministic geometry.
class MokuContract : public ::testing::Test {
protected:
    void SetUp() override { g_config.board_size = 15; }
    void TearDown() override { g_config.board_size = 15; }

    static int rc(int row, int col) { return row * g_config.board_size + col; }
};

TEST_F(MokuContract, MaxSquaresMatchesNineteenSquared) {
    EXPECT_EQ(MAX_SQUARES, 19 * 19);
}

TEST_F(MokuContract, ConfigSquaresFollowsBoardSize) {
    g_config.board_size = 15;
    EXPECT_EQ(g_config.squares(), 225);
    g_config.board_size = 19;
    EXPECT_EQ(g_config.squares(), 361);
}

TEST_F(MokuContract, MakeAndUndoMoveTracksStones) {
    Board board;
    EXPECT_EQ(board.black.count(), std::size_t{0});
    EXPECT_EQ(board.white.count(), std::size_t{0});

    const int p = rc(7, 7);
    board.make_move(p, true);
    EXPECT_TRUE(board.black.test(p));
    EXPECT_EQ(board.black.count(), std::size_t{1});
    EXPECT_EQ(board.white.count(), std::size_t{0});

    board.undo_move(p);
    EXPECT_FALSE(board.black.test(p));
    EXPECT_EQ(board.black.count(), std::size_t{0});

    board.make_move(p, false);
    EXPECT_TRUE(board.white.test(p));
    EXPECT_EQ(board.white.count(), std::size_t{1});
}

TEST_F(MokuContract, WinsAtDetectsFiveInARow) {
    Board board;
    for (int c = 0; c < 5; ++c) board.make_move(rc(3, c), true);
    EXPECT_TRUE(board.wins_at(rc(3, 4), true));
    EXPECT_TRUE(board.wins_at(rc(3, 2), true));
    EXPECT_FALSE(board.wins_at(rc(3, 4), false));
}

TEST_F(MokuContract, WinsAtRejectsFourInARow) {
    Board board;
    for (int c = 0; c < 4; ++c) board.make_move(rc(3, c), true);
    EXPECT_FALSE(board.wins_at(rc(3, 3), true));
}

TEST_F(MokuContract, ValidMovesOnEmptyBoardIsCenter) {
    Board board;
    Evaluator eval;
    const std::vector<int> moves = eval.get_valid_moves(board);
    ASSERT_EQ(moves.size(), std::size_t{1});
    EXPECT_EQ(moves[0], rc(7, 7));
}

TEST_F(MokuContract, ValidMovesAreEmptyNeighborsOfStones) {
    Board board;
    Evaluator eval;
    const int center = rc(7, 7);
    board.make_move(center, true);

    const std::vector<int> moves = eval.get_valid_moves(board);
    ASSERT_FALSE(moves.empty());
    for (int m : moves) {
        EXPECT_FALSE(board.black.test(m));
        EXPECT_FALSE(board.white.test(m));
        const int dr = std::abs(m / g_config.board_size - 7);
        const int dc = std::abs(m % g_config.board_size - 7);
        EXPECT_LE(std::max(dr, dc), 2);
    }
    EXPECT_EQ(std::find(moves.begin(), moves.end(), center), moves.end());
}

TEST_F(MokuContract, BestMoveOnEmptyBoardReturnsCenter) {
    Board board;
    MinimaxAgent agent(4);
    agent.clear_tt();

    const int move = agent.get_best_move_timed(board, true, 100);
    EXPECT_EQ(move, rc(7, 7));

    const std::vector<RootCandidate>& cands = agent.get_last_root_candidates();
    ASSERT_EQ(cands.size(), std::size_t{1});
    EXPECT_EQ(cands[0].move, rc(7, 7));
    EXPECT_FLOAT_EQ(agent.get_last_root_score(), 0.0f);
}

TEST_F(MokuContract, BestMoveReturnsValidCandidate) {
    Board board;
    board.make_move(rc(7, 7), true);
    board.make_move(rc(7, 8), false);

    Evaluator eval;
    const std::vector<int> valid = eval.get_valid_moves(board);

    MinimaxAgent agent(4);
    agent.clear_tt();
    const int move = agent.get_best_move_timed(board, true, 100);

    ASSERT_GE(move, 0);
    EXPECT_NE(std::find(valid.begin(), valid.end(), move), valid.end());

    const std::vector<RootCandidate>& cands = agent.get_last_root_candidates();
    ASSERT_FALSE(cands.empty());
    for (const RootCandidate& cand : cands) {
        EXPECT_NE(std::find(valid.begin(), valid.end(), cand.move), valid.end());
    }
}

// Tactical check: with four black stones in a row, the engine must complete the win.
TEST_F(MokuContract, BestMoveTakesImmediateWin) {
    Board board;
    for (int c = 0; c < 4; ++c) board.make_move(rc(5, c), true);

    MinimaxAgent agent(4);
    agent.clear_tt();
    const int move = agent.get_best_move_timed(board, true, 200);
    EXPECT_EQ(move, rc(5, 4));
}

}  // namespace