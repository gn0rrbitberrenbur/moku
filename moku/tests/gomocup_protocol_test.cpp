#include "config.hpp"
#include "engine/gomocup_engine.hpp"

#include <gtest/gtest.h>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>

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

TEST_F(GomocupProtocolTest, StartAcceptsSupportedBoardSizes)
{
    ProtocolEngine engine;

    EXPECT_EQ(engine.execute("START 5"), "OK\n");
    EXPECT_EQ(g_config.board_size, 5);

    EXPECT_EQ(engine.execute("START 20"), "OK\n");
    EXPECT_EQ(g_config.board_size, 20);
}

TEST_F(GomocupProtocolTest, StartRejectsUnsupportedBoardSizes)
{
    ProtocolEngine engine;

    EXPECT_EQ(engine.execute("START 4").find("ERROR"), 0);
    EXPECT_EQ(engine.execute("START 21").find("ERROR"), 0);
}

TEST_F(GomocupProtocolTest, StartCreatesEmptyBoard)
{
    ProtocolEngine engine;

    engine.execute("START 15");

    EXPECT_EQ(engine.get_board().black.count(), 0);
    EXPECT_EQ(engine.get_board().white.count(), 0);
}

TEST_F(GomocupProtocolTest, BeginReturnsMoveAndUpdatesBoard)
{
    ProtocolEngine engine;
    engine.execute("START 15");

    const std::string output = engine.execute("BEGIN");

    expect_coordinate(output, 15);
    EXPECT_EQ(engine.get_board().black.count() + engine.get_board().white.count(), 1);
}

TEST_F(GomocupProtocolTest, BeginOnEmptyBoardUsesCenter)
{
    ProtocolEngine engine;
    engine.execute("START 15");

    EXPECT_EQ(engine.execute("BEGIN"), "7,7\n");
}

TEST_F(GomocupProtocolTest, TurnReturnsMoveAndRegistersOpponentMove)
{
    ProtocolEngine engine;
    engine.execute("START 15");
    engine.execute("INFO timeout_turn 1");

    const std::string output = engine.execute("TURN 0,0");

    expect_coordinate(output, 15);
    EXPECT_TRUE(engine.get_board().test_pos(0));
    EXPECT_EQ(engine.get_board().black.count() + engine.get_board().white.count(), 2);
}

TEST_F(GomocupProtocolTest, TurnRejectsNegativeCoordinates)
{
    ProtocolEngine engine;
    engine.execute("START 15");

    const std::string output = engine.execute("TURN -1,0");

    EXPECT_NE(output.find("ERROR"), std::string::npos);
}

TEST_F(GomocupProtocolTest, TurnRejectsCoordinatesOutsideBoard)
{
    ProtocolEngine engine;
    engine.execute("START 15");

    const std::string output = engine.execute("TURN 15,0");

    EXPECT_NE(output.find("ERROR"), std::string::npos);
}

TEST_F(GomocupProtocolTest, TurnRejectsOccupiedPosition)
{
    ProtocolEngine engine;
    engine.execute("START 15");
    engine.execute("BEGIN");

    const std::string output = engine.execute("TURN 7,7");

    EXPECT_NE(output.find("ERROR"), std::string::npos);
}

TEST_F(GomocupProtocolTest, BoardLoadsOwnAndOpponentStones)
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

TEST_F(GomocupProtocolTest, BoardResetsPreviousPosition)
{
    ProtocolEngine engine;
    engine.execute("START 15");
    engine.execute("BEGIN");

    EXPECT_EQ(engine.get_board().black.count() + engine.get_board().white.count(), 1);

    engine.execute("BOARD", "0,0,1\nDONE\n");

    EXPECT_TRUE(engine.get_board().test_pos(0));
    EXPECT_FALSE(engine.get_board().test_pos(112));
}

TEST_F(GomocupProtocolTest, InfoDoesNotWriteToStandardOutput)
{
    ProtocolEngine engine;

    EXPECT_TRUE(engine.execute("INFO timeout_turn 4000").empty());
    EXPECT_EQ(g_config.time_limit_ms, 4000);
}

TEST_F(GomocupProtocolTest, InfoUpdatesSupportedValues)
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

TEST_F(GomocupProtocolTest, UnknownInfoIsIgnored)
{
    ProtocolEngine engine;

    EXPECT_TRUE(engine.execute("INFO unknown_key 123").empty());
}

TEST_F(GomocupProtocolTest, RestartClearsBoard)
{
    ProtocolEngine engine;
    engine.execute("START 15");
    engine.execute("BEGIN");

    EXPECT_EQ(engine.execute("RESTART"), "OK\n");
    EXPECT_EQ(engine.get_board().black.count(), 0);
    EXPECT_EQ(engine.get_board().white.count(), 0);
    EXPECT_EQ(g_config.board_size, 15);
}

TEST_F(GomocupProtocolTest, TakebackRemovesStone)
{
    ProtocolEngine engine;
    engine.execute("START 15");
    engine.execute("BEGIN");

    EXPECT_EQ(engine.execute("TAKEBACK 7,7"), "OK\n");
    EXPECT_FALSE(engine.get_board().test_pos(7 * 15 + 7));
}

TEST_F(GomocupProtocolTest, TakebackRejectsMissingStone)
{
    ProtocolEngine engine;
    engine.execute("START 15");

    const std::string output = engine.execute("TAKEBACK 7,7");

    EXPECT_NE(output.find("ERROR"), std::string::npos);
}

TEST_F(GomocupProtocolTest, RectStartReportsUnsupportedBoard)
{
    ProtocolEngine engine;

    const std::string output = engine.execute("RECTSTART 20,15");

    EXPECT_NE(output.find("ERROR"), std::string::npos);
}

TEST_F(GomocupProtocolTest, AboutReturnsMachineReadableInformation)
{
    ProtocolEngine engine;

    const std::string output = engine.execute("ABOUT");

    EXPECT_NE(output.find("name=\"moku\""), std::string::npos);
    EXPECT_NE(output.find("version=\""), std::string::npos);
}

TEST_F(GomocupProtocolTest, UnknownCommandReturnsUnknown)
{
    ProtocolEngine engine;

    EXPECT_EQ(engine.execute("SOMETHING"), "UNKNOWN SOMETHING\n");
}

TEST_F(GomocupProtocolTest, CommandsAreCaseInsensitive)
{
    ProtocolEngine engine;

    EXPECT_EQ(engine.execute("start 15"), "OK\n");
    EXPECT_EQ(engine.execute("begin"), "7,7\n");
}

TEST_F(GomocupProtocolTest, EndStopsEngineWithoutOutput)
{
    ProtocolEngine engine;

    EXPECT_TRUE(engine.is_running());
    EXPECT_TRUE(engine.execute("END").empty());
    EXPECT_FALSE(engine.is_running());
}