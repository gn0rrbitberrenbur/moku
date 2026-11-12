#include "config.hpp"
#include "game/board.hpp"

#include <gtest/gtest.h>

// Test cases for the Board class
TEST(BoardTest, InvalidMakeMove)
{
    Board board;
    board.make_move(g_config.squares(), false);
}

TEST(BoardTest, ValidMakeMove)
{
    Board board;
    board.make_move(0, true);   // Black at A1
    board.make_move(15, false); // White at A2

    EXPECT_TRUE(board.black[0]);
    EXPECT_FALSE(board.white[0]);
    EXPECT_FALSE(board.black[15]);
    EXPECT_TRUE(board.white[15]);
}

TEST(BoardTest, ValidUndoMove)
{
    Board board;
    board.make_move(0, true);   // Black at A1
    board.make_move(15, false); // White at A2
    board.undo_move(0);
    board.undo_move(15);
    EXPECT_FALSE(board.test_pos(0));
    EXPECT_FALSE(board.test_pos(15));
}

TEST(BoardTest, ValidTestPos)
{
    Board board;
    board.make_move(0, true);   // Black at A1
    board.make_move(15, false); // White at A2
    EXPECT_TRUE(board.test_pos(0));
    EXPECT_TRUE(board.test_pos(15));
}

TEST(BoardTest, InvalidTestPos)
{
    Board board;
    board.make_move(0, true);   // Black at A1
    board.make_move(15, false); // White at A2
    EXPECT_TRUE(board.test_pos(0));
    EXPECT_TRUE(board.test_pos(15));
}

TEST(BoardTest, ValidCheckWinBlack)
{
    Board board;
    board.make_move(0, true); // A1
    board.make_move(1, true); // B1
    board.make_move(2, true); // C1
    board.make_move(3, true); // D1
    board.make_move(4, true); // E1

    EXPECT_TRUE(board.check_win());
}

TEST(BoardTest, InvalidCheckWinBlack)
{
    Board board;
    board.make_move(0, true); // A1
    board.make_move(1, true); // B1
    board.make_move(2, true); // C1
    board.make_move(3, true); // D1

    EXPECT_FALSE(board.check_win());
}

TEST(BoardTest, ValidCheckWinWhite)
{
    Board board;
    board.make_move(0, false); // A1
    board.make_move(1, false); // B1
    board.make_move(2, false); // C1
    board.make_move(3, false); // D1
    board.make_move(4, false); // E1

    EXPECT_TRUE(board.check_win());
}

TEST(BoardTest, InvalidCheckWinWhite)
{
    Board board;
    board.make_move(0, false); // A1
    board.make_move(1, false); // B1
    board.make_move(2, false); // C1
    board.make_move(3, false); // D1

    EXPECT_FALSE(board.check_win());
}

TEST(BoardTest, ValidCheckWinBlackVertical)
{
    Board board;
    board.make_move(0, true);  // A1
    board.make_move(15, true); // A2
    board.make_move(30, true); // A3
    board.make_move(45, true); // A4
    board.make_move(60, true); // A5

    EXPECT_TRUE(board.check_win());
}

TEST(BoardTest, InvalidCheckWinBlackVertical)
{
    Board board;
    board.make_move(0, true);  // A1
    board.make_move(15, true); // A2
    board.make_move(30, true); // A3
    board.make_move(45, true); // A4

    EXPECT_FALSE(board.check_win());
}

TEST(BoardTest, ValidCheckWinWhiteDiagonal)
{
    Board board;
    board.make_move(0, false);  // A1
    board.make_move(16, false); // B2
    board.make_move(32, false); // C3
    board.make_move(48, false); // D4
    board.make_move(64, false); // E5

    EXPECT_TRUE(board.check_win());
}

TEST(BoardTest, InvalidCheckWinWhiteDiagonal)
{
    Board board;
    board.make_move(0, false);  // A1
    board.make_move(16, false); // B2
    board.make_move(32, false); // C3
    board.make_move(48, false); // D4

    EXPECT_FALSE(board.check_win());
}

TEST(BoardTest, CheckWinsAt)
{
    Board board;
    board.make_move(0, true); // A1
    board.make_move(1, true); // B1
    board.make_move(2, true); // C1
    board.make_move(3, true); // D1
    board.make_move(4, true); // E1

    EXPECT_TRUE(board.wins_at(4, true));   // Black wins at E1
    EXPECT_FALSE(board.wins_at(4, false)); // White does not win at E1
}