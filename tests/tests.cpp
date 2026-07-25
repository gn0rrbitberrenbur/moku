#include "../include/board.hpp"
#include "../include/utils.hpp"
#include "../include/minimax/evaluation.hpp"
#include "../include/minimax/transposition_table.hpp"
#include "../include/config.hpp"
#include "../include/engine.hpp"
#include <gtest/gtest.h>

// Test cases for the algebraic_to_index function
TEST(AlgebraicConversion, ValidInputs) {
    EXPECT_EQ(algebraic_to_index("A1", g_config.board_size), 0);
    EXPECT_EQ(algebraic_to_index("A2", g_config.board_size), 15);
    EXPECT_EQ(algebraic_to_index("B1", g_config.board_size), 1);
    EXPECT_EQ(algebraic_to_index("b1", g_config.board_size), 1);
    EXPECT_EQ(algebraic_to_index("B2", g_config.board_size), 16);
    EXPECT_EQ(algebraic_to_index("a3", g_config.board_size), 30);
    EXPECT_EQ(algebraic_to_index("A4", g_config.board_size), 45);
    EXPECT_EQ(algebraic_to_index("O15", g_config.board_size), 224);
}

TEST(AlgebraicConversion, InvalidInputs) {
    EXPECT_EQ(algebraic_to_index("", g_config.board_size), -1);
    EXPECT_EQ(algebraic_to_index("Z1", g_config.board_size), -1);
    EXPECT_EQ(algebraic_to_index("A0", g_config.board_size), -1);
    EXPECT_EQ(algebraic_to_index("A16", g_config.board_size), -1);
}

// Test cases for the Board class
TEST(BoardTest, InvalidMakeMove) {
    Board board;
    board.make_move(g_config.squares(), false);
}

TEST(BoardTest, ValidMakeMove) {
    Board board;
    board.make_move(0, true); // Black at A1
    board.make_move(15, false); // White at A2

    EXPECT_TRUE(board.black[0]);
    EXPECT_FALSE(board.white[0]);
    EXPECT_FALSE(board.black[15]);
    EXPECT_TRUE(board.white[15]);
}

TEST(BoardTest, ValidUndoMove)
{
    Board board;
    board.make_move(0, true); // Black at A1
    board.make_move(15, false); // White at A2
    board.undo_move(0);
    board.undo_move(15);
    EXPECT_FALSE(board.test_pos(0));
    EXPECT_FALSE(board.test_pos(15));
}

TEST(BoardTest, ValidTestPos) {
    Board board;
    board.make_move(0, true); // Black at A1
    board.make_move(15, false); // White at A2
    EXPECT_TRUE(board.test_pos(0));
    EXPECT_TRUE(board.test_pos(15));
}

TEST(BoardTest, InvalidTestPos) {
    Board board;
    board.make_move(0, true); // Black at A1
    board.make_move(15, false); // White at A2
    EXPECT_TRUE(board.test_pos(0));
    EXPECT_TRUE(board.test_pos(15));
}

TEST(BoardTest, ValidCheckWinBlack) {
    Board board;
    board.make_move(0, true); // A1
    board.make_move(1, true); // B1
    board.make_move(2, true); // C1
    board.make_move(3, true); // D1
    board.make_move(4, true); // E1

    EXPECT_TRUE(board.check_win());
}

TEST(BoardTest, InvalidCheckWinBlack) {
    Board board;
    board.make_move(0, true); // A1
    board.make_move(1, true); // B1
    board.make_move(2, true); // C1
    board.make_move(3, true); // D1

    EXPECT_FALSE(board.check_win());
}

TEST(BoardTest, ValidCheckWinWhite) {
    Board board;
    board.make_move(0, false); // A1
    board.make_move(1, false); // B1
    board.make_move(2, false); // C1
    board.make_move(3, false); // D1
    board.make_move(4, false); // E1

    EXPECT_TRUE(board.check_win());
}

TEST(BoardTest, InvalidCheckWinWhite) {
    Board board;
    board.make_move(0, false); // A1
    board.make_move(1, false); // B1
    board.make_move(2, false); // C1
    board.make_move(3, false); // D1

    EXPECT_FALSE(board.check_win());
}

TEST(BoardTest, ValidCheckWinBlackVertical) {
    Board board;
    board.make_move(0, true); // A1
    board.make_move(15, true); // A2
    board.make_move(30, true); // A3
    board.make_move(45, true); // A4
    board.make_move(60, true); // A5

    EXPECT_TRUE(board.check_win());
}

TEST(BoardTest, InvalidCheckWinBlackVertical) {
    Board board;
    board.make_move(0, true); // A1
    board.make_move(15, true); // A2
    board.make_move(30, true); // A3
    board.make_move(45, true); // A4

    EXPECT_FALSE(board.check_win());
}

TEST(BoardTest, ValidCheckWinWhiteDiagonal) {
    Board board;
    board.make_move(0, false); // A1
    board.make_move(16, false); // B2
    board.make_move(32, false); // C3
    board.make_move(48, false); // D4
    board.make_move(64, false); // E5

    EXPECT_TRUE(board.check_win());
}

TEST(BoardTest, InvalidCheckWinWhiteDiagonal) {
    Board board;
    board.make_move(0, false); // A1
    board.make_move(16, false); // B2
    board.make_move(32, false); // C3
    board.make_move(48, false); // D4

    EXPECT_FALSE(board.check_win());
}

TEST(BoardTest, CheckWinsAt) {
    Board board;
    board.make_move(0, true); // A1
    board.make_move(1, true); // B1
    board.make_move(2, true); // C1
    board.make_move(3, true); // D1
    board.make_move(4, true); // E1

    EXPECT_TRUE(board.wins_at(4, true));  // Black wins at E1
    EXPECT_FALSE(board.wins_at(4, false)); // White does not win at E1
}

// Test cases for the Evaluator class
TEST(EvaluatorTest, ScorePatternFiveInRow)
{
    Evaluator eval;
    EXPECT_GE(eval.score_pattern(5, 2), 100000);  // win
    EXPECT_GE(eval.score_pattern(5, 1), 100000);
    EXPECT_GE(eval.score_pattern(5, 0), 100000);
}

TEST(EvaluatorTest, ScorePatternFourInRow)
{
    Evaluator eval;
    EXPECT_EQ(eval.score_pattern(4, 2), 50000);   // Open-4
    EXPECT_EQ(eval.score_pattern(4, 1), 5000);    // Half-open 4
    EXPECT_EQ(eval.score_pattern(4, 0), 500);     // Blocked 4
}

TEST(EvaluatorTest, ScorePatternThreeInRow)
{
    Evaluator eval;
    EXPECT_EQ(eval.score_pattern(3, 2), 2000);    // Open-3
    EXPECT_EQ(eval.score_pattern(3, 1), 200);     // Half-open 3
    EXPECT_EQ(eval.score_pattern(3, 0), 20);      // Blocked 3
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
    int center_pos = 7 * 15 + 7;  // Middle of board (H8)
    int corner_pos = 0;           // Corner (A1)
    
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
    
    board.make_move(4 * 15 + 1, true);  // B5
    board.make_move(4 * 15 + 2, true);  // C5
    board.make_move(4 * 15 + 3, true);  // D5
    
    auto info = eval.analyze_line(board, 4 * 15 + 1, 1, 0, true);
    EXPECT_EQ(info.count, 3);
    EXPECT_EQ(info.open_ends, 2);
}

TEST(EvaluatorTest, AnalyzeLineVertical)
{
    Board board;
    Evaluator eval;
    
    board.make_move(2 * 15 + 4, true);  // E3
    board.make_move(3 * 15 + 4, true);  // E4
    board.make_move(4 * 15 + 4, true);  // E5
    
    auto info = eval.analyze_line(board, 2 * 15 + 4, 0, 1, true);
    EXPECT_EQ(info.count, 3);
    EXPECT_EQ(info.open_ends, 2);
}

TEST(EvaluatorTest, AnalyzeLineAtEdge)
{
    Board board;
    Evaluator eval;
    
    board.make_move(0, true);  // A1
    board.make_move(1, true);  // B1
    board.make_move(2, true);  // C1
    
    auto info = eval.analyze_line(board, 0, 1, 0, true);
    EXPECT_EQ(info.count, 3);
    EXPECT_EQ(info.open_ends, 1);
}

TEST(EvaluatorTest, EvaluateBoardEmpty)
{
    Board board;
    Evaluator eval;
    
    float score = eval.evaluate_board(board);
    EXPECT_EQ(score, 0.0f);  // Empty board should have neutral score
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
    EXPECT_GT(score, 0.0f);  // Black in advantage = positive score
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
    EXPECT_LT(score, 0.0f);  // White in advantage = negative score
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
    for (int m : moves) {
        if (m == 7 * 15 + 7) center_in_moves = true;
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
    for (int m : moves) {
        if (m == far_pos) far_in_moves = true;
    }
    EXPECT_FALSE(far_in_moves);
}

TEST(EvaluatorTest, AnalyzeLineDiagonalDownRight)
{
    Board board;
    Evaluator eval;
    int SIZE = g_config.board_size;

    board.make_move(2 * SIZE + 2, true);  // C3
    board.make_move(3 * SIZE + 3, true);  // D4
    board.make_move(4 * SIZE + 4, true);  // E5

    auto info = eval.analyze_line(board, 2 * SIZE + 2, 1, 1, true);
    EXPECT_EQ(info.count, 3);
    EXPECT_EQ(info.open_ends, 2);
}

TEST(EvaluatorTest, AnalyzeLineDiagonalUpRight)
{
    Board board;
    Evaluator eval;
    int SIZE = g_config.board_size;

    board.make_move(4 * SIZE + 2, true);  // C5
    board.make_move(3 * SIZE + 3, true);  // D4
    board.make_move(2 * SIZE + 4, true);  // E3

    auto info = eval.analyze_line(board, 4 * SIZE + 2, 1, -1, true);
    EXPECT_EQ(info.count, 3);
    EXPECT_EQ(info.open_ends, 2);
}

TEST(EvaluatorTest, ScoreWindowDiagonal)
{
    Evaluator eval;
    Board board;
    int SIZE = g_config.board_size;
    int base = 2 * SIZE + 2;  // C3

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
    int far_move = 0;  // A1 corner

    EXPECT_GT(eval.move_priority(board, win_move, true),
              eval.move_priority(board, far_move, true));
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

    EXPECT_GT(eval.move_priority(board, center, true),
              eval.move_priority(board, edge, true));
}

// Test cases for the is_line_dead function
TEST(EvaluatorTest, IsLineDeadBothEndsBlocked)
{
    Evaluator eval;
    Board board;
    // O X X X O  -> black run of 3 blocked on both ends
    int row = (g_config.board_size / 2);
    int base = row * g_config.board_size + 5;

    board.make_move(base - 1, false);   // O (white) before
    board.make_move(base, true);        // X
    board.make_move(base + 1, true);    // X
    board.make_move(base + 2, true);    // X
    board.make_move(base + 3, false);   // O (white) after

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

    board.make_move(base - 1, false);   // O before
    board.make_move(base, true);        // X
    board.make_move(base + 1, true);    // X
    board.make_move(base + 2, true);    // X
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

    board.make_move(base, true);        // X
    board.make_move(base + 1, true);    // X
    board.make_move(base + 2, true);    // X

    EXPECT_FALSE(eval.is_line_dead(board, base, 1, 0, 3, true));
}

TEST(EvaluatorTest, IsLineDeadBlockedByEdge)
{
    Evaluator eval;
    Board board;
    // run starts at column 0, so the front end is the board edge (blocked),
    // and we block the back end with white -> dead
    int row = (g_config.board_size / 2);
    int base = row * g_config.board_size + 0;  // column 0

    board.make_move(base, true);        // X at edge
    board.make_move(base + 1, true);    // X
    board.make_move(base + 2, true);    // X
    board.make_move(base + 3, false);   // O after

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
    board.make_move(base + 2, false);  // opponent

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
    for (int i = 0; i < 5; i++) {
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

class TestEngine : public Engine {
public:
    Board& get_board() { return board; }
    int get_timeout_turn() const { return timeout_turn; }
    int get_timeout_match() const { return timeout_match; }
    int get_time_left() const { return time_left; }
    long long get_max_memory() const { return max_memory; }
    int get_game_type() const { return game_type; }
    int get_rule() const { return rule; }
    int get_search_depth() const { return search_depth; }

    bool is_running() const { return running; }

    std::string execute_command(const std::string& command)
    {
        std::ostringstream capture;
        std::streambuf* old_cout_buf = std::cout.rdbuf(capture.rdbuf());

        process_command(command);

        std::cout.rdbuf(old_cout_buf);
        return capture.str();
    }
};

// Test cases for the Engine class
TEST(EngineTest, TestStartCommand)
{
    TestEngine engine;
    std::string output = engine.execute_command("START 15");
    
    EXPECT_EQ(output, "OK\n");
    EXPECT_EQ(g_config.board_size, 15);
}

TEST(EngineTest, StartCommandSmallBoard) {
    TestEngine engine;
    std::string output = engine.execute_command("START 9");
    
    EXPECT_EQ(output, "OK\n");
    EXPECT_EQ(g_config.board_size, 9);
}

TEST(EngineTest, StartCommandInvalidSize) {
    TestEngine engine;
    std::string output = engine.execute_command("START 3");
    
    EXPECT_TRUE(output.find("ERROR") != std::string::npos);
}

TEST(EngineTest, StartCommandTooLarge) {
    TestEngine engine;
    std::string output = engine.execute_command("START 25");
    
    EXPECT_TRUE(output.find("ERROR") != std::string::npos);
}

TEST(EngineTest, BeginCommandFirstMove) {
    TestEngine engine;
    engine.execute_command("START 15");
    std::string output = engine.execute_command("BEGIN");
    
    EXPECT_EQ(output, "7,7\n");
    EXPECT_TRUE(engine.get_board().test_pos(7 * 15 + 7));
}

TEST(EngineTest, TurnCommandValid) {
    TestEngine engine;
    engine.execute_command("START 15");
    std::string output = engine.execute_command("TURN 0,0");
    
    EXPECT_TRUE(engine.get_board().test_pos(0));  // A1 = pos 0
}

TEST(EngineTest, TurnCommandOutOfBounds) {
    TestEngine engine;
    engine.execute_command("START 10");
    std::string output = engine.execute_command("TURN 11,11");
    
    EXPECT_TRUE(output.find("ERROR") != std::string::npos);
}

TEST(EngineTest, TurnCommandAlreadyOccupied) {
    TestEngine engine;
    engine.execute_command("START 15");
    engine.execute_command("BEGIN");
    std::string output = engine.execute_command("TURN 7,7");
    
    EXPECT_TRUE(output.find("ERROR") != std::string::npos);
}

TEST(EngineTest, BoardCommandEmptyBoard) {
    TestEngine engine;
    engine.execute_command("START 15");

    std::istringstream input("DONE\n");
    std::streambuf* old_cin = std::cin.rdbuf(input.rdbuf());
    
    std::string output = engine.execute_command("BOARD");
    
    std::cin.rdbuf(old_cin);
    
    // Empty board should still return a valid move (7,7)
    EXPECT_TRUE(output.find("7,7") != std::string::npos || 
                output.find(",") != std::string::npos);
}

TEST(EngineTest, BoardCommandSingleOwnStone) {
    TestEngine engine;
    engine.execute_command("START 15");

    std::istringstream input("7,7,1\nDONE\n");
    std::streambuf* old_cin = std::cin.rdbuf(input.rdbuf());
    
    std::string output = engine.execute_command("BOARD");
    
    std::cin.rdbuf(old_cin);

    EXPECT_TRUE(output.find(",") != std::string::npos);
    EXPECT_TRUE(output.find("ERROR") == std::string::npos);

    EXPECT_TRUE(engine.get_board().test_pos(7 * 15 + 7));
}

TEST(EngineTest, BoardCommandSingleOpponentStone) {
    TestEngine engine;
    engine.execute_command("START 15");

    std::istringstream input("7,7,2\nDONE\n");
    std::streambuf* old_cin = std::cin.rdbuf(input.rdbuf());
    
    std::string output = engine.execute_command("BOARD");
    
    std::cin.rdbuf(old_cin);
    
    EXPECT_TRUE(output.find(",") != std::string::npos);
    EXPECT_TRUE(output.find("ERROR") == std::string::npos);
}

TEST(EngineTest, BoardCommandMultipleStones) {
    TestEngine engine;
    engine.execute_command("START 15");

    std::istringstream input("7,7,1\n8,7,1\n6,7,2\nDONE\n");
    std::streambuf* old_cin = std::cin.rdbuf(input.rdbuf());
    
    std::string output = engine.execute_command("BOARD");
    
    std::cin.rdbuf(old_cin);
    
    EXPECT_TRUE(output.find(",") != std::string::npos);

    EXPECT_TRUE(engine.get_board().test_pos(7 * 15 + 7));  // 7,7
    EXPECT_TRUE(engine.get_board().test_pos(7 * 15 + 8));  // 8,7
    EXPECT_TRUE(engine.get_board().test_pos(7 * 15 + 6));  // 6,7
}

TEST(EngineTest, BoardCommandDeterminesColorCorrectly) {
    TestEngine engine;
    engine.execute_command("START 15");

    std::istringstream input("7,7,1\n8,8,2\nDONE\n");
    std::streambuf* old_cin = std::cin.rdbuf(input.rdbuf());
    
    engine.execute_command("BOARD");
    
    std::cin.rdbuf(old_cin);

    EXPECT_TRUE(engine.get_board().black.test(7 * 15 + 7));  // own -> black
    EXPECT_TRUE(engine.get_board().white.test(8 * 15 + 8));  // opp -> white
}

TEST(EngineTest, BoardCommandWeAreWhite) {
    TestEngine engine;
    engine.execute_command("START 15");

    std::istringstream input("7,7,2\n8,8,2\n5,5,1\nDONE\n");
    std::streambuf* old_cin = std::cin.rdbuf(input.rdbuf());
    
    engine.execute_command("BOARD");
    
    std::cin.rdbuf(old_cin);
    
    EXPECT_TRUE(engine.get_board().white.test(5 * 15 + 5));
}

TEST(EngineTest, BoardCommandInvalidCoordinatesIgnored) {
    TestEngine engine;
    engine.execute_command("START 15");

    std::istringstream input("100,100,1\n7,7,1\nDONE\n");
    std::streambuf* old_cin = std::cin.rdbuf(input.rdbuf());
    
    std::string output = engine.execute_command("BOARD");
    
    std::cin.rdbuf(old_cin);

    EXPECT_TRUE(engine.get_board().test_pos(7 * 15 + 7));
    EXPECT_EQ(engine.get_board().black.count() + engine.get_board().white.count(), 2);
}

TEST(EngineTest, BoardCommandMalformedLineIgnored) {
    TestEngine engine;
    engine.execute_command("START 15");

    std::istringstream input("invalid_line\n7,7,1\nDONE\n");
    std::streambuf* old_cin = std::cin.rdbuf(input.rdbuf());
    
    std::string output = engine.execute_command("BOARD");
    
    std::cin.rdbuf(old_cin);
    
    EXPECT_TRUE(output.find("ERROR") == std::string::npos);
}

TEST(EngineTest, BoardCommandClearsExistingBoard) {
    TestEngine engine;
    engine.execute_command("START 15");
    
    engine.get_board().make_move(0, true);
    EXPECT_TRUE(engine.get_board().test_pos(0));
    
    std::istringstream input("7,7,1\nDONE\n");
    std::streambuf* old_cin = std::cin.rdbuf(input.rdbuf());
    
    engine.execute_command("BOARD");
    
    std::cin.rdbuf(old_cin);
    
    EXPECT_FALSE(engine.get_board().test_pos(0));
}

TEST(EngineTest, BoardCommandField3Ignored) {
    TestEngine engine;
    engine.execute_command("START 15");

    std::istringstream input("7,7,3\n7,8,1\n8,7,2\nDONE\n");
    std::streambuf* old_cin = std::cin.rdbuf(input.rdbuf());
    
    engine.execute_command("BOARD");
    
    std::cin.rdbuf(old_cin);

    EXPECT_TRUE(engine.get_board().test_pos(8 * 15 + 7));
    EXPECT_TRUE(engine.get_board().test_pos(7 * 15 + 8));
    
    size_t total_stones = engine.get_board().black.count() + engine.get_board().white.count();
    EXPECT_EQ(total_stones, 3);
}

TEST(EngineTest, InfoTimeoutTurn) {
    TestEngine engine;
    engine.execute_command("START 15");
    engine.execute_command("INFO timeout_turn 5000");
    
    EXPECT_EQ(engine.get_timeout_turn(), 5000);
}

TEST(EngineTest, InfoTimeoutMatch) {
    TestEngine engine;
    engine.execute_command("START 15");
    engine.execute_command("INFO timeout_match 60000");
    
    EXPECT_EQ(engine.get_timeout_match(), 60000);
}

TEST(EngineTest, InfoTimeLeft) {
    TestEngine engine;
    engine.execute_command("START 15");
    engine.execute_command("INFO time_left 120000");
    
    EXPECT_EQ(engine.get_time_left(), 120000);
}

TEST(EngineTest, InfoMaxMemory) {
    TestEngine engine;
    engine.execute_command("START 15");
    engine.execute_command("INFO max_memory 1024");
    
    EXPECT_EQ(engine.get_max_memory(), 1024);
}

TEST(EngineTest, InfoGameType) {
    TestEngine engine;
    engine.execute_command("START 15");
    engine.execute_command("INFO game_type 1");
    
    EXPECT_EQ(engine.get_game_type(), 1);
}

TEST(EngineTest, InfoRule) {
    TestEngine engine;
    engine.execute_command("START 15");
    engine.execute_command("INFO rule 2");
    
    EXPECT_EQ(engine.get_rule(), 2);
}

TEST(EngineTest, InfoDepth) {
    TestEngine engine;
    engine.execute_command("START 15");
    engine.execute_command("INFO depth 8");
    
    EXPECT_EQ(engine.get_search_depth(), 8);
}

TEST(EngineTest, InfoInvalidDepth) {
    TestEngine engine;
    engine.execute_command("START 15");
    std::string output = engine.execute_command("INFO depth -1");
    
    EXPECT_EQ(engine.get_search_depth(), 6);
}

TEST(EngineTest, InfoIsRunning) {
    TestEngine engine;
    engine.execute_command("START 15");
    
    EXPECT_TRUE(engine.is_running());
}

TEST(EngineTest, InfoUnknownKey) {
    TestEngine engine;
    engine.execute_command("START 15");
    std::string output = engine.execute_command("INFO unknown_key 123");
    
    EXPECT_EQ(output, "");
}

TEST(EngineTest, EndStopsEngine) {
    TestEngine engine;
    engine.execute_command("START 15");
    
    EXPECT_TRUE(engine.is_running());
    
    engine.execute_command("END");
    
    EXPECT_FALSE(engine.is_running());
}

TEST(EngineTest, RestartClearsBoard) {
    TestEngine engine;
    engine.execute_command("START 15");
    engine.get_board().make_move(112, true);
    
    EXPECT_EQ(engine.get_board().black.count(), 1);
    
    std::string output = engine.execute_command("RESTART");
    
    EXPECT_EQ(output, "OK\n");
    EXPECT_EQ(engine.get_board().black.count(), 0);
    EXPECT_EQ(engine.get_board().white.count(), 0);
}

TEST(EngineTest, TakebackValid) {
    TestEngine engine;
    engine.execute_command("START 15");
    engine.get_board().make_move(112, true);  // E8
    engine.get_board().make_move(113, false); // F8
    
    std::string output = engine.execute_command("TAKEBACK 7,7");
    
    EXPECT_EQ(output, "OK\n");
    EXPECT_FALSE(engine.get_board().test_pos(7 * 15 + 7));
}

TEST(EngineTest, TakebackInvalidCoordinates) {
    TestEngine engine;
    engine.execute_command("START 15");
    engine.get_board().make_move(112, true);  // E8
    
    std::string output = engine.execute_command("TAKEBACK 15,15");
    
    EXPECT_TRUE(output.find("ERROR") != std::string::npos);
    EXPECT_TRUE(engine.get_board().test_pos(112));
}

// Test cases for the transposition table
TEST(TranspositionTableTest, SizeIsPowerOfTwo) {
    TranspositionTable tt;
    EXPECT_EQ(tt.size(), size_t(1) << 23);
}

TEST(TranspositionTableTest, HashEmptyBoardDeterministic) {
    g_config.board_size = 15;
    TranspositionTable tt;
    Board board;
    uint64_t h1 = tt.compute_hash(board, true);
    uint64_t h2 = tt.compute_hash(board, true);
    EXPECT_EQ(h1, h2);
}

TEST(TranspositionTableTest, HashDiffersByTurn) {
    g_config.board_size = 15;
    TranspositionTable tt;
    Board board;
    EXPECT_NE(tt.compute_hash(board, true), tt.compute_hash(board, false));
}

TEST(TranspositionTableTest, HashDiffersByStone) {
    g_config.board_size = 15;
    TranspositionTable tt;
    Board board;
    uint64_t empty = tt.compute_hash(board, true);
    board.make_move(0, true);
    EXPECT_NE(empty, tt.compute_hash(board, true));
}

TEST(TranspositionTableTest, HashDiffersByColorOnSameSquare) {
    g_config.board_size = 15;
    TranspositionTable tt;
    Board black_board;
    black_board.make_move(0, true);
    Board white_board;
    white_board.make_move(0, false);
    EXPECT_NE(tt.compute_hash(black_board, true),
              tt.compute_hash(white_board, true));
}

TEST(TranspositionTableTest, IncrementalUpdateMatchesFullHash) {
    g_config.board_size = 15;
    TranspositionTable tt;
    Board board;

    uint64_t incremental = tt.compute_hash(board, true);
    incremental = tt.update_hash(incremental, 0, true);   // black at 0, turn -> white
    board.make_move(0, true);

    uint64_t full = tt.compute_hash(board, false);
    EXPECT_EQ(incremental, full);
}

TEST(TranspositionTableTest, IncrementalUpdateMultipleMoves) {
    g_config.board_size = 15;
    TranspositionTable tt;
    Board board;

    uint64_t inc = tt.compute_hash(board, true);
    inc = tt.update_hash(inc, 5, true);    // black
    inc = tt.update_hash(inc, 6, false);   // white
    board.make_move(5, true);
    board.make_move(6, false);

    EXPECT_EQ(inc, tt.compute_hash(board, true));
}

TEST(TranspositionTableTest, StoreAndProbe) {
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

TEST(TranspositionTableTest, ProbeMissReturnsFalse) {
    TranspositionTable tt;
    TTEntry entry;
    EXPECT_FALSE(tt.probe(0xDEADBEEFULL, entry, 0));
}

TEST(TranspositionTableTest, DeeperDoesNotOverwriteSameHash) {
    TranspositionTable tt;
    uint64_t hash = 0x1111ULL;
    tt.store(hash, 10.0f, 8, TTEntry::EXACT, 1, 0);
    tt.store(hash, 20.0f, 3, TTEntry::EXACT, 2, 0);  // shallower, same key

    TTEntry entry;
    ASSERT_TRUE(tt.probe(hash, entry, 0));
    EXPECT_EQ(entry.depth, 8);
    EXPECT_FLOAT_EQ(entry.score, 10.0f);
    EXPECT_EQ(entry.best_move, 1);
}

TEST(TranspositionTableTest, DeeperOverwritesSameHash) {
    TranspositionTable tt;
    uint64_t hash = 0x2222ULL;
    tt.store(hash, 10.0f, 3, TTEntry::EXACT, 1, 0);
    tt.store(hash, 20.0f, 8, TTEntry::EXACT, 2, 0);  // deeper, same key

    TTEntry entry;
    ASSERT_TRUE(tt.probe(hash, entry, 0));
    EXPECT_EQ(entry.depth, 8);
    EXPECT_FLOAT_EQ(entry.score, 20.0f);
    EXPECT_EQ(entry.best_move, 2);
}

TEST(TranspositionTableTest, MateScoreAdjustedByPly) {
    TranspositionTable tt;
    uint64_t hash = 0x3333ULL;
    // store a mate score found at ply 4
    tt.store(hash, 95000.0f, 5, TTEntry::EXACT, 0, 4);

    TTEntry entry;
    ASSERT_TRUE(tt.probe(hash, entry, 4));
    // store adds ply, probe subtracts ply -> round trip
    EXPECT_FLOAT_EQ(entry.score, 95000.0f);
}

TEST(TranspositionTableTest, NegativeMateScoreAdjustedByPly) {
    TranspositionTable tt;
    uint64_t hash = 0x4444ULL;
    tt.store(hash, -95000.0f, 5, TTEntry::EXACT, 0, 3);

    TTEntry entry;
    ASSERT_TRUE(tt.probe(hash, entry, 3));
    EXPECT_FLOAT_EQ(entry.score, -95000.0f);
}

TEST(TranspositionTableTest, ClearRemovesEntries) {
    TranspositionTable tt;
    uint64_t hash = 0x5555ULL;
    tt.store(hash, 1.0f, 1, TTEntry::EXACT, 0, 0);

    TTEntry entry;
    ASSERT_TRUE(tt.probe(hash, entry, 0));

    tt.clear();
    EXPECT_FALSE(tt.probe(hash, entry, 0));
}