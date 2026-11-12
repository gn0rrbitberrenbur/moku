#include "config.hpp"
#include "engine/gomocup_protocol/gomocup_engine.hpp"

#include <gtest/gtest.h>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>

class ScopedStreamBuffer
{
public:
    ScopedStreamBuffer(std::ios& stream, std::streambuf* replacement)
        : stream_(stream), original_(stream.rdbuf(replacement))
    {
    }

    ~ScopedStreamBuffer()
    {
        stream_.rdbuf(original_);
    }

private:
    std::ios& stream_;
    std::streambuf* original_;
};

/**
 * This test suite implements tests that check whether the engine is compliant
 * to the Gomoku AI protocol (see: https://plastovicka.github.io/protocl2en.htm)
 */

class ProtocolEngine : public Engine
{
public:
    std::string execute(const std::string& command, const std::string& input = "")
    {
        std::ostringstream output;
        std::istringstream input_stream(input);
        ScopedStreamBuffer output_redirect(std::cout, output.rdbuf());
        ScopedStreamBuffer input_redirect(std::cin,
                                          input.empty() ? std::cin.rdbuf() : input_stream.rdbuf());

        process_command(command);

        return output.str();
    }

    std::string execute_stream(const std::string& input)
    {
        std::ostringstream output;
        std::istringstream input_stream(input);
        ScopedStreamBuffer output_redirect(std::cout, output.rdbuf());
        ScopedStreamBuffer input_redirect(std::cin, input_stream.rdbuf());

        run();

        return output.str();
    }

    const Board& get_board() const
    {
        return board;
    }

    bool is_running() const
    {
        return running;
    }
};

class GomocupProtocolTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        g_config.board_size = 15;
        g_config.time_limit_ms = 20000;
        g_config.max_depth = 6;
        g_config.timeout_match = 0;
        g_config.time_left = 2147483647;
        g_config.max_memory = 0;
        g_config.game_type = 0;
        g_config.rule = 0;
        g_config.folder.clear();
    }

    static void expect_coordinate(const std::string& output, int board_size)
    {
        std::regex coordinate_pattern(R"(^[0-9]+,[0-9]+(\r\n|\n|\r)$)");
        ASSERT_TRUE(std::regex_match(output, coordinate_pattern));

        std::istringstream stream(output);
        int x;
        int y;
        char comma;

        ASSERT_TRUE(stream >> x >> comma >> y);
        EXPECT_EQ(comma, ',');
        EXPECT_GE(x, 0);
        EXPECT_LT(x, board_size);
        EXPECT_GE(y, 0);
        EXPECT_LT(y, board_size);
    }

    static bool is_ok_response(const std::string& output)
    {
        const std::regex ok_pattern(R"(^OK( [^\r\n]*)?(\r\n|\n|\r)$)");
        return std::regex_match(output, ok_pattern);
    }
};

/**
 * "START [size]"
 * Following this command, the brain should initialize an empty board but does not make a move yet.
 * A board of size 20 is required. If the size is good, the engine answers "OK".
 * If the size is unsupported, the engine answers "ERROR message".
 */
TEST_F(GomocupProtocolTest, START_StartAcceptsRequiredBoardSize20)
{
    g_config.max_depth = 1;
    ProtocolEngine engine;

    ASSERT_TRUE(is_ok_response(engine.execute("START 20")));
    EXPECT_EQ(g_config.board_size, 20);

    EXPECT_EQ(engine.get_board().black.count(), 0);
    EXPECT_EQ(engine.get_board().white.count(), 0);

    engine.execute("INFO timeout_turn 100");
    const std::string output = engine.execute("TURN 19,19");
    expect_coordinate(output, 20);
    EXPECT_TRUE(engine.get_board().test_pos(19 * 20 + 19));
}

/**
 * "BEGIN"
 * This command is sent by the manager to the engine at the beginning of the match, right
 * after "START". This means that the engine is expected to play the first move.
 * Expected answer: "[X],[Y]", coordinates of the brains move.
 */
TEST_F(GomocupProtocolTest, BEGIN_BeginReturnsMoveAndUpdatesBoard)
{
    ProtocolEngine engine;
    engine.execute("START 15");

    const std::string output = engine.execute("BEGIN");

    expect_coordinate(output, 15);
    EXPECT_EQ(engine.get_board().black.count() + engine.get_board().white.count(), 1);
}

/**
 * "TURN [X],[Y]"
 * This command is sent to the engine to make a move.
 * The parameters are the coordinates of the move, numbered from zero.
 */
TEST_F(GomocupProtocolTest, TURN_TurnReturnsMoveAndRegistersOpponentMove)
{
    ProtocolEngine engine;
    engine.execute("START 15");
    engine.execute("INFO timeout_turn 1");

    const std::string output = engine.execute("TURN 0,0");

    expect_coordinate(output, 15);
    EXPECT_TRUE(engine.get_board().test_pos(0));
    std::istringstream stream(output);
    int x;
    int y;
    char comma;
    ASSERT_TRUE(stream >> x >> comma >> y);
    EXPECT_NE(y * 15 + x, 0);
    EXPECT_EQ(engine.get_board().black.count() + engine.get_board().white.count(), 2);
}

TEST_F(GomocupProtocolTest, TURN_TurnStoresLegalReplyAndOpponentMoveWithoutOverlap)
{
    g_config.max_depth = 1;
    ProtocolEngine engine;
    ASSERT_TRUE(is_ok_response(engine.execute("START 15")));
    engine.execute("INFO timeout_turn 100");

    ASSERT_NO_FATAL_FAILURE(expect_coordinate(engine.execute("BEGIN"), 15));
    const Board before = engine.get_board();
    const int opponent_pos = 3 * 15 + 2;
    ASSERT_FALSE(before.test_pos(opponent_pos));

    const std::string output = engine.execute("TURN 2,3");
    ASSERT_NO_FATAL_FAILURE(expect_coordinate(output, 15));

    std::istringstream stream(output);
    int reply_x;
    int reply_y;
    char comma;
    ASSERT_TRUE(stream >> reply_x >> comma >> reply_y);
    const int reply_pos = reply_y * 15 + reply_x;

    EXPECT_FALSE(before.test_pos(reply_pos));
    EXPECT_NE(reply_pos, opponent_pos);
    EXPECT_TRUE(engine.get_board().test_pos(opponent_pos));
    EXPECT_TRUE(engine.get_board().test_pos(reply_pos));
    EXPECT_EQ(engine.get_board().black.count() + engine.get_board().white.count(), 3);
}

TEST_F(GomocupProtocolTest, TURN_TurnRejectsNegativeCoordinates)
{
    ProtocolEngine engine;
    engine.execute("START 15");

    const std::string output = engine.execute("TURN -1,0");

    EXPECT_NE(output.find("ERROR"), std::string::npos);
}

TEST_F(GomocupProtocolTest, TURN_TurnRejectsCoordinatesOutsideBoard)
{
    ProtocolEngine engine;
    engine.execute("START 15");

    const std::string output = engine.execute("TURN 15,0");

    EXPECT_NE(output.find("ERROR"), std::string::npos);
}

TEST_F(GomocupProtocolTest, TURN_TurnRejectsOccupiedPosition)
{
    ProtocolEngine engine;
    engine.execute("START 15");
    engine.execute("BEGIN");

    const std::string output = engine.execute("TURN 7,7");

    EXPECT_NE(output.find("ERROR"), std::string::npos);
}

/**
 * "BOARD"
 * This command is used to build a board.
 * Parameters: [X],[Y],[field]
 * field=1 means "own stone"
 * field=2 means "opponents stone"
 * field=3 means stone is part of winning line or forbidden (only if continuous game is enabled)
 * Data is ended with "DONE"
 */
TEST_F(GomocupProtocolTest, BOARD_BoardLoadsOwnAndOpponentStones)
{
    g_config.max_depth = 1;
    ProtocolEngine engine;
    engine.execute("START 15");
    engine.execute("INFO timeout_turn 100");

    const std::string output = engine.execute("BOARD", "7,7,1\n"
                                                       "8,7,2\n"
                                                       "6,7,1\n"
                                                       "DONE\n");

    expect_coordinate(output, 15);
    EXPECT_EQ(engine.get_board().black.count() + engine.get_board().white.count(), 4);
    EXPECT_TRUE(engine.get_board().test_pos(7 * 15 + 7));
    EXPECT_TRUE(engine.get_board().test_pos(7 * 15 + 8));
    EXPECT_TRUE(engine.get_board().test_pos(7 * 15 + 6));

    std::istringstream stream(output);
    int x;
    int y;
    char comma;
    ASSERT_TRUE(stream >> x >> comma >> y);
    const int reply_pos = y * 15 + x;
    EXPECT_NE(reply_pos, 7 * 15 + 7);
    EXPECT_NE(reply_pos, 7 * 15 + 8);
    EXPECT_NE(reply_pos, 7 * 15 + 6);
}

TEST_F(GomocupProtocolTest, BOARD_BoardPreservesPositionsAndAddsExactlyOneOwnStone)
{
    g_config.max_depth = 1;
    ProtocolEngine engine;
    ASSERT_TRUE(is_ok_response(engine.execute("START 15")));
    engine.execute("INFO timeout_turn 100");

    const std::string records = "5,6,2\n2,3,1\n8,9,2\n";
    const std::string output = engine.execute("BOARD", records + "DONE\n");
    ASSERT_NO_FATAL_FAILURE(expect_coordinate(output, 15));

    const int first = 6 * 15 + 5;
    const int second = 3 * 15 + 2;
    const int third = 9 * 15 + 8;
    EXPECT_TRUE(engine.get_board().test_pos(first));
    EXPECT_TRUE(engine.get_board().test_pos(second));
    EXPECT_TRUE(engine.get_board().test_pos(third));
    EXPECT_EQ(engine.get_board().black.count() + engine.get_board().white.count(), 4);

    std::istringstream stream(output);
    int x;
    int y;
    char comma;
    ASSERT_TRUE(stream >> x >> comma >> y);
    const int reply_pos = y * 15 + x;
    EXPECT_NE(reply_pos, first);
    EXPECT_NE(reply_pos, second);
    EXPECT_NE(reply_pos, third);

    const std::string shuffled_output = engine.execute("BOARD", "8,9,2\n"
                                                                "5,6,2\n"
                                                                "2,3,1\n"
                                                                "DONE\n");
    ASSERT_NO_FATAL_FAILURE(expect_coordinate(shuffled_output, 15));
    EXPECT_TRUE(engine.get_board().test_pos(first));
    EXPECT_TRUE(engine.get_board().test_pos(second));
    EXPECT_TRUE(engine.get_board().test_pos(third));
    EXPECT_EQ(engine.get_board().black.count() + engine.get_board().white.count(), 4);
    std::istringstream shuffled_stream(shuffled_output);
    int shuffled_x;
    int shuffled_y;
    char shuffled_comma;
    ASSERT_TRUE(shuffled_stream >> shuffled_x >> shuffled_comma >> shuffled_y);
    const int shuffled_reply = shuffled_y * 15 + shuffled_x;
    EXPECT_NE(shuffled_reply, first);
    EXPECT_NE(shuffled_reply, second);
    EXPECT_NE(shuffled_reply, third);
}

TEST_F(GomocupProtocolTest, BOARD_BoardResetsPreviousPosition)
{
    ProtocolEngine engine;
    engine.execute("START 15");
    engine.execute("INFO timeout_turn 100");
    engine.execute("BEGIN");

    EXPECT_EQ(engine.get_board().black.count() + engine.get_board().white.count(), 1);

    engine.execute("BOARD", "0,0,1\nDONE\n");

    EXPECT_TRUE(engine.get_board().test_pos(0));
    EXPECT_FALSE(engine.get_board().test_pos(112));
}

TEST_F(GomocupProtocolTest, BOARD_RenjuForbiddenFieldThreeIsNotPlayed)
{
    g_config.max_depth = 1;
    ProtocolEngine engine;
    ASSERT_TRUE(is_ok_response(engine.execute("START 15")));
    engine.execute("INFO timeout_turn 100");
    engine.execute("INFO rule 6");

    const std::string output = engine.execute("BOARD", "6,7,1\n"
                                                       "0,0,2\n"
                                                       "8,7,1\n"
                                                       "2,0,2\n"
                                                       "7,6,1\n"
                                                       "4,0,2\n"
                                                       "7,8,1\n"
                                                       "6,0,2\n"
                                                       "12,12,1\n"
                                                       "8,0,2\n"
                                                       "7,7,3\n"
                                                       "DONE\n");

    ASSERT_NO_FATAL_FAILURE(expect_coordinate(output, 15));
    std::istringstream stream(output);
    int x;
    int y;
    char comma;
    ASSERT_TRUE(stream >> x >> comma >> y);
    EXPECT_FALSE(x == 7 && y == 7);
}

/**
 * "INFO [key] [value]"
 * This command is used by the manager to send information to the brain.
 * This could be:
 * timeout_turn  - time limit for each move (milliseconds, 0=play as fast as possible)
 * timeout_match - time limit of a whole match (milliseconds, 0=no limit)
 * max_memory    - memory limit (bytes, 0=no limit)
 * time_left     - remaining time limit of a whole match (milliseconds)
 * game_type     - 0=opponent is human, 1=opponent is brain, 2=tournament, 3=network tournament
 * rule          - bitmask or sum of 1=exactly five in a row win, 2=continuous game, 4=renju, 8=caro
 * evaluate      - coordinates X,Y representing current position of the mouse cursor
 * folder        - folder for persistent files
 */
TEST_F(GomocupProtocolTest, INFO_InfoDoesNotWriteToStandardOutput)
{
    ProtocolEngine engine;

    EXPECT_TRUE(engine.execute("INFO timeout_turn 4000").empty());
}

TEST_F(GomocupProtocolTest, INFO_DocumentedAndUnknownInfoDoesNotWriteToStandardOutput)
{
    ProtocolEngine engine;

    for (const char* info :
         {"INFO timeout_turn 4000", "INFO timeout_match 60000", "INFO time_left -1",
          "INFO time_left 2147483647", "INFO max_memory 1024", "INFO game_type 1", "INFO rule 2",
          "INFO evaluate 7,7", "INFO folder /tmp/moku", "INFO unknown_key 123"})
    {
        EXPECT_TRUE(engine.execute(info).empty()) << info;
    }
}

/**
 * "END"
 * This command terminates the engine.
 */
TEST_F(GomocupProtocolTest, END_EndStopsEngineWithoutOutput)
{
    ProtocolEngine engine;

    EXPECT_TRUE(engine.is_running());
    EXPECT_TRUE(engine.execute("END").empty());
    EXPECT_FALSE(engine.is_running());
}

/**
 * "ABOUT"
 * Each item must be a keyword followed by = and a quoted text value.
 */
TEST_F(GomocupProtocolTest, ABOUT_AboutReturnsMachineReadableInformation)
{
    ProtocolEngine engine;

    const std::string output = engine.execute("ABOUT");

    const std::regex about_pattern(
        R"(^[^=,\r\n]+ *= *"[^"\r\n]*"(, *[^=,\r\n]+ *= *"[^"\r\n]*")*(\r\n|\n|\r)$)");
    EXPECT_TRUE(std::regex_match(output, about_pattern));
}

/**
 * "RESTART"
 * This command is supposed to reset the board.
 * The board size is supposed to stay the same.
 */
TEST_F(GomocupProtocolTest, RESTART_RestartClearsBoard)
{
    ProtocolEngine engine;
    engine.execute("START 15");
    engine.execute("BEGIN");

    EXPECT_EQ(engine.execute("RESTART"), "OK\n");
    EXPECT_EQ(engine.get_board().black.count(), 0);
    EXPECT_EQ(engine.get_board().white.count(), 0);
    EXPECT_EQ(g_config.board_size, 15);
}

/**
 * "TAKEBACK [X],[Y]"
 * This command is used to undo the last move.
 */
TEST_F(GomocupProtocolTest, TAKEBACK_TakebackRemovesStone)
{
    ProtocolEngine engine;
    engine.execute("START 15");
    engine.execute("BEGIN");

    EXPECT_EQ(engine.execute("TAKEBACK 7,7"), "OK\n");
    EXPECT_FALSE(engine.get_board().test_pos(7 * 15 + 7));
}

TEST_F(GomocupProtocolTest, TAKEBACK_TakebackRejectsMissingStone)
{
    ProtocolEngine engine;
    engine.execute("START 15");

    const std::string output = engine.execute("TAKEBACK 7,7");

    EXPECT_NE(output.find("ERROR"), std::string::npos);
}

/**
 * "RECTSTART [width],[height]"
 * This command is similar to "START", but it initializes a rectangular board.
 */
TEST_F(GomocupProtocolTest, RECTSTART_RectStartReportsUnsupportedBoard)
{
    ProtocolEngine engine;

    const std::string output = engine.execute("RECTSTART 20,15");

    EXPECT_NE(output.find("ERROR"), std::string::npos);
}

/**
 * If an unknown command is sent, "UNKNOWN [message] should be displayed."
 */
TEST_F(GomocupProtocolTest, UnknownCommandReturnsUnknown)
{
    ProtocolEngine engine;

    const std::string output = engine.execute("SOMETHING");
    const std::regex unknown_pattern(R"(^UNKNOWN( [^\r\n]*)?(\r\n|\n|\r)$)");
    EXPECT_TRUE(std::regex_match(output, unknown_pattern));
    EXPECT_TRUE(engine.is_running());
}

TEST_F(GomocupProtocolTest, RunIgnoresEmptyLinesAndAcceptsManagerCRLF)
{
    ProtocolEngine engine;

    const std::string output =
        engine.execute_stream("\r\nSTART 15\r\n\r\nINFO unknown_key 1\r\nEND\r\n");

    EXPECT_TRUE(is_ok_response(output));
    EXPECT_FALSE(engine.is_running());
}