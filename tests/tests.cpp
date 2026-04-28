#include "../include/board.hpp"
#include "../include/utils.hpp"
#include "../include/agents/evaluation.hpp"
#include <gtest/gtest.h>
#include <iostream>

TEST(AlgebraicConversion, ValidInputs) 
{
    EXPECT_EQ(algebraic_to_index("A1", Board::SIZE), 0);
    EXPECT_EQ(algebraic_to_index("A2", Board::SIZE), 15);
    EXPECT_EQ(algebraic_to_index("B1", Board::SIZE), 1);
    EXPECT_EQ(algebraic_to_index("b1", Board::SIZE), 1);
    EXPECT_EQ(algebraic_to_index("B2", Board::SIZE), 16);
    EXPECT_EQ(algebraic_to_index("a3", Board::SIZE), 30);
    EXPECT_EQ(algebraic_to_index("A4", Board::SIZE), 45);
    EXPECT_EQ(algebraic_to_index("O15", Board::SIZE), 224);
}

TEST(AlgebraicConversion, InvalidInputs) 
{
    EXPECT_EQ(algebraic_to_index("", Board::SIZE), -1);
    EXPECT_EQ(algebraic_to_index("Z1", Board::SIZE), -1);
    EXPECT_EQ(algebraic_to_index("A0", Board::SIZE), -1);
    EXPECT_EQ(algebraic_to_index("A16", Board::SIZE), -1);
}

TEST(BoardTest, ValidMakeMove) 
{
    Board board;
    board.make_move(0, true); // Black at A1
    board.make_move(15, false); // White at A2

    EXPECT_TRUE(board.black[0]);
    EXPECT_FALSE(board.white[0]);
    EXPECT_FALSE(board.black[15]);
    EXPECT_TRUE(board.white[15]);
}

TEST(BoardTest, InvalidMakeMove) 
{
    Board board;
    board.make_move(225, true); // Invalid position
    board.make_move(Board::SIZE * Board::SIZE, false); // Invalid position

    EXPECT_TRUE(board.black.none());
    EXPECT_TRUE(board.white.none());
}

TEST(BoardTest, ValidUndoMove)
{
    Board board;
    board.make_move(0, true); // Black at A1
    board.make_move(15, false); // White at A2
    board.undo_move(0);   // Nur 1 Parameter
    board.undo_move(15);  // Nur 1 Parameter
    EXPECT_FALSE(board.test_pos(0));
    EXPECT_FALSE(board.test_pos(15));
}

TEST(BoardTest, ValidTestPos) 
{
    Board board;
    board.make_move(0, true); // Black at A1
    board.make_move(15, false); // White at A2
    EXPECT_TRUE(board.test_pos(0));
    EXPECT_TRUE(board.test_pos(15));
}

TEST(BoardTest, InvalidTestPos) 
{
    Board board;
    board.make_move(0, true); // Black at A1
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
    board.make_move(0, true); // A1
    board.make_move(15, true); // A2
    board.make_move(30, true); // A3
    board.make_move(45, true); // A4
    board.make_move(60, true); // A5

    EXPECT_TRUE(board.check_win());
}

TEST(BoardTest, InvalidCheckWinBlackVertical) 
{
    Board board;
    board.make_move(0, true); // A1
    board.make_move(15, true); // A2
    board.make_move(30, true); // A3
    board.make_move(45, true); // A4

    EXPECT_FALSE(board.check_win());
}

// Evaluator Tests

TEST(EvaluatorTest, ScorePatternFiveInRow)
{
    Evaluator eval;
    EXPECT_EQ(eval.score_pattern(5, 0), 100000);
    EXPECT_EQ(eval.score_pattern(5, 1), 100000);
    EXPECT_EQ(eval.score_pattern(5, 2), 100000);
    EXPECT_EQ(eval.score_pattern(6, 2), 100000);
}

TEST(EvaluatorTest, ScorePatternFourInRow)
{
    Evaluator eval;
    EXPECT_EQ(eval.score_pattern(4, 2), 10000);  // Offene 4 = sehr gefährlich
    EXPECT_EQ(eval.score_pattern(4, 1), 1000);   // Halb-offene 4
    EXPECT_EQ(eval.score_pattern(4, 0), 1000);   // Geschlossene 4
}

TEST(EvaluatorTest, ScorePatternThreeInRow)
{
    Evaluator eval;
    EXPECT_EQ(eval.score_pattern(3, 2), 1000);   // Offene 3
    EXPECT_EQ(eval.score_pattern(3, 1), 100);    // Halb-offene 3
    EXPECT_EQ(eval.score_pattern(3, 0), 100);    // Geschlossene 3
}

TEST(EvaluatorTest, ScorePatternTwoInRow)
{
    Evaluator eval;
    EXPECT_EQ(eval.score_pattern(2, 2), 50);     // Offene 2
    EXPECT_EQ(eval.score_pattern(2, 1), 10);     // Halb-offene 2
    EXPECT_EQ(eval.score_pattern(2, 0), 10);     // Geschlossene 2
}

TEST(EvaluatorTest, ScorePatternSingle)
{
    Evaluator eval;
    EXPECT_EQ(eval.score_pattern(1, 2), 0);
    EXPECT_EQ(eval.score_pattern(0, 2), 0);
}

TEST(EvaluatorTest, PositionScoreCenter)
{
    Evaluator eval;
    int center_pos = 7 * 15 + 7;  // Mitte des Boards (H8)
    int corner_pos = 0;           // Ecke (A1)
    
    EXPECT_GT(eval.position_score(center_pos), eval.position_score(corner_pos));
}

TEST(EvaluatorTest, PositionScoreSymmetry)
{
    Evaluator eval;
    // Symmetrische Positionen sollten gleichen Score haben
    int pos1 = 3 * 15 + 3;   // D4
    int pos2 = 11 * 15 + 11; // L12
    
    EXPECT_EQ(eval.position_score(pos1), eval.position_score(pos2));
}

TEST(EvaluatorTest, AnalyzeLineHorizontal)
{
    Board board;
    Evaluator eval;
    
    // 3 schwarze Steine horizontal in der Mitte: B5, C5, D5
    board.make_move(4 * 15 + 1, true);  // B5
    board.make_move(4 * 15 + 2, true);  // C5
    board.make_move(4 * 15 + 3, true);  // D5
    
    auto info = eval.analyze_line(board, 4 * 15 + 2, 1, 0, true);  // Von C5 horizontal
    EXPECT_EQ(info.count, 3);
    EXPECT_EQ(info.open_ends, 2);  // Beide Enden frei
}

TEST(EvaluatorTest, AnalyzeLineVertical)
{
    Board board;
    Evaluator eval;
    
    // 3 schwarze Steine vertikal in der Mitte: E3, E4, E5
    board.make_move(2 * 15 + 4, true);  // E3
    board.make_move(3 * 15 + 4, true);  // E4
    board.make_move(4 * 15 + 4, true);  // E5
    
    auto info = eval.analyze_line(board, 3 * 15 + 4, 0, 1, true);  // Von E4 vertikal
    EXPECT_EQ(info.count, 3);
    EXPECT_EQ(info.open_ends, 2);
}

TEST(EvaluatorTest, AnalyzeLineAtEdge)
{
    Board board;
    Evaluator eval;
    
    // 3 schwarze Steine am Rand: A1, B1, C1
    board.make_move(0, true);
    board.make_move(1, true);
    board.make_move(2, true);
    
    auto info = eval.analyze_line(board, 1, 1, 0, true);
    EXPECT_EQ(info.count, 3);
    EXPECT_EQ(info.open_ends, 1);  // Nur rechtes Ende frei, links ist Rand
}

TEST(EvaluatorTest, EvaluateBoardEmpty)
{
    Board board;
    Evaluator eval;
    
    float score = eval.evaluate_board(board);
    EXPECT_EQ(score, 0.0f);  // Leeres Board = neutraler Score
}

TEST(EvaluatorTest, EvaluateBoardBlackAdvantage)
{
    Board board;
    Evaluator eval;
    
    // Schwarz hat 3 in Reihe, Weiß hat nur 1
    board.make_move(0, true);
    board.make_move(1, true);
    board.make_move(2, true);
    board.make_move(100, false);
    
    float score = eval.evaluate_board(board);
    EXPECT_GT(score, 0.0f);  // Schwarz im Vorteil = positiv
}

TEST(EvaluatorTest, EvaluateBoardWhiteAdvantage)
{
    Board board;
    Evaluator eval;
    
    // Weiß hat 3 in Reihe, Schwarz hat nur 1
    board.make_move(0, false);
    board.make_move(1, false);
    board.make_move(2, false);
    board.make_move(100, true);
    
    float score = eval.evaluate_board(board);
    EXPECT_LT(score, 0.0f);  // Weiß im Vorteil = negativ
}

TEST(EvaluatorTest, EvaluateBoardWinBlack)
{
    Board board;
    Evaluator eval;
    
    // Schwarz gewinnt
    board.make_move(0, true);
    board.make_move(1, true);
    board.make_move(2, true);
    board.make_move(3, true);
    board.make_move(4, true);
    
    float score = eval.evaluate_board(board);
    EXPECT_EQ(score, 100000.0f);
}

TEST(EvaluatorTest, EvaluateBoardWinWhite)
{
    Board board;
    Evaluator eval;
    
    // Weiß gewinnt
    board.make_move(0, false);
    board.make_move(1, false);
    board.make_move(2, false);
    board.make_move(3, false);
    board.make_move(4, false);
    
    float score = eval.evaluate_board(board);
    EXPECT_EQ(score, -100000.0f);
}

TEST(EvaluatorTest, GetValidMovesEmpty)
{
    Board board;
    Evaluator eval;
    
    auto moves = eval.get_valid_moves(board);
    
    EXPECT_EQ(moves.size(), 1);
    EXPECT_EQ(moves[0], 7 * 15 + 7);  // Nur Mitte bei leerem Board
}

TEST(EvaluatorTest, GetValidMovesNearStones)
{
    Board board;
    Evaluator eval;
    
    board.make_move(7 * 15 + 7, true);  // Mitte
    
    auto moves = eval.get_valid_moves(board);
    
    // Sollte mehrere Kandidaten im Radius 2 finden
    EXPECT_GT(moves.size(), 1);
    
    // Mitte sollte NICHT in den Moves sein (belegt)
    bool center_in_moves = false;
    for (int m : moves) {
        if (m == 7 * 15 + 7) center_in_moves = true;
    }
    EXPECT_FALSE(center_in_moves);
}

TEST(EvaluatorTest, GetValidMovesNoFarPositions)
{
    Board board;
    Evaluator eval;
    
    board.make_move(0, true);  // A1 - Ecke
    
    auto moves = eval.get_valid_moves(board);
    
    // Weit entfernte Positionen sollten nicht enthalten sein
    int far_pos = 14 * 15 + 14;  // O15 - gegenüberliegende Ecke
    bool far_in_moves = false;
    for (int m : moves) {
        if (m == far_pos) far_in_moves = true;
    }
    EXPECT_FALSE(far_in_moves);
}