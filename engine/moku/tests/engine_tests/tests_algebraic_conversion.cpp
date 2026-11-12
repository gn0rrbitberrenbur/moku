#include "config.hpp"
#include "game/tools/algebraic_to_index.hpp"

#include <gtest/gtest.h>

// Test cases for the algebraic_to_index function
TEST(AlgebraicConversion, ValidInputs)
{
    EXPECT_EQ(algebraic_to_index("A1", g_config.board_size), 0);
    EXPECT_EQ(algebraic_to_index("A2", g_config.board_size), 15);
    EXPECT_EQ(algebraic_to_index("B1", g_config.board_size), 1);
    EXPECT_EQ(algebraic_to_index("b1", g_config.board_size), 1);
    EXPECT_EQ(algebraic_to_index("B2", g_config.board_size), 16);
    EXPECT_EQ(algebraic_to_index("a3", g_config.board_size), 30);
    EXPECT_EQ(algebraic_to_index("A4", g_config.board_size), 45);
    EXPECT_EQ(algebraic_to_index("O15", g_config.board_size), 224);
}

TEST(AlgebraicConversion, InvalidInputs)
{
    EXPECT_EQ(algebraic_to_index("", g_config.board_size), -1);
    EXPECT_EQ(algebraic_to_index("Z1", g_config.board_size), -1);
    EXPECT_EQ(algebraic_to_index("A0", g_config.board_size), -1);
    EXPECT_EQ(algebraic_to_index("A16", g_config.board_size), -1);
}