#include "config.hpp"
#include "eval/evaluation.hpp"

#include <gtest/gtest.h>

// Test cases for the Evaluator class
TEST(EvaluatorTest, ScorePatternFiveInRow)
{
    Evaluator eval;
    EXPECT_GE(eval.score_pattern(5, 2), 100000); // win
    EXPECT_GE(eval.score_pattern(5, 1), 100000);
    EXPECT_GE(eval.score_pattern(5, 0), 100000);
}

TEST(EvaluatorTest, ScorePatternFourInRow)
{
    Evaluator eval;
    EXPECT_EQ(eval.score_pattern(4, 2), 50000); // Open-4
    EXPECT_EQ(eval.score_pattern(4, 1), 5000);  // Half-open 4
    EXPECT_EQ(eval.score_pattern(4, 0), 500);   // Blocked 4
}

TEST(EvaluatorTest, ScorePatternThreeInRow)
{
    Evaluator eval;
    EXPECT_EQ(eval.score_pattern(3, 2), 2000); // Open-3
    EXPECT_EQ(eval.score_pattern(3, 1), 200);  // Half-open 3
    EXPECT_EQ(eval.score_pattern(3, 0), 20);   // Blocked 3
}

TEST(EvaluatorTest, ScorePatternTwoInRow)
{
    Evaluator eval;
    EXPECT_EQ(eval.score_pattern(2, 2), 50);
    EXPECT_EQ(eval.score_pattern(2, 1), 10);
    EXPECT_EQ(eval.score_pattern(2, 0), 2);
}

TEST(EvaluatorTest, ScorePatternSingle)
{
    Evaluator eval;
    EXPECT_EQ(eval.score_pattern(1, 2), 3);
    EXPECT_EQ(eval.score_pattern(1, 1), 0);
    EXPECT_EQ(eval.score_pattern(1, 0), 0);
}

TEST(EvaluatorTest, PositionScoreCenter)
{
    Evaluator eval;
    int center_pos = 7 * 15 + 7; // Middle of board (H8)
    int corner_pos = 0;          // Corner (A1)

    EXPECT_GT(eval.position_score(center_pos), eval.position_score(corner_pos));
}

TEST(EvaluatorTest, PositionScoreSymmetry)
{
    Evaluator eval;
    // Symmetric positions should have the same score
    int pos1 = 3 * 15 + 3;   // D4
    int pos2 = 11 * 15 + 11; // L12

    EXPECT_EQ(eval.position_score(pos1), eval.position_score(pos2));
}

TEST(EvaluatorTest, AnalyzeLineHorizontal)
{
    Board board;
    Evaluator eval;

    board.make_move(4 * 15 + 1, true); // B5
    board.make_move(4 * 15 + 2, true); // C5
    board.make_move(4 * 15 + 3, true); // D5

    auto info = eval.analyze_line(board, 4 * 15 + 1, 1, 0, true);
    EXPECT_EQ(info.count, 3);
    EXPECT_EQ(info.open_ends, 2);
}

TEST(EvaluatorTest, AnalyzeLineVertical)
{
    Board board;
    Evaluator eval;

    board.make_move(2 * 15 + 4, true); // E3
    board.make_move(3 * 15 + 4, true); // E4
    board.make_move(4 * 15 + 4, true); // E5

    auto info = eval.analyze_line(board, 2 * 15 + 4, 0, 1, true);
    EXPECT_EQ(info.count, 3);
    EXPECT_EQ(info.open_ends, 2);
}

TEST(EvaluatorTest, AnalyzeLineAtEdge)
{
    Board board;
    Evaluator eval;

    board.make_move(0, true); // A1
    board.make_move(1, true); // B1
    board.make_move(2, true); // C1

    auto info = eval.analyze_line(board, 0, 1, 0, true);
    EXPECT_EQ(info.count, 3);
    EXPECT_EQ(info.open_ends, 1);
}

TEST(EvaluatorTest, EvaluateBoardEmpty)
{
    Board board;
    Evaluator eval;

    float score = eval.evaluate_board(board);
    EXPECT_EQ(score, 0.0f); // Empty board should have neutral score
}

TEST(EvaluatorTest, EvaluateBoardBlackAdvantage)
{
    Board board;
    Evaluator eval;

    board.make_move(0, true);
    board.make_move(1, true);
    board.make_move(2, true);
    board.make_move(100, false);

    float score = eval.evaluate_board(board);
    EXPECT_GT(score, 0.0f); // Black in advantage = positive score
}

TEST(EvaluatorTest, EvaluateBoardWhiteAdvantage)
{
    Board board;
    Evaluator eval;

    board.make_move(0, false);
    board.make_move(1, false);
    board.make_move(2, false);
    board.make_move(100, true);

    float score = eval.evaluate_board(board);
    EXPECT_LT(score, 0.0f); // White in advantage = negative score
}

TEST(EvaluatorTest, EvaluateBoardWinBlack)
{
    Board board;
    Evaluator eval;

    // Black wins
    board.make_move(0, true);
    board.make_move(1, true);
    board.make_move(2, true);
    board.make_move(3, true);
    board.make_move(4, true);

    float score = eval.evaluate_board(board);
    EXPECT_GE(score, 100000.0f);
}

TEST(EvaluatorTest, EvaluateBoardWinWhite)
{
    Board board;
    Evaluator eval;

    // White wins
    board.make_move(0, false);
    board.make_move(1, false);
    board.make_move(2, false);
    board.make_move(3, false);
    board.make_move(4, false);

    float score = eval.evaluate_board(board);
    EXPECT_LE(score, -100000.0f);
}

TEST(EvaluatorTest, GetValidMovesEmpty)
{
    Board board;
    Evaluator eval;

    auto moves = eval.get_valid_moves(board);

    EXPECT_EQ(moves.size(), 1);
    EXPECT_EQ(moves[0], 7 * 15 + 7);
}

TEST(EvaluatorTest, GetValidMovesNearStones)
{
    Board board;
    Evaluator eval;

    board.make_move(7 * 15 + 7, true);

    auto moves = eval.get_valid_moves(board);

    EXPECT_GT(moves.size(), 1);

    bool center_in_moves = false;
    for (int m : moves)
    {
        if (m == 7 * 15 + 7)
            center_in_moves = true;
    }
    EXPECT_FALSE(center_in_moves);
}

TEST(EvaluatorTest, GetValidMovesNoFarPositions)
{
    Board board;
    Evaluator eval;

    board.make_move(0, true);

    auto moves = eval.get_valid_moves(board);

    int far_pos = 14 * 15 + 14;
    bool far_in_moves = false;
    for (int m : moves)
    {
        if (m == far_pos)
            far_in_moves = true;
    }
    EXPECT_FALSE(far_in_moves);
}

TEST(EvaluatorTest, AnalyzeLineDiagonalDownRight)
{
    Board board;
    Evaluator eval;
    int SIZE = g_config.board_size;

    board.make_move(2 * SIZE + 2, true); // C3
    board.make_move(3 * SIZE + 3, true); // D4
    board.make_move(4 * SIZE + 4, true); // E5

    auto info = eval.analyze_line(board, 2 * SIZE + 2, 1, 1, true);
    EXPECT_EQ(info.count, 3);
    EXPECT_EQ(info.open_ends, 2);
}

TEST(EvaluatorTest, AnalyzeLineDiagonalUpRight)
{
    Board board;
    Evaluator eval;
    int SIZE = g_config.board_size;

    board.make_move(4 * SIZE + 2, true); // C5
    board.make_move(3 * SIZE + 3, true); // D4
    board.make_move(2 * SIZE + 4, true); // E3

    auto info = eval.analyze_line(board, 4 * SIZE + 2, 1, -1, true);
    EXPECT_EQ(info.count, 3);
    EXPECT_EQ(info.open_ends, 2);
}

TEST(EvaluatorTest, ScoreWindowDiagonal)
{
    Evaluator eval;
    Board board;
    int SIZE = g_config.board_size;
    int base = 2 * SIZE + 2; // C3

    // diagonal three: C3 D4 E5
    board.make_move(base, true);
    board.make_move(base + SIZE + 1, true);
    board.make_move(base + 2 * (SIZE + 1), true);

    EXPECT_EQ(eval.score_window(board, base, 1, 1, true), 120);
}

TEST(EvaluatorTest, ScoreWindowTwoPieces)
{
    Evaluator eval;
    Board board;
    int base = (g_config.board_size / 2) * g_config.board_size + 5;

    board.make_move(base, true);
    board.make_move(base + 1, true);

    EXPECT_GT(eval.score_window(board, base, 1, 0, true), 0);
}

TEST(EvaluatorTest, ScoreWindowSinglePiece)
{
    Evaluator eval;
    Board board;
    int base = (g_config.board_size / 2) * g_config.board_size + 5;

    board.make_move(base, true);

    EXPECT_GT(eval.score_window(board, base, 1, 0, true), 0);
}

// Test cases for the move_priority function
TEST(EvaluatorTest, MovePriorityWinningMoveHighest)
{
    Evaluator eval;
    Board board;
    int SIZE = g_config.board_size;
    int row = SIZE / 2;
    int base = row * SIZE + 3;

    // black has four in a row: base..base+3, winning move at base+4
    board.make_move(base, true);
    board.make_move(base + 1, true);
    board.make_move(base + 2, true);
    board.make_move(base + 3, true);

    int win_move = base + 4;
    int far_move = 0; // A1 corner

    EXPECT_GT(eval.move_priority(board, win_move, true), eval.move_priority(board, far_move, true));
}

TEST(EvaluatorTest, MovePriorityBlockingOpponentThreat)
{
    Evaluator eval;
    Board board;
    int SIZE = g_config.board_size;
    int row = SIZE / 2;
    int base = row * SIZE + 3;

    // white (opponent) has three in a row -> black should prioritize blocking
    board.make_move(base, false);
    board.make_move(base + 1, false);
    board.make_move(base + 2, false);

    int block_move = base + 3;
    int far_move = 0;

    EXPECT_GT(eval.move_priority(board, block_move, true),
              eval.move_priority(board, far_move, true));
}

TEST(EvaluatorTest, MovePriorityCenterOverEdge)
{
    Evaluator eval;
    Board board;
    int SIZE = g_config.board_size;
    int center = (SIZE / 2) * SIZE + (SIZE / 2);
    int edge = 0;

    EXPECT_GT(eval.move_priority(board, center, true), eval.move_priority(board, edge, true));
}

// Test cases for the is_line_dead function
TEST(EvaluatorTest, IsLineDeadBothEndsBlocked)
{
    Evaluator eval;
    Board board;
    // O X X X O  -> black run of 3 blocked on both ends
    int row = (g_config.board_size / 2);
    int base = row * g_config.board_size + 5;

    board.make_move(base - 1, false); // O (white) before
    board.make_move(base, true);      // X
    board.make_move(base + 1, true);  // X
    board.make_move(base + 2, true);  // X
    board.make_move(base + 3, false); // O (white) after

    // run starts at base, length 3, horizontal (dx=1, dy=0)
    EXPECT_TRUE(eval.is_line_dead(board, base, 1, 0, 3, true));
}

TEST(EvaluatorTest, IsLineDeadOneEndOpen)
{
    Evaluator eval;
    Board board;
    // O X X X _  -> blocked only on the front end
    int row = (g_config.board_size / 2);
    int base = row * g_config.board_size + 5;

    board.make_move(base - 1, false); // O before
    board.make_move(base, true);      // X
    board.make_move(base + 1, true);  // X
    board.make_move(base + 2, true);  // X
    // base + 3 stays empty

    EXPECT_FALSE(eval.is_line_dead(board, base, 1, 0, 3, true));
}

TEST(EvaluatorTest, IsLineDeadBothEndsOpen)
{
    Evaluator eval;
    Board board;
    // _ X X X _  -> open on both ends
    int row = (g_config.board_size / 2);
    int base = row * g_config.board_size + 5;

    board.make_move(base, true);     // X
    board.make_move(base + 1, true); // X
    board.make_move(base + 2, true); // X

    EXPECT_FALSE(eval.is_line_dead(board, base, 1, 0, 3, true));
}

TEST(EvaluatorTest, IsLineDeadBlockedByEdge)
{
    Evaluator eval;
    Board board;
    // run starts at column 0, so the front end is the board edge (blocked),
    // and we block the back end with white -> dead
    int row = (g_config.board_size / 2);
    int base = row * g_config.board_size + 0; // column 0

    board.make_move(base, true);      // X at edge
    board.make_move(base + 1, true);  // X
    board.make_move(base + 2, true);  // X
    board.make_move(base + 3, false); // O after

    // front is edge (blocked), back blocked by white -> dead
    EXPECT_TRUE(eval.is_line_dead(board, base, 1, 0, 3, true));
}

TEST(EvaluatorTest, IsLineDeadVerticalBlocked)
{
    Evaluator eval;
    Board board;
    int SIZE = g_config.board_size;
    int col = SIZE / 2;
    int startRow = 4;
    int base = startRow * SIZE + col;

    // vertical: O X X X O
    board.make_move(base - SIZE, false);     // O above
    board.make_move(base, true);             // X
    board.make_move(base + SIZE, true);      // X
    board.make_move(base + 2 * SIZE, true);  // X
    board.make_move(base + 3 * SIZE, false); // O below

    EXPECT_TRUE(eval.is_line_dead(board, base, 0, 1, 3, true));
}

// Test cases for the score_window function
TEST(EvaluatorTest, ScoreWindowEmptyWindow)
{
    Evaluator eval;
    Board board;
    int row = g_config.board_size / 2;
    int base = row * g_config.board_size + 5;

    // empty window -> 0
    EXPECT_EQ(eval.score_window(board, base, 1, 0, true), 0);
}

TEST(EvaluatorTest, ScoreWindowBlockedByOpponent)
{
    Evaluator eval;
    Board board;
    int row = g_config.board_size / 2;
    int base = row * g_config.board_size + 5;

    // X X O _ _  -> contains opponent stone -> dead window -> 0
    board.make_move(base, true);
    board.make_move(base + 1, true);
    board.make_move(base + 2, false); // opponent

    EXPECT_EQ(eval.score_window(board, base, 1, 0, true), 0);
}

TEST(EvaluatorTest, ScoreWindowBrokenFour)
{
    Evaluator eval;
    Board board;
    int row = g_config.board_size / 2;
    int base = row * g_config.board_size + 5;

    // X X _ X X  -> broken four, own_count == 4
    board.make_move(base, true);
    board.make_move(base + 1, true);
    // base + 2 gap
    board.make_move(base + 3, true);
    board.make_move(base + 4, true);

    // expect the "4 stones" score (1200 per current WINDOW_SCORE table)
    EXPECT_EQ(eval.score_window(board, base, 1, 0, true), 1200);
}

TEST(EvaluatorTest, ScoreWindowSolidFive)
{
    Evaluator eval;
    Board board;
    int row = g_config.board_size / 2;
    int base = row * g_config.board_size + 5;

    // X X X X X -> five in a row, own_count == 5
    for (int i = 0; i < 5; i++)
    {
        board.make_move(base + i, true);
    }

    EXPECT_EQ(eval.score_window(board, base, 1, 0, true), 100000);
}

TEST(EvaluatorTest, ScoreWindowRunsOffBoard)
{
    Evaluator eval;
    Board board;
    int SIZE = g_config.board_size;
    int row = SIZE / 2;
    // start near the right edge so a horizontal window of 5 cannot fit
    int base = row * SIZE + (SIZE - 2);

    board.make_move(base, true);

    // window cannot fit on the board -> 0
    EXPECT_EQ(eval.score_window(board, base, 1, 0, true), 0);
}

TEST(EvaluatorTest, ScoreWindowWhitePieces)
{
    Evaluator eval;
    Board board;
    int row = g_config.board_size / 2;
    int base = row * g_config.board_size + 5;

    // three white stones, no black -> 3-stone score for white
    board.make_move(base, false);
    board.make_move(base + 1, false);
    board.make_move(base + 2, false);

    EXPECT_EQ(eval.score_window(board, base, 1, 0, false), 120);
}