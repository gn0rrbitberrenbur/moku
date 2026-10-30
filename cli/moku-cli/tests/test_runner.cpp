#include "tests/test_runner.hpp"

#include "engine/engine.hpp"

#include <algorithm>
#include <chrono>
#include <memory>
#include <random>

namespace cli
{
namespace
{
template <typename T>
T await(std::future<T>& future, const TestPoll& poll, const TestProgress& progress)
{
    while (future.wait_for(std::chrono::milliseconds(10)) != std::future_status::ready)
    {
        if (poll && poll(progress))
        {
            throw TestAborted();
        }
    }
    return future.get();
}

void validate(const TestSettings& settings)
{
    if (settings.games < 1 || settings.games > 10000 || settings.openingStones < 0 ||
        settings.openingStones > 8 || settings.output.empty())
    {
        throw std::invalid_argument("Invalid test configuration.");
    }
}
}

std::vector<std::vector<Move>> generateOpenings(int size, int stones, int count, std::uint32_t seed)
{
    const int center = size / 2;
    const int radius = std::min(3, center);
    std::vector<Move> cells;
    for (int y = center - radius; y <= center + radius; ++y)
    {
        for (int x = center - radius; x <= center + radius; ++x)
        {
            if (x < size && y < size)
            {
                cells.push_back({x, y});
            }
        }
    }
    if (stones < 0 || stones > static_cast<int>(cells.size()) || stones > 8)
    {
        throw std::invalid_argument("Opening stone count must be between 0 and 8.");
    }
    std::mt19937 random(seed);
    std::vector<std::vector<Move>> openings;
    for (int i = 0; i < count; ++i)
    {
        auto pool = cells;
        for (int j = 0; j < stones; ++j)
        {
            const auto remaining = static_cast<std::uint32_t>(pool.size() - j);
            std::swap(pool[j], pool[j + random() % remaining]);
        }
        openings.emplace_back(pool.begin(), pool.begin() + stones);
    }
    return openings;
}

GameRecord playTestGame(const TestSettings& settings, int index, const std::vector<Move>& opening,
                        std::size_t black, const TestPoll& poll,
                        const std::vector<GameRecord>& finished)
{
    Board board(settings.size, settings.rule);
    GameRecord record;
    record.index = index;
    record.opening = opening;
    record.black = black;
    std::vector<PlacedStone> history;
    Stone turn = Stone::Black;
    for (const auto move : opening)
    {
        board.play(move, turn);
        history.push_back({move, turn});
        turn = opposite(turn);
    }
    EngineConfig config;
    config.size = settings.size;
    config.rule = settings.rule;
    config.turnMilliseconds = settings.turnMilliseconds;
    config.humanOpponent = false;
    config.startupMilliseconds = settings.startupMilliseconds;
    std::array<std::unique_ptr<EngineWorker>, 2> workers;
    for (std::size_t engine = 0; engine < 2; ++engine)
    {
        workers[engine] = std::make_unique<EngineWorker>(settings.engines[engine], config);
    }
    for (auto& worker : workers)
    {
        auto ready = worker->ready();
        await(ready, poll, TestProgress{index, board.moves(), black, finished});
    }
    std::array<bool, 2> synced{};
    std::optional<Move> last;
    if (!history.empty())
    {
        last = history.back().move;
    }
    while (true)
    {
        const std::size_t engine = turn == Stone::Black ? black : 1 - black;
        const TestProgress progress{index, board.moves(), black, finished};
        if (poll && poll(progress))
        {
            throw TestAborted();
        }
        EngineReply reply{};
        try
        {
            auto future = !synced[engine] && history.size() > 1
                              ? workers[engine]->requestBoard(history, turn)
                              : workers[engine]->request(last);
            synced[engine] = true;
            reply = await(future, poll, progress);
        }
        catch (const TestAborted&)
        {
            throw;
        }
        catch (const std::exception& error)
        {
            const auto* engineError = dynamic_cast<const EngineError*>(&error);
            record.end = GameEnd::Forfeit;
            record.winner = 1 - engine;
            record.forfeitCause = engineError ? engineError->kind() : EngineFailure::Protocol;
            record.forfeitMessage = error.what();
            return record;
        }
        if (!board.legal(reply.move))
        {
            record.end = GameEnd::Forfeit;
            record.winner = 1 - engine;
            record.forfeitCause = EngineFailure::IllegalMove;
            record.forfeitMessage = settings.engines[engine].name + " played occupied square " +
                                    coordinate(reply.move) + ".";
            return record;
        }
        board.play(reply.move, turn);
        history.push_back({reply.move, turn});
        record.moves.push_back({reply.move, engine, reply.milliseconds});
        last = reply.move;
        if (board.wins(reply.move))
        {
            record.end = GameEnd::Five;
            record.winner = engine;
            return record;
        }
        if (board.full())
        {
            record.end = GameEnd::BoardFull;
            return record;
        }
        turn = opposite(turn);
    }
}

TestOutcome runTest(const TestSettings& settings, const TestPoll& poll)
{
    validate(settings);
    TestOutcome outcome;
    outcome.startedAt = utcTimestamp();
    outcome.finishedAt = outcome.startedAt;
    const auto openings = generateOpenings(settings.size, settings.openingStones,
                                           (settings.games + 1) / 2, settings.seed);
    writeTestReport(settings, outcome);
    for (int game = 0; game < settings.games; ++game)
    {
        const std::size_t black = game % 2 == 0 ? 0 : 1;
        try
        {
            outcome.games.push_back(
                playTestGame(settings, game + 1, openings[game / 2], black, poll, outcome.games));
        }
        catch (const std::exception& error)
        {
            outcome.abortReason = error.what();
            break;
        }
        outcome.finishedAt = utcTimestamp();
        writeTestReport(settings, outcome);
    }
    outcome.completed = outcome.abortReason.empty();
    outcome.finishedAt = utcTimestamp();
    writeTestReport(settings, outcome);
    return outcome;
}
}
