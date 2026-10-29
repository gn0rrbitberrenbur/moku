#include "config.hpp"
#include "engine/gomocup_protocol/gomocup_engine.hpp"

#include <gtest/gtest.h>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>

/**
 * This test suit implements tests that check whether the engine is compliant
 * to the Gomoku AI protocol (see: https://plastovicka.github.io/protocl2en.htm)
 */

class ProtocolEngine : public Engine
{
public:
    std::string execute(const std::string& command, const std::string& input = "")
    {
        std::ostringstream output;

        std::streambuf* old_cout = std::cout.rdbuf(output.rdbuf());
        std::streambuf* old_cin = nullptr;

        std::istringstream input_stream(input);
        if (!input.empty())
        {
            old_cin = std::cin.rdbuf(input_stream.rdbuf());
        }

        process_command(command);

        if (old_cin != nullptr)
        {
            std::cin.rdbuf(old_cin);
        }

        std::cout.rdbuf(old_cout);
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
        std::regex coordinate_pattern(R"(^[0-9]+,[0-9]+\n$)");
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
};

/**
 * "START [size]"
 * Following this command, the brain should initialize an empty board but does not make a move yet.
 * If the size is good, the engine answers "OK"
 * If the size is unsupported, the engine answers "ERROR message"
 */
TEST_F(GomocupProtocolTest, START_StartAcceptsSupportedBoardSizes)
{
    ProtocolEngine engine;

    EXPECT_EQ(engine.execute("START 5"), "OK\n");
    EXPECT_EQ(g_config.board_size, 5);

    EXPECT_EQ(engine.execute("START 19"), "OK\n");
    EXPECT_EQ(g_config.board_size, 19);
}

TEST_F(GomocupProtocolTest, START_StartRejectsUnsupportedBoardSizes)
{
    ProtocolEngine engine;

    EXPECT_EQ(engine.execute("START 4").find("ERROR"), 0);
    EXPECT_EQ(engine.execute("START 20").find("ERROR"), 0);
}

TEST_F(GomocupProtocolTest, START_StartCreatesEmptyBoard)
{
    ProtocolEngine engine;

    engine.execute("START 15");

    EXPECT_EQ(engine.get_board().black.count(), 0);
    EXPECT_EQ(engine.get_board().white.count(), 0);
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
    EXPECT_EQ(engine.get_board().black.count() + engine.get_board().white.count(), 2);
}

TEST_F(GomocupProtocolTest, TURN_TurnStoresLegalReplyAndOpponentMoveWithoutOverlap)
{
    for (const bool engine_is_black : {false, true})
    {
        SCOPED_TRACE(engine_is_black ? "Engine plays black" : "Engine plays white");
        g_config.max_depth = 1;
        ProtocolEngine engine;
        ASSERT_EQ(engine.execute("START 15"), "OK\n");
        engine.execute("INFO timeout_turn 100");

        if (engine_is_black)
        {
            ASSERT_NO_FATAL_FAILURE(expect_coordinate(engine.execute("BEGIN"), 15));
        }

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

        const Board& after = engine.get_board();
        const auto& own_stones = engine_is_black ? after.black : after.white;
        const auto& opponent_stones = engine_is_black ? after.white : after.black;
        EXPECT_TRUE(own_stones.test(reply_pos));
        EXPECT_FALSE(opponent_stones.test(reply_pos));
        EXPECT_TRUE(opponent_stones.test(opponent_pos));
        EXPECT_FALSE(own_stones.test(opponent_pos));
        EXPECT_TRUE((after.black & after.white).none());

        auto expected_black = before.black;
        auto expected_white = before.white;
        (engine_is_black ? expected_black : expected_white).set(reply_pos);
        (engine_is_black ? expected_white : expected_black).set(opponent_pos);
        EXPECT_EQ(after.black, expected_black);
        EXPECT_EQ(after.white, expected_white);
    }
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
    ProtocolEngine engine;
    engine.execute("START 15");

    const std::string output = engine.execute("BOARD", "7,7,1\n"
                                                       "8,7,2\n"
                                                       "6,7,1\n"
                                                       "DONE\n");

    expect_coordinate(output, 15);
    EXPECT_EQ(engine.get_board().black.count() + engine.get_board().white.count(), 4);
}

TEST_F(GomocupProtocolTest, BOARD_BoardPreservesPositionsAndAddsExactlyOneOwnStone)
{
    for (const bool engine_is_black : {false, true})
    {
        SCOPED_TRACE(engine_is_black ? "Engine plays black" : "Engine plays white");
        g_config.max_depth = 1;
        ProtocolEngine engine;
        ASSERT_EQ(engine.execute("START 15"), "OK\n");
        engine.execute("INFO timeout_turn 100");

        const int own_pos = 3 * 15 + 2;
        const int opponent_pos = 6 * 15 + 5;
        const int extra_opponent_pos = 9 * 15 + 8;
        std::string input = "5,6,2\n2,3,1\n";
        if (!engine_is_black)
        {
            input += "8,9,2\n";
        }
        input += "DONE\n";

        Board expected;
        expected.make_move(own_pos, engine_is_black);
        expected.make_move(opponent_pos, !engine_is_black);
        if (!engine_is_black)
        {
            expected.make_move(extra_opponent_pos, !engine_is_black);
        }

        const std::string output = engine.execute("BOARD", input);
        ASSERT_NO_FATAL_FAILURE(expect_coordinate(output, 15));
        std::istringstream stream(output);
        int reply_x;
        int reply_y;
        char comma;
        ASSERT_TRUE(stream >> reply_x >> comma >> reply_y);
        ASSERT_GE(reply_x, 0);
        ASSERT_LT(reply_x, 15);
        ASSERT_GE(reply_y, 0);
        ASSERT_LT(reply_y, 15);
        const int reply_pos = reply_y * 15 + reply_x;
        EXPECT_FALSE(expected.test_pos(reply_pos));
        expected.make_move(reply_pos, engine_is_black);

        const Board& after = engine.get_board();
        const auto& own_stones = engine_is_black ? after.black : after.white;
        const auto& opponent_stones = engine_is_black ? after.white : after.black;
        EXPECT_EQ(own_stones.count(), 2);
        EXPECT_EQ(opponent_stones.count(), engine_is_black ? 1 : 2);
        EXPECT_EQ(after.black, expected.black);
        EXPECT_EQ(after.white, expected.white);
        EXPECT_TRUE((after.black & after.white).none());
    }
}

TEST_F(GomocupProtocolTest, BOARD_BoardResetsPreviousPosition)
{
    ProtocolEngine engine;
    engine.execute("START 15");
    engine.execute("BEGIN");

    EXPECT_EQ(engine.get_board().black.count() + engine.get_board().white.count(), 1);

    engine.execute("BOARD", "0,0,1\nDONE\n");

    EXPECT_TRUE(engine.get_board().test_pos(0));
    EXPECT_FALSE(engine.get_board().test_pos(112));
}

/**
 * "INFO [key],[value]"
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
    EXPECT_EQ(g_config.time_limit_ms, 4000);
}

TEST_F(GomocupProtocolTest, INFO_InfoUpdatesSupportedValues)
{
    ProtocolEngine engine;

    engine.execute("INFO timeout_match 60000");
    engine.execute("INFO time_left 50000");
    engine.execute("INFO max_memory 1024");
    engine.execute("INFO game_type 1");
    engine.execute("INFO rule 2");
    engine.execute("INFO folder /tmp/moku");

    EXPECT_EQ(g_config.timeout_match, 60000);
    EXPECT_EQ(g_config.time_left, 50000);
    EXPECT_EQ(g_config.max_memory, 1024);
    EXPECT_EQ(g_config.game_type, 1);
    EXPECT_EQ(g_config.rule, 2);
    EXPECT_EQ(g_config.folder, "/tmp/moku");
}

TEST_F(GomocupProtocolTest, INFO_UnknownInfoIsIgnored)
{
    ProtocolEngine engine;

    EXPECT_TRUE(engine.execute("INFO unknown_key 123").empty());
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
 * This command should send some information about the engine in the following format:
 * key="value", key="value", ...
 */
TEST_F(GomocupProtocolTest, ABOUT_AboutReturnsMachineReadableInformation)
{
    ProtocolEngine engine;

    const std::string output = engine.execute("ABOUT");

    EXPECT_NE(output.find("name=\"moku\""), std::string::npos);
    EXPECT_NE(output.find("version=\""), std::string::npos);
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

    EXPECT_EQ(engine.execute("SOMETHING"), "UNKNOWN SOMETHING\n");
}

/**
 * Commands should be case insensitive (converted to uppercase internally)
 */
TEST_F(GomocupProtocolTest, CommandsAreCaseInsensitive)
{
    ProtocolEngine engine;

    EXPECT_EQ(engine.execute("start 15"), "OK\n");
    EXPECT_EQ(engine.execute("begin"), "7,7\n");
}