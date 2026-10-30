#pragma once

#include "board/board.hpp"
#include "engine/engine_error.hpp"
#include "settings/settings.hpp"

#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace cli
{
struct TestSettings
{
    int games = 10;
    int size = 15;
    Rule rule = Rule::Freestyle;
    int turnMilliseconds = 5000;
    int openingStones = 2;
    std::uint32_t seed = 0;
    int startupMilliseconds = 10000;
    std::array<EngineEntry, 2> engines;
    std::filesystem::path output;
};

enum class GameEnd
{
    Five,
    BoardFull,
    Forfeit
};

struct TimedMove
{
    Move move;
    std::size_t engine;
    double milliseconds;
};

struct GameRecord
{
    int index = 0;
    std::vector<Move> opening;
    std::size_t black = 0;
    std::optional<std::size_t> winner;
    GameEnd end = GameEnd::BoardFull;
    EngineFailure forfeitCause = EngineFailure::Protocol;
    std::string forfeitMessage;
    std::vector<TimedMove> moves;
};

struct TestOutcome
{
    std::vector<GameRecord> games;
    bool completed = false;
    std::string abortReason;
    std::string startedAt;
    std::string finishedAt;
};

struct ColorStats
{
    int games = 0;
    int wins = 0;
    int losses = 0;
    int draws = 0;
};

struct TimeStats
{
    int moves = 0;
    double average = 0;
    double median = 0;
    double maximum = 0;
};

struct EngineSummary
{
    int wins = 0;
    int losses = 0;
    int draws = 0;
    double score = 0;
    ColorStats asBlack;
    ColorStats asWhite;
    std::array<int, 5> forfeits{};
    TimeStats moveTime;
};

struct TestSummary
{
    int games = 0;
    int draws = 0;
    std::array<EngineSummary, 2> engines;
    double averageLength = 0;
    int minimumLength = 0;
    int maximumLength = 0;
    std::optional<double> elo;
    std::optional<double> eloLower;
    std::optional<double> eloUpper;
};

/**
 * @param record finished game
 * @return number of stones on the board at the end of the game, including the opening
 */
int gameLength(const GameRecord& record);

/**
 * Aggregates results, color splits, forfeits, move times, game lengths and the Elo estimate.
 *
 * @param games finished games
 * @return statistics; Elo values are from the perspective of engine 0 against engine 1
 */
TestSummary summarize(const std::vector<GameRecord>& games);

/**
 * @param settings test configuration
 * @param outcome games played so far and completion state
 * @return UTF-8 JSON document
 */
std::string reportJson(const TestSettings& settings, const TestOutcome& outcome);

/**
 * Atomically writes the JSON report to settings.output.
 *
 * @param settings test configuration including the output path
 * @param outcome games played so far and completion state
 */
void writeTestReport(const TestSettings& settings, const TestOutcome& outcome);

/**
 * @return current UTC time in ISO 8601 format
 */
std::string utcTimestamp();
}
