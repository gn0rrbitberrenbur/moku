#pragma once

#include "tests/test_report.hpp"

#include <functional>
#include <stdexcept>
#include <vector>

namespace cli
{
struct TestProgress
{
    int game;
    int stones;
    std::size_t black;
    const std::vector<GameRecord>& finished;
};

/**
 * Called repeatedly while a test runs.
 *
 * @param progress current game, stones on the board and finished games
 * @return true to abort the test
 */
using TestPoll = std::function<bool(const TestProgress& progress)>;

class TestAborted : public std::runtime_error
{
public:
    TestAborted() : std::runtime_error("Cancelled by user.") {}
};

/**
 * Generates reproducible random openings near the board center. Stones alternate Black/White.
 *
 * @param size board size
 * @param stones stones per opening, 0 to 8
 * @param count number of openings
 * @param seed random seed
 * @return openings in play order
 */
std::vector<std::vector<Move>> generateOpenings(int size, int stones, int count,
                                                std::uint32_t seed);

/**
 * Plays one game with two freshly started engine processes.
 *
 * @param settings test configuration
 * @param index one-based game number
 * @param opening stones placed before the engines move
 * @param black engine index (0 or 1) that plays Black
 * @param poll progress callback that can abort the game
 * @param finished games completed before this one, forwarded to poll
 * @return game record; engine failures are recorded as forfeits
 */
GameRecord playTestGame(const TestSettings& settings, int index, const std::vector<Move>& opening,
                        std::size_t black, const TestPoll& poll,
                        const std::vector<GameRecord>& finished);

/**
 * Runs all games, alternating the first player and repeating each opening with swapped colors.
 * The JSON report is rewritten after every game and at the end.
 *
 * @param settings test configuration
 * @param poll progress callback that can abort the test
 * @return all finished games and the completion state
 */
TestOutcome runTest(const TestSettings& settings, const TestPoll& poll);
} // namespace cli
