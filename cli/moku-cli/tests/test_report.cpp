#include "tests/test_report.hpp"

#include "file_handling/file_io.hpp"
#include "file_handling/json.hpp"

#include <algorithm>
#include <cmath>
#include <ctime>
#include <iomanip>
#include <limits>
#include <sstream>

namespace cli
{
namespace
{
std::string quote(const std::string& text)
{
    return jsonQuote(text);
}

std::string number(double value)
{
    if (!std::isfinite(value))
    {
        return "null";
    }
    std::ostringstream output;
    output.imbue(std::locale::classic());
    output << std::fixed << std::setprecision(3) << value;
    return output.str();
}

std::string number(const std::optional<double>& value)
{
    return value ? number(*value) : "null";
}

std::string stoneName(Stone stone)
{
    return stone == Stone::Black ? "\"black\"" : "\"white\"";
}

std::string moveJson(Move move)
{
    return "\"move\": " + quote(coordinate(move)) + ", \"x\": " + std::to_string(move.x) +
           ", \"y\": " + std::to_string(move.y);
}

std::string colorJson(const ColorStats& stats)
{
    return "{\"games\": " + std::to_string(stats.games) +
           ", \"wins\": " + std::to_string(stats.wins) +
           ", \"losses\": " + std::to_string(stats.losses) +
           ", \"draws\": " + std::to_string(stats.draws) + "}";
}

std::optional<double> eloFromScore(double score)
{
    if (score <= 0 || score >= 1)
    {
        return std::nullopt;
    }
    return -400.0 * std::log10(1.0 / score - 1.0);
}

void addResult(ColorStats& stats, int result)
{
    ++stats.games;
    if (result > 0)
    {
        ++stats.wins;
    }
    else if (result < 0)
    {
        ++stats.losses;
    }
    else
    {
        ++stats.draws;
    }
}
}

int gameLength(const GameRecord& record)
{
    return static_cast<int>(record.opening.size() + record.moves.size());
}

TestSummary summarize(const std::vector<GameRecord>& games)
{
    TestSummary summary;
    summary.games = static_cast<int>(games.size());
    std::array<std::vector<double>, 2> times;
    long long totalLength = 0;
    summary.minimumLength = std::numeric_limits<int>::max();
    for (const auto& game : games)
    {
        if (!game.winner)
        {
            ++summary.draws;
        }
        const int length = gameLength(game);
        totalLength += length;
        summary.minimumLength = std::min(summary.minimumLength, length);
        summary.maximumLength = std::max(summary.maximumLength, length);
        for (std::size_t engine = 0; engine < 2; ++engine)
        {
            auto& stats = summary.engines[engine];
            const int result = !game.winner ? 0 : *game.winner == engine ? 1 : -1;
            stats.wins += result > 0;
            stats.losses += result < 0;
            stats.draws += result == 0;
            addResult(game.black == engine ? stats.asBlack : stats.asWhite, result);
            if (game.end == GameEnd::Forfeit && result < 0)
            {
                ++stats.forfeits[static_cast<std::size_t>(game.forfeitCause)];
            }
        }
        for (const auto& move : game.moves)
        {
            times[move.engine].push_back(move.milliseconds);
        }
    }
    if (games.empty())
    {
        summary.minimumLength = 0;
    }
    else
    {
        summary.averageLength = static_cast<double>(totalLength) / summary.games;
    }
    for (std::size_t engine = 0; engine < 2; ++engine)
    {
        auto& stats = summary.engines[engine];
        stats.score = stats.wins + 0.5 * stats.draws;
        auto& values = times[engine];
        stats.moveTime.moves = static_cast<int>(values.size());
        if (!values.empty())
        {
            std::sort(values.begin(), values.end());
            double total = 0;
            for (const double value : values)
            {
                total += value;
            }
            stats.moveTime.average = total / values.size();
            const auto middle = values.size() / 2;
            stats.moveTime.median =
                values.size() % 2 ? values[middle] : (values[middle - 1] + values[middle]) / 2;
            stats.moveTime.maximum = values.back();
        }
    }
    if (summary.games > 0)
    {
        const auto& first = summary.engines[0];
        const double n = summary.games;
        const double score = first.score / n;
        const double variance =
            (first.wins * std::pow(1 - score, 2) + first.draws * std::pow(0.5 - score, 2) +
             first.losses * std::pow(score, 2)) /
            n;
        const double margin = 1.959964 * std::sqrt(variance / n);
        summary.elo = eloFromScore(score);
        if (summary.elo)
        {
            summary.eloLower = eloFromScore(score - margin);
            summary.eloUpper = eloFromScore(score + margin);
        }
    }
    return summary;
}

std::string reportJson(const TestSettings& settings, const TestOutcome& outcome)
{
    const auto summary = summarize(outcome.games);
    std::ostringstream json;
    json << "{\n  \"format\": \"moku-cli-test\",\n  \"version\": 1,\n";
    json << "  \"completed\": " << (outcome.completed ? "true" : "false") << ",\n";
    json << "  \"abortReason\": "
         << (outcome.abortReason.empty() ? "null" : quote(outcome.abortReason)) << ",\n";
    json << "  \"startedAt\": " << quote(outcome.startedAt) << ",\n";
    json << "  \"finishedAt\": " << quote(outcome.finishedAt) << ",\n";
    json << "  \"settings\": {\"games\": " << settings.games << ", \"boardSize\": " << settings.size
         << ", \"rule\": "
         << (settings.rule == Rule::Freestyle ? "\"freestyle\"" : "\"exact_five\"")
         << ", \"turnMilliseconds\": " << settings.turnMilliseconds
         << ", \"openingStones\": " << settings.openingStones << ", \"seed\": " << settings.seed
         << "},\n";
    json << "  \"engines\": [\n";
    for (std::size_t engine = 0; engine < 2; ++engine)
    {
        json << "    {\"index\": " << engine
             << ", \"name\": " << quote(settings.engines[engine].name)
             << ", \"path\": " << quote(settings.engines[engine].executable.u8string()) << "}"
             << (engine == 0 ? "," : "") << "\n";
    }
    json << "  ],\n  \"summary\": {\n";
    json << "    \"gamesPlayed\": " << summary.games << ",\n";
    json << "    \"draws\": " << summary.draws << ",\n";
    json << "    \"gameLength\": {\"average\": " << number(summary.averageLength)
         << ", \"minimum\": " << summary.minimumLength << ", \"maximum\": " << summary.maximumLength
         << "},\n";
    json << "    \"elo\": {\"perspective\": 0, \"difference\": " << number(summary.elo)
         << ", \"lower95\": " << number(summary.eloLower)
         << ", \"upper95\": " << number(summary.eloUpper) << "},\n";
    json << "    \"engines\": [\n";
    for (std::size_t engine = 0; engine < 2; ++engine)
    {
        const auto& stats = summary.engines[engine];
        json << "      {\"index\": " << engine << ", \"wins\": " << stats.wins
             << ", \"losses\": " << stats.losses << ", \"draws\": " << stats.draws
             << ", \"score\": " << number(stats.score) << ", \"scorePercent\": "
             << (summary.games ? number(100.0 * stats.score / summary.games) : "null")
             << ",\n       \"asBlack\": " << colorJson(stats.asBlack)
             << ", \"asWhite\": " << colorJson(stats.asWhite) << ",\n       \"forfeits\": {";
        for (std::size_t kind = 0; kind < stats.forfeits.size(); ++kind)
        {
            json << (kind ? ", " : "") << quote(failureName(static_cast<EngineFailure>(kind)))
                 << ": " << stats.forfeits[kind];
        }
        json << "},\n       \"moveTimeMs\": {\"moves\": " << stats.moveTime.moves
             << ", \"average\": " << number(stats.moveTime.average)
             << ", \"median\": " << number(stats.moveTime.median)
             << ", \"maximum\": " << number(stats.moveTime.maximum) << "}}"
             << (engine == 0 ? "," : "") << "\n";
    }
    json << "    ]\n  },\n  \"games\": [";
    for (std::size_t i = 0; i < outcome.games.size(); ++i)
    {
        const auto& game = outcome.games[i];
        const std::string result = !game.winner                 ? "draw"
                                   : *game.winner == game.black ? "black"
                                                                : "white";
        const std::string end = game.end == GameEnd::Five        ? "five"
                                : game.end == GameEnd::BoardFull ? "board_full"
                                                                 : "forfeit";
        json << (i ? "," : "") << "\n    {\"index\": " << game.index
             << ", \"black\": " << game.black << ", \"white\": " << 1 - game.black
             << ", \"result\": \"" << result
             << "\", \"winner\": " << (game.winner ? std::to_string(*game.winner) : "null")
             << ", \"end\": \"" << end << "\", \"length\": " << gameLength(game)
             << ",\n     \"forfeit\": ";
        if (game.end == GameEnd::Forfeit)
        {
            json << "{\"engine\": " << (game.winner ? 1 - *game.winner : 0)
                 << ", \"cause\": " << quote(failureName(game.forfeitCause))
                 << ", \"message\": " << quote(game.forfeitMessage) << "}";
        }
        else
        {
            json << "null";
        }
        json << ",\n     \"opening\": [";
        Stone stone = Stone::Black;
        for (std::size_t j = 0; j < game.opening.size(); ++j)
        {
            json << (j ? ", " : "") << "{" << moveJson(game.opening[j])
                 << ", \"color\": " << stoneName(stone) << "}";
            stone = opposite(stone);
        }
        json << "],\n     \"moves\": [";
        for (std::size_t j = 0; j < game.moves.size(); ++j)
        {
            const auto& move = game.moves[j];
            json << (j ? "," : "") << "\n       {" << moveJson(move.move)
                 << ", \"engine\": " << move.engine << ", \"color\": "
                 << stoneName(move.engine == game.black ? Stone::Black : Stone::White)
                 << ", \"ms\": " << number(move.milliseconds) << "}";
        }
        json << (game.moves.empty() ? "" : "\n     ") << "]}";
    }
    json << (outcome.games.empty() ? "" : "\n  ") << "]\n}\n";
    return json.str();
}

void writeTestReport(const TestSettings& settings, const TestOutcome& outcome)
{
    writeFileAtomically(settings.output, reportJson(settings, outcome), "test report");
}

std::string utcTimestamp()
{
    const auto now = std::time(nullptr);
    std::tm utc{};
#ifdef _WIN32
    gmtime_s(&utc, &now);
#else
    gmtime_r(&now, &utc);
#endif
    std::ostringstream output;
    output << std::put_time(&utc, "%Y-%m-%dT%H:%M:%SZ");
    return output.str();
}
}
