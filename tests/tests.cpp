#include "../include/board.hpp"
#include "../include/utils.hpp"
#include "../include/minimax/evaluation.hpp"
#include "../include/config.hpp"
#include "../include/engine.hpp"
#include <gtest/gtest.h>

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

// Evaluator Tests

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
    // Angepasst an neue Werte
    EXPECT_EQ(eval.score_pattern(4, 2), 50000);   // Open-4
    EXPECT_EQ(eval.score_pattern(4, 1), 5000);    // Half-open 4
    EXPECT_EQ(eval.score_pattern(4, 0), 500);     // Blocked 4
}

TEST(EvaluatorTest, ScorePatternThreeInRow)
{
    Evaluator eval;
    // Angepasst an neue Werte
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
    
    // 3 schwarze Steine horizontal: B5, C5, D5
    board.make_move(4 * 15 + 1, true);  // B5
    board.make_move(4 * 15 + 2, true);  // C5
    board.make_move(4 * 15 + 3, true);  // D5
    
    // analyze_line zählt ab Startpunkt NUR VORWÄRTS
    // Um alle 3 zu zählen, muss man beim ERSTEN Stein (B5) starten
    auto info = eval.analyze_line(board, 4 * 15 + 1, 1, 0, true);  // Start bei B5
    EXPECT_EQ(info.count, 3);
    EXPECT_EQ(info.open_ends, 2);  // A5 und E5 sind frei
}

TEST(EvaluatorTest, AnalyzeLineVertical)
{
    Board board;
    Evaluator eval;
    
    // 3 schwarze Steine vertikal: E3, E4, E5
    board.make_move(2 * 15 + 4, true);  // E3
    board.make_move(3 * 15 + 4, true);  // E4
    board.make_move(4 * 15 + 4, true);  // E5
    
    // Start beim ERSTEN Stein (E3) und zähle nach unten (dy=1)
    auto info = eval.analyze_line(board, 2 * 15 + 4, 0, 1, true);  // Start bei E3
    EXPECT_EQ(info.count, 3);
    EXPECT_EQ(info.open_ends, 2);  // E2 und E6 sind frei
}

TEST(EvaluatorTest, AnalyzeLineAtEdge)
{
    Board board;
    Evaluator eval;
    
    // 3 schwarze Steine am Rand: A1, B1, C1
    board.make_move(0, true);  // A1
    board.make_move(1, true);  // B1
    board.make_move(2, true);  // C1
    
    // Start beim ERSTEN Stein (A1) und zähle nach rechts
    auto info = eval.analyze_line(board, 0, 1, 0, true);  // Start bei A1
    EXPECT_EQ(info.count, 3);
    EXPECT_EQ(info.open_ends, 1);  // Nur D1 ist frei, links ist Rand
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

        std::cout.rdbuf(old_cout_buf);  // Restore original buffer
        return capture.str();
    }
};

// test START command with various sizes
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

// test BEGIN command
TEST(EngineTest, BeginCommandFirstMove) {
    TestEngine engine;
    engine.execute_command("START 15");
    std::string output = engine.execute_command("BEGIN");
    
    EXPECT_EQ(output, "7,7\n");
    EXPECT_TRUE(engine.get_board().test_pos(7 * 15 + 7));
}

// test TURN command with valid and invalid input
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

// test BOARD command
TEST(EngineTest, BoardCommandEmptyBoard) {
    TestEngine engine;
    engine.execute_command("START 15");
    
    // Simuliere BOARD-Befehl mit leerem Board
    std::istringstream input("DONE\n");
    std::streambuf* old_cin = std::cin.rdbuf(input.rdbuf());
    
    std::string output = engine.execute_command("BOARD");
    
    std::cin.rdbuf(old_cin);
    
    // Bei leerem Board sollte Mitte gespielt werden
    EXPECT_TRUE(output.find("7,7") != std::string::npos || 
                output.find(",") != std::string::npos);
}

TEST(EngineTest, BoardCommandSingleOwnStone) {
    TestEngine engine;
    engine.execute_command("START 15");
    
    // field=1 bedeutet eigener Stein
    std::istringstream input("7,7,1\nDONE\n");
    std::streambuf* old_cin = std::cin.rdbuf(input.rdbuf());
    
    std::string output = engine.execute_command("BOARD");
    
    std::cin.rdbuf(old_cin);
    
    // Sollte einen gültigen Zug ausgeben
    EXPECT_TRUE(output.find(",") != std::string::npos);
    EXPECT_TRUE(output.find("ERROR") == std::string::npos);
    
    // Position 7,7 sollte belegt sein
    EXPECT_TRUE(engine.get_board().test_pos(7 * 15 + 7));
}

TEST(EngineTest, BoardCommandSingleOpponentStone) {
    TestEngine engine;
    engine.execute_command("START 15");
    
    // field=2 bedeutet gegnerischer Stein
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
    
    // Mehrere Steine: 2 eigene, 1 gegnerischer
    std::istringstream input("7,7,1\n8,7,1\n6,7,2\nDONE\n");
    std::streambuf* old_cin = std::cin.rdbuf(input.rdbuf());
    
    std::string output = engine.execute_command("BOARD");
    
    std::cin.rdbuf(old_cin);
    
    EXPECT_TRUE(output.find(",") != std::string::npos);
    
    // Alle drei Positionen sollten belegt sein
    EXPECT_TRUE(engine.get_board().test_pos(7 * 15 + 7));  // 7,7
    EXPECT_TRUE(engine.get_board().test_pos(7 * 15 + 8));  // 8,7
    EXPECT_TRUE(engine.get_board().test_pos(7 * 15 + 6));  // 6,7
}

TEST(EngineTest, BoardCommandDeterminesColorCorrectly) {
    TestEngine engine;
    engine.execute_command("START 15");
    
    // Gleiche Anzahl eigene/gegnerische -> wir sind Schwarz
    std::istringstream input("7,7,1\n8,8,2\nDONE\n");
    std::streambuf* old_cin = std::cin.rdbuf(input.rdbuf());
    
    engine.execute_command("BOARD");
    
    std::cin.rdbuf(old_cin);
    
    // Bei gleicher Anzahl: own=black, opp=white
    EXPECT_TRUE(engine.get_board().black.test(7 * 15 + 7));  // own -> black
    EXPECT_TRUE(engine.get_board().white.test(8 * 15 + 8));  // opp -> white
}

TEST(EngineTest, BoardCommandWeAreWhite) {
    TestEngine engine;
    engine.execute_command("START 15");
    
    // Mehr gegnerische Steine -> wir sind Weiß
    std::istringstream input("7,7,2\n8,8,2\n5,5,1\nDONE\n");
    std::streambuf* old_cin = std::cin.rdbuf(input.rdbuf());
    
    engine.execute_command("BOARD");
    
    std::cin.rdbuf(old_cin);
    
    // own_moves=1, opp_moves=2 -> we_are_black=false
    // Eigener Stein (field=1) sollte Weiß sein
    EXPECT_TRUE(engine.get_board().white.test(5 * 15 + 5));
}

TEST(EngineTest, BoardCommandInvalidCoordinatesIgnored) {
    TestEngine engine;
    engine.execute_command("START 15");
    
    // Ungültige Koordinaten sollten ignoriert werden
    std::istringstream input("100,100,1\n7,7,1\nDONE\n");
    std::streambuf* old_cin = std::cin.rdbuf(input.rdbuf());
    
    std::string output = engine.execute_command("BOARD");
    
    std::cin.rdbuf(old_cin);
    
    // Nur der gültige Stein sollte gesetzt sein
    EXPECT_TRUE(engine.get_board().test_pos(7 * 15 + 7));
    EXPECT_EQ(engine.get_board().black.count() + engine.get_board().white.count(), 2);  // 1 gelesen + 1 gespielt
}

TEST(EngineTest, BoardCommandMalformedLineIgnored) {
    TestEngine engine;
    engine.execute_command("START 15");
    
    // Fehlerhafte Zeile sollte ignoriert werden
    std::istringstream input("invalid_line\n7,7,1\nDONE\n");
    std::streambuf* old_cin = std::cin.rdbuf(input.rdbuf());
    
    std::string output = engine.execute_command("BOARD");
    
    std::cin.rdbuf(old_cin);
    
    EXPECT_TRUE(output.find("ERROR") == std::string::npos);
}

TEST(EngineTest, BoardCommandClearsExistingBoard) {
    TestEngine engine;
    engine.execute_command("START 15");
    
    // Erst einen Stein setzen
    engine.get_board().make_move(0, true);
    EXPECT_TRUE(engine.get_board().test_pos(0));
    
    // BOARD sollte das Board zurücksetzen
    std::istringstream input("7,7,1\nDONE\n");
    std::streambuf* old_cin = std::cin.rdbuf(input.rdbuf());
    
    engine.execute_command("BOARD");
    
    std::cin.rdbuf(old_cin);
    
    // Position 0 sollte jetzt leer sein (Board wurde zurückgesetzt)
    EXPECT_FALSE(engine.get_board().test_pos(0));
}

TEST(EngineTest, BoardCommandField3Ignored) {
    TestEngine engine;
    engine.execute_command("START 15");
    
    // field=3 ist laut Protokoll ungültig/reserviert
    // Setze 8,8 als eigenen Stein und 9,9 als gegnerischen
    // um zu vermeiden, dass die Engine auf 7,7 spielt
    std::istringstream input("7,7,3\n7,8,1\n8,7,2\nDONE\n");
    std::streambuf* old_cin = std::cin.rdbuf(input.rdbuf());
    
    engine.execute_command("BOARD");
    
    std::cin.rdbuf(old_cin);
    
    // Prüfe: field=1 und field=2 wurden verarbeitet
    EXPECT_TRUE(engine.get_board().test_pos(8 * 15 + 7));   // 7,8 -> field=1 gesetzt
    EXPECT_TRUE(engine.get_board().test_pos(7 * 15 + 8));   // 8,7 -> field=2 gesetzt
    
    // Die Engine macht einen zusätzlichen Zug, also 3 Steine total
    // (2 aus BOARD + 1 Antwortzug)
    size_t total_stones = engine.get_board().black.count() + engine.get_board().white.count();
    EXPECT_EQ(total_stones, 3);
}

// test INFO commands that set time limits and memory
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

// test END command
TEST(EngineTest, EndStopsEngine) {
    TestEngine engine;
    engine.execute_command("START 15");
    
    EXPECT_TRUE(engine.is_running());
    
    engine.execute_command("END");
    
    EXPECT_FALSE(engine.is_running());
}

// test RESTART command
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

// test TAKEBACK command
TEST(EngineTest, TakebackValid) {
    TestEngine engine;
    engine.execute_command("START 15");
    engine.get_board().make_move(112, true);  // E8
    engine.get_board().make_move(113, false); // F8
    
    std::string output = engine.execute_command("TAKEBACK 7,7");
    
    EXPECT_EQ(output, "OK\n");
    EXPECT_FALSE(engine.get_board().test_pos(7 * 15 + 7));  // E8 sollte zurückgenommen sein
}

TEST(EngineTest, TakebackInvalidCoordinates) {
    TestEngine engine;
    engine.execute_command("START 15");
    engine.get_board().make_move(112, true);  // E8
    
    std::string output = engine.execute_command("TAKEBACK 15,15");
    
    EXPECT_TRUE(output.find("ERROR") != std::string::npos);
    EXPECT_TRUE(engine.get_board().test_pos(112));  // E8 sollte weiterhin belegt sein
}

// ===== is_line_dead Tests =====

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

// ===== score_window Tests =====

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