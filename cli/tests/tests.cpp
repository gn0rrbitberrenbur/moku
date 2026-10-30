#include "board/board.hpp"
#include "engine/engine.hpp"
#include "file_handling/json.hpp"
#include "settings/settings.hpp"
#include "tests/test_runner.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <future>
#include <iostream>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

namespace
{
void require(bool condition, const std::string& message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}

void rejects(const std::function<void()>& operation, const std::string& expected = "")
{
    try
    {
        operation();
    }
    catch (const std::exception& error)
    {
        require(expected.empty() || std::string(error.what()).find(expected) != std::string::npos,
                "Unexpected error: " + std::string(error.what()));
        return;
    }
    throw std::runtime_error("Expected operation to fail.");
}

template <typename T> T await(std::future<T> future)
{
    require(future.wait_for(std::chrono::seconds(5)) == std::future_status::ready,
            "Worker did not complete within 5 seconds.");
    return future.get();
}

void boardTests()
{
    rejects([] { cli::Board board(4); });
    rejects([] { cli::Board board(20); });
    cli::Board board(19);
    require(board.size() == 19 && board.moves() == 0, "Board initialization.");
    board.play({18, 18}, cli::Stone::Black);
    require(board.at({18, 18}) == cli::Stone::Black && board.moves() == 1, "Board move.");
    rejects([&] { board.play({18, 18}, cli::Stone::White); });
    rejects([&] { board.play({0, 0}, cli::Stone::Empty); });
    rejects([&] { board.at({19, 0}); });
    require(!board.legal({-1, 0}) && !board.legal({0, 19}), "Out-of-bounds legality.");
    require(cli::coordinate({18, 18}) == "S19", "Coordinate formatting.");
    const auto parsed = cli::parseCoordinate("  h8 ", 15);
    require(parsed && parsed->x == 7 && parsed->y == 7, "Coordinate parsing.");
    for (const auto& text : {"A0", "T1", "A20", "H8junk", "8H", "", "H999999999999"})
    {
        require(!cli::parseCoordinate(text, 19), "Invalid coordinate accepted.");
    }
    const auto protocol = cli::parseProtocolMove(" 18, 0 ", 19);
    require(protocol.x == 18 && protocol.y == 0, "Protocol coordinate parsing.");
    for (const auto& text : {"19,0", "-1,1", "1 2", "1,2junk", "a,b", "1,2,3", ""})
    {
        rejects([&] { cli::parseProtocolMove(text, 19); });
    }
    const std::vector<cli::Move> directions{{1, 0}, {0, 1}, {1, 1}, {1, -1}};
    for (const auto rule : {cli::Rule::Freestyle, cli::Rule::ExactFive})
    {
        for (const auto d : directions)
        {
            cli::Board line(19, rule);
            cli::Move last{};
            for (int i = 0; i < 5; ++i)
            {
                last = {3 + i * d.x, 8 + i * d.y};
                line.play(last, cli::Stone::White);
                require(line.wins(last) == (i == 4), "Five-stone win detection.");
            }
            line.play({3 + 5 * d.x, 8 + 5 * d.y}, cli::Stone::White);
            require(line.wins(last) == (rule == cli::Rule::Freestyle), "Overline rule detection.");
        }
    }
    cli::Board bridge(9, cli::Rule::ExactFive);
    for (int x : {0, 1, 3, 4})
    {
        bridge.play({x, 0}, cli::Stone::Black);
    }
    bridge.play({2, 0}, cli::Stone::Black);
    require(bridge.wins({2, 0}), "Win crossing both sides of last move.");
    cli::Board full(5);
    for (int y = 0; y < 5; ++y)
    {
        for (int x = 0; x < 5; ++x)
        {
            full.play({x, y}, (x + y) % 2 ? cli::Stone::Black : cli::Stone::White);
        }
    }
    require(full.full(), "Full board detection.");
}

void storeTests(const std::filesystem::path& directory, const std::filesystem::path& executable)
{
    const auto file = directory / "config with spaces" / "engines.cfg";
    cli::EngineStore store(file);
    require(store.entries().empty(), "New registry must be empty.");
    store.add({"First \"engine\"", executable});
    require(cli::EngineStore(file).entries()[0].name == "First \"engine\"",
            "Registry quoted round-trip.");
    rejects([&] { store.add({"First \"engine\"", executable}); }, "already exists");
    rejects([&] { store.add({"", executable}); });
    rejects([&] { store.add({"bad\nname", executable}); });
    rejects([&] { store.add({"Missing", directory / "missing"}); });
    store.add({"Second", executable});
    rejects([&] { store.edit(1, {"First \"engine\"", executable}); });
    store.edit(0, {"Renamed", executable});
    require(cli::EngineStore(file).entries()[0].name == "Renamed", "Persistent edit.");
    store.remove(1);
    require(cli::EngineStore(file).entries().size() == 1, "Persistent removal.");
#ifdef _WIN32
    {
        HANDLE lock = CreateFileW(file.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                  nullptr, OPEN_EXISTING, 0, nullptr);
        require(lock != INVALID_HANDLE_VALUE, "Cannot lock registry for transient-lock test.");
        auto release = std::async(std::launch::async,
                                  [lock]
                                  {
                                      std::this_thread::sleep_for(std::chrono::milliseconds(120));
                                      CloseHandle(lock);
                                  });
        store.edit(0, {"After transient lock", executable});
        release.get();
        require(cli::EngineStore(file).entries()[0].name == "After transient lock",
                "Registry update must tolerate transient Windows file locks.");
    }
#endif
    const auto unicodePath =
        directory / std::filesystem::u8path(u8"engine-\u00e4") / executable.filename();
    std::filesystem::create_directory(unicodePath.parent_path());
    std::filesystem::copy_file(executable, unicodePath);
    std::filesystem::permissions(unicodePath, std::filesystem::status(executable).permissions());
    store.add({u8"Engine \u00e4", unicodePath});
    const cli::EngineStore reloaded(file);
    require(reloaded.entries()[1].name == u8"Engine \u00e4" &&
                reloaded.entries()[1].executable == unicodePath,
            "UTF-8 registry round-trip.");
    {
        cli::EngineWorker worker(reloaded.entries()[1], {});
        await(worker.ready());
        require(await(worker.request(std::nullopt)).move.x == 0, "Unicode executable path.");
    }
    rejects([&] { store.remove(99); });
    const auto corrupt = directory / "corrupt.cfg";
    {
        std::ofstream output(corrupt);
        output << "not a registry\n";
    }
    rejects([&] { cli::EngineStore invalid(corrupt); }, "Invalid engine registry");
    const auto blocked = directory / "blocked";
    std::filesystem::create_directory(blocked);
    cli::EngineStore blockedStore(blocked / "engines.cfg");
    std::filesystem::remove(blocked);
    {
        std::ofstream output(blocked);
        output << "not a directory";
    }
    rejects([&] { blockedStore.add({"Cannot save", executable}); });
    require(blockedStore.entries().empty(), "Failed save must not mutate memory.");
}

std::filesystem::path variant(const std::filesystem::path& original,
                              const std::filesystem::path& directory, const std::string& name)
{
    const auto target = directory / ("mock-" + name + original.extension().string());
    std::filesystem::copy_file(original, target);
    std::filesystem::permissions(target, std::filesystem::status(original).permissions());
    return target;
}

void match(const std::filesystem::path& executable, cli::Rule rule, int timeout, bool human)
{
    cli::EngineConfig config;
    config.size = 5;
    config.rule = rule;
    config.turnMilliseconds = timeout;
    config.humanOpponent = human;
    cli::EngineWorker black({"Black", executable}, config);
    cli::EngineWorker white({"White", executable}, config);
    await(black.ready());
    await(white.ready());
    cli::Board board(config.size, rule);
    cli::Stone turn = cli::Stone::Black;
    std::optional<cli::Move> last;
    for (int i = 0; i < 25; ++i)
    {
        const auto move = await((turn == cli::Stone::Black ? black : white).request(last)).move;
        require(board.legal(move), "Engine returned an illegal move.");
        board.play(move, turn);
        last = move;
        if (board.wins(move) || board.full())
        {
            return;
        }
        turn = cli::opposite(turn);
    }
    throw std::runtime_error("Match failed to terminate.");
}

void protocolTests(const std::filesystem::path& directory, const std::filesystem::path& executable)
{
    match(executable, cli::Rule::Freestyle, 2000, false);
    match(executable, cli::Rule::ExactFive, 2000, true);
    const auto crOnly = variant(executable, directory, "cr-only");
    match(crOnly, cli::Rule::Freestyle, 2000, false);
    const auto noisy = variant(executable, directory, "noise");
    {
        cli::EngineConfig config;
        cli::EngineWorker worker({"Noisy", noisy}, config);
        await(worker.ready());
        require(await(worker.request(std::nullopt)).move.x == 0,
                "Stderr draining and debug skipping.");
    }
    for (const auto& mode :
         {"bad-start", "long-line", "startup-timeout", "move-timeout", "crash", "invalid"})
    {
        const auto path = variant(executable, directory, mode);
        cli::EngineConfig config;
        config.startupMilliseconds = std::string(mode) == "startup-timeout" ? 150 : 2000;
        config.turnMilliseconds = 150;
        cli::EngineWorker worker({mode, path}, config);
        auto ready = worker.ready();
        if (std::string(mode) == "bad-start" || std::string(mode) == "long-line" ||
            std::string(mode) == "startup-timeout")
        {
            rejects([&] { await(std::move(ready)); });
        }
        else
        {
            await(std::move(ready));
            rejects([&] { await(worker.request(std::nullopt)); },
                    std::string(mode) == "crash" ? "mock diagnostic" : "");
        }
    }
    const auto hanging = variant(executable, directory, "cancel-move-timeout");
    const auto begin = std::chrono::steady_clock::now();
    {
        cli::EngineWorker worker({"Cancel", hanging}, {});
        await(worker.ready());
        auto future = worker.request(std::nullopt);
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
        worker.stop();
        rejects([&] { await(std::move(future)); }, "cancel");
        rejects([&] { worker.request(std::nullopt); });
    }
    require(std::chrono::steady_clock::now() - begin < std::chrono::seconds(2),
            "Cancellation and process termination must finish within two seconds.");
    const auto hangingStart = variant(executable, directory, "cancel-startup-timeout");
    {
        cli::EngineWorker worker({"Cancel startup", hangingStart}, {});
        auto future = worker.ready();
        worker.stop();
        rejects([&] { await(std::move(future)); });
    }
}

cli::GameRecord decided(std::optional<std::size_t> winner)
{
    cli::GameRecord record;
    record.winner = winner;
    record.end = winner ? cli::GameEnd::Five : cli::GameEnd::BoardFull;
    record.moves = {{{0, 0}, 0, 4.0}, {{1, 0}, 1, 2.0}, {{2, 0}, 0, 10.0}};
    return record;
}

std::string readFile(const std::filesystem::path& file)
{
    std::ifstream input(file, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

void debugTests(const std::filesystem::path& directory, const std::filesystem::path& executable)
{
    const std::string metrics =
        R"({"score":null,"event":"search_progress","nodes_total":18446744073709551615,)"
        R"("tt_hits_total":7,"nps":8,"depth_current":9,"elapsed_ms":10,"best_move":null})";
    const auto parsed = cli::parseMetrics(metrics, 15);
    require(parsed.nodes == UINT64_MAX && !parsed.best && parsed.score == "null",
            "Metrics retain 64-bit counters, nulls and reordered keys.");
    require(cli::metricsStatus(parsed).find("18446744073709551615") != std::string::npos,
            "Metrics display must not round counters.");
    for (const auto& text :
         std::vector<std::string>{"{}", "null", "{\"event\":", metrics + "x",
                                  "{\"event\":\"search_progress\",\"nodes_total\":-1}"})
    {
        rejects([&] { cli::parseMetrics(text, 15); });
    }
    for (const auto& substitution : {"-1", "1.5", "18446744073709551616", "\"12\""})
    {
        auto invalid = metrics;
        invalid.replace(invalid.find("18446744073709551615"), 20, substitution);
        rejects([&] { cli::parseMetrics(invalid, 15); });
    }
    auto invalidMove = metrics;
    invalidMove.replace(invalidMove.find("\"best_move\":null"), 16,
                        "\"best_move\":{\"x\":15,\"y\":0}");
    rejects([&] { cli::parseMetrics(invalidMove, 15); });
    for (const auto& invalid : {"{\"a\":1,\"a\":2}", "[1,]", "01", "\"\\uD800\""})
    {
        rejects([&] { cli::parseJson(invalid); });
    }
    require(cli::parseJson(cli::jsonQuote("quote\"\\\r\n\t")).text == "quote\"\\\r\n\t",
            "Debug JSON escaping round-trip.");
    require(cli::parseJson("\"\\u0061\\u00e4\\ud83d\\ude00\"").text == u8"a\u00e4\U0001f600",
            "JSON Unicode escapes.");

    for (const auto& mode :
         {"metrics", "metrics-once", "metrics-debug-enabled", "debug-rejected", "debug-silent",
          "debug-delayed", "metrics-malformed", "metrics-absent"})
    {
        auto session = std::make_shared<cli::DebugSession>(directory / "debug-reports");
        const auto path = variant(executable, directory, mode);
        cli::EngineConfig config;
        config.size = 5;
        config.turnMilliseconds = 2000;
        config.debug = session;
        {
            cli::EngineWorker black({"Debug \"black\"", path}, config);
            config.debugEngine = 1;
            cli::EngineWorker white({"Debug white", path}, config);
            await(black.ready());
            await(white.ready());
            auto first = black.request(std::nullopt);
            auto second = white.request(std::nullopt);
            const bool supported = std::string(mode) == "metrics" ||
                                   std::string(mode) == "metrics-once" ||
                                   std::string(mode) == "metrics-debug-enabled";
            bool sawFirst = false;
            bool sawSecond = false;
            bool sawNulls = false;
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(4);
            while (first.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready ||
                   second.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready)
            {
                require(std::chrono::steady_clock::now() < deadline, "Debug request hung.");
                if (const auto current = session->metrics(0))
                {
                    sawFirst = sawFirst || current->nodes == 1000;
                    sawSecond = sawSecond || current->nodes == 2000;
                    sawNulls = sawNulls || (!current->best && current->score == "null");
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
            require(await(std::move(first)).move.x == 0 && await(std::move(second)).move.x == 0,
                    "Debug negotiation must not contaminate move replies.");
            const auto last = session->metrics(0);
            require(supported ? sawFirst && sawSecond && sawNulls && last && last->nodes == 3000 &&
                                    last->best && last->score == "-42"
                              : !last && !session->metrics(1),
                    "Live metrics updates or unsupported/malformed fallback failed.");
            if (supported)
            {
                auto next = black.request(cli::Move{1, 0});
                while (next.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
                }
                require(await(std::move(next)).move.x == 2, "Debug subsequent TURN.");
                if (std::string(mode) == "metrics-once")
                {
                    require(!session->metrics(0),
                            "A new search must clear the previous search's metrics.");
                }
            }
        }
        session->finish();
        const auto document = cli::parseJson(readFile(session->file()));
        const auto& events = document.members.at("events").elements;
        std::array<std::string, 2> commands;
        std::array<std::string, 2> errors;
        std::array<std::string, 2> replies;
        bool malformedLogged = false;
        for (const auto& event : events)
        {
            require(event.members.count("timestamp") && event.members.count("elapsed_ms") &&
                        event.members.count("name"),
                    "Missing debug event metadata.");
            const auto index =
                static_cast<std::size_t>(std::stoul(event.members.at("engine").text));
            const auto& stream = event.members.at("stream").text;
            const auto& text = event.members.at("text").text;
            if (stream == "stdin")
            {
                commands.at(index) += text;
            }
            if (stream == "stdout")
            {
                replies.at(index) += text;
            }
            if (stream == "stderr")
            {
                errors.at(index) += text;
            }
            if (stream == "metrics_error")
            {
                malformedLogged = true;
            }
        }
        for (std::size_t index = 0; index < 2; ++index)
        {
            require(commands[index].find("START 5\r\n") != std::string::npos &&
                        commands[index].find("DEBUG_OUTPUT\r\n") != std::string::npos &&
                        commands[index].find("BEGIN\r\n") != std::string::npos &&
                        commands[index].find("END\r\n") != std::string::npos &&
                        replies[index].find("0,0") != std::string::npos &&
                        errors[index].find("mock shutdown complete") != std::string::npos,
                    "Full debug transcript must include both engines and shutdown output.");
        }
        require(std::string(mode) != "metrics-malformed" || malformedLogged,
                "Malformed metrics must be logged explicitly.");
    }
    {
        cli::DebugSession empty(directory / "debug-reports");
        empty.finish();
        require(cli::parseJson(readFile(empty.file())).members.at("events").elements.empty(),
                "Empty/cancelled debug report must be valid JSON.");
    }
    for (const auto& mode : {"debug-crash", "debug-move-timeout", "debug-startup-timeout"})
    {
        auto session = std::make_shared<cli::DebugSession>(directory / "debug-reports");
        cli::EngineConfig config;
        config.debug = session;
        config.turnMilliseconds = 100;
        config.startupMilliseconds = std::string(mode) == "debug-startup-timeout" ? 100 : 2000;
        {
            cli::EngineWorker worker({mode, variant(executable, directory, mode)}, config);
            if (std::string(mode) == "debug-startup-timeout")
            {
                rejects([&] { await(worker.ready()); });
            }
            else
            {
                await(worker.ready());
                rejects([&] { await(worker.request(std::nullopt)); });
            }
        }
        session->finish();
        require(!cli::parseJson(readFile(session->file())).members.at("events").elements.empty(),
                "Engine failure must retain a valid JSON transcript.");
    }
    {
        auto session = std::make_shared<cli::DebugSession>(directory / "debug-reports");
        cli::EngineConfig config;
        config.debug = session;
        {
            cli::EngineWorker worker(
                {"Late acknowledgement", variant(executable, directory, "metrics-debug-delayed")},
                config);
            await(worker.ready());
            require(await(worker.request(std::nullopt)).move.x == 0 && session->metrics(0) &&
                        session->metrics(0)->nodes == 3000,
                    "Late debug acknowledgement must enable live metrics.");
        }
        session->finish();
    }
    const auto blocked = directory / "blocked-debug";
    {
        std::ofstream output(blocked);
        output << "not a directory";
    }
    rejects([&] { cli::DebugSession session(blocked); });
}

void reportTests()
{
    std::vector<cli::GameRecord> games;
    for (int i = 0; i < 10; ++i)
    {
        games.push_back(decided(i < 6   ? std::optional<std::size_t>(0)
                                : i < 9 ? std::optional<std::size_t>(1)
                                        : std::nullopt));
        games.back().black = static_cast<std::size_t>(i % 2);
    }
    const auto summary = cli::summarize(games);
    require(summary.games == 10 && summary.draws == 1 && summary.engines[0].wins == 6 &&
                summary.engines[0].losses == 3 && summary.engines[1].score == 3.5,
            "Result aggregation.");
    require(summary.elo && *summary.elo > 107.0 && *summary.elo < 108.0, "Elo estimate.");
    require(summary.eloLower && summary.eloUpper && *summary.eloLower < *summary.elo &&
                *summary.elo < *summary.eloUpper,
            "Elo confidence interval.");
    require(summary.engines[0].asBlack.games + summary.engines[0].asWhite.games == 10,
            "Color split.");
    require(summary.engines[0].moveTime.moves == 20 &&
                summary.engines[0].moveTime.maximum == 10.0 &&
                summary.engines[0].moveTime.median == 7.0,
            "Move time statistics.");
    require(summary.averageLength == 3.0, "Game length.");
    cli::TestSettings settings;
    settings.engines = {cli::EngineEntry{"Quote \" and \n \xff", "a"}, cli::EngineEntry{"B", "b"}};
    cli::TestOutcome outcome;
    outcome.games = {decided(0)};
    const auto json = cli::reportJson(settings, outcome);
    require(json.find("\"Quote \\\" and \\n \\ufffd\"") != std::string::npos, "JSON escaping.");
    require(json.find("\"difference\": null") != std::string::npos, "Undefined Elo is null.");
    require(json.find("\"abortReason\": null") != std::string::npos, "Missing abort reason.");
}

void openingTests()
{
    const auto first = cli::generateOpenings(15, 6, 20, 42);
    const auto second = cli::generateOpenings(15, 6, 20, 42);
    require(first.size() == 20, "Opening count.");
    bool different = false;
    for (std::size_t i = 0; i < first.size(); ++i)
    {
        require(first[i].size() == 6, "Opening length.");
        for (std::size_t j = 0; j < first[i].size(); ++j)
        {
            const auto move = first[i][j];
            require(move.x == second[i][j].x && move.y == second[i][j].y, "Opening determinism.");
            require(move.x >= 4 && move.x <= 10 && move.y >= 4 && move.y <= 10, "Opening area.");
            for (std::size_t k = 0; k < j; ++k)
            {
                require(move.x != first[i][k].x || move.y != first[i][k].y, "Opening overlap.");
            }
            different = different || move.x != first[0][j].x || move.y != first[0][j].y;
        }
    }
    require(different, "Openings must vary.");
    require(cli::generateOpenings(5, 8, 3, 1)[2].size() == 8, "Small-board openings.");
    rejects([] { cli::generateOpenings(15, 9, 1, 1); });
}

cli::TestSettings testSettings(const std::filesystem::path& directory,
                               const std::filesystem::path& first,
                               const std::filesystem::path& second, int games, int stones)
{
    cli::TestSettings settings;
    settings.games = games;
    settings.size = 7;
    settings.turnMilliseconds = 1000;
    settings.openingStones = stones;
    settings.seed = 7;
    settings.startupMilliseconds = 2000;
    settings.engines = {cli::EngineEntry{"First", first}, cli::EngineEntry{"Second", second}};
    settings.output = directory / "results" / ("run-" + std::to_string(stones) + ".json");
    return settings;
}

void runnerTests(const std::filesystem::path& directory, const std::filesystem::path& executable)
{
    for (const int stones : {0, 1, 3})
    {
        const auto settings = testSettings(directory, executable, executable, 4, stones);
        int polls = 0;
        const auto outcome =
            cli::runTest(settings, [&](const cli::TestProgress&) { return ++polls < 0; });
        require(outcome.completed && outcome.games.size() == 4 && polls > 0, "Test run.");
        for (std::size_t i = 0; i < 4; ++i)
        {
            const auto& game = outcome.games[i];
            require(game.black == i % 2 && game.end != cli::GameEnd::Forfeit,
                    "Alternating colors without forfeits.");
            require(game.opening.size() == static_cast<std::size_t>(stones), "Opening applied.");
            cli::Board board(settings.size, settings.rule);
            cli::Stone turn = cli::Stone::Black;
            for (const auto move : game.opening)
            {
                board.play(move, turn);
                turn = cli::opposite(turn);
            }
            for (const auto& move : game.moves)
            {
                require(move.engine == (turn == cli::Stone::Black ? game.black : 1 - game.black),
                        "Engine turn order.");
                board.play(move.move, turn);
                turn = cli::opposite(turn);
            }
            const auto last = game.moves.back().move;
            require(game.winner ? board.wins(last) : board.full(), "Recorded game result.");
        }
        for (std::size_t j = 0; j < static_cast<std::size_t>(stones); ++j)
        {
            require(outcome.games[0].opening[j].x == outcome.games[1].opening[j].x &&
                        outcome.games[2].opening[j].y == outcome.games[3].opening[j].y,
                    "Openings are repeated with swapped colors.");
        }
        const auto json = readFile(settings.output);
        require(json.find("\"completed\": true") != std::string::npos &&
                    json.find("\"gamesPlayed\": 4") != std::string::npos,
                "Test report file.");
    }
    const std::vector<std::pair<std::string, cli::EngineFailure>> failures{
        {"invalid", cli::EngineFailure::IllegalMove},
        {"occupied", cli::EngineFailure::IllegalMove},
        {"move-timeout", cli::EngineFailure::Timeout},
        {"crash", cli::EngineFailure::Crash},
        {"move-error", cli::EngineFailure::Protocol}};
    for (const auto& failure : failures)
    {
        auto settings = testSettings(directory, executable,
                                     variant(executable, directory, "test-" + failure.first), 1, 0);
        settings.turnMilliseconds = 150;
        const auto outcome = cli::runTest(settings, {});
        require(outcome.completed && outcome.games.size() == 1, "Forfeit run completes.");
        const auto& game = outcome.games[0];
        require(game.end == cli::GameEnd::Forfeit && game.winner == std::size_t(0) &&
                    game.forfeitCause == failure.second,
                "Forfeit classification for " + failure.first + ": " +
                    cli::failureName(game.forfeitCause));
    }
    auto broken =
        testSettings(directory, executable, variant(executable, directory, "test-bad-start"), 2, 0);
    const auto failed = cli::runTest(broken, {});
    require(!failed.completed && failed.games.empty() && !failed.abortReason.empty(),
            "Startup failure aborts the test.");
    require(readFile(broken.output).find("\"completed\": false") != std::string::npos,
            "Aborted report file.");
    auto cancelled = testSettings(directory, executable, executable, 3, 0);
    cancelled.output = directory / "results" / "cancelled.json";
    const auto stopped = cli::runTest(cancelled, [](const cli::TestProgress& progress)
                                      { return progress.game == 2; });
    require(!stopped.completed && stopped.games.size() == 1 &&
                stopped.abortReason == cli::TestAborted().what(),
            "User cancellation keeps finished games.");
}
} // namespace

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::cerr << "Usage: cli_tests <mock-engine> [real-engine]\n";
        return 2;
    }
    const auto directory =
        std::filesystem::temp_directory_path() /
        ("moku-cli-tests-" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    try
    {
        std::filesystem::create_directories(directory);
        const auto executable =
            variant(std::filesystem::absolute(argv[1]), directory, "normal with spaces");
        boardTests();
        std::cout << "PASS board rules and coordinates" << std::endl;
        storeTests(directory, executable);
        std::cout << "PASS engine registry CRUD and persistence" << std::endl;
        protocolTests(directory, executable);
        std::cout << "PASS protocol, workers, failures, deadlines and cancellation" << std::endl;
        debugTests(directory, executable);
        std::cout << "PASS debug metrics, fallback and full JSON transcripts" << std::endl;
        reportTests();
        openingTests();
        std::cout << "PASS test statistics, JSON and openings" << std::endl;
        runnerTests(directory, executable);
        std::cout << "PASS test runner, alternation, forfeits and aborts" << std::endl;
        if (argc > 2)
        {
            match(std::filesystem::absolute(argv[2]), cli::Rule::Freestyle, 2000, false);
            auto settings = testSettings(directory, std::filesystem::absolute(argv[2]),
                                         std::filesystem::absolute(argv[2]), 2, 2);
            settings.size = 15;
            settings.turnMilliseconds = 1000;
            settings.startupMilliseconds = 10000;
            const auto outcome = cli::runTest(settings, {});
            require(outcome.completed && outcome.games.size() == 2, "Real-engine test run.");
            for (const auto& game : outcome.games)
            {
                require(game.end != cli::GameEnd::Forfeit ||
                            game.forfeitCause == cli::EngineFailure::Timeout,
                        "Real engine forfeited: " + game.forfeitMessage);
            }
            std::cout << "PASS real-engine match and test run" << std::endl;
        }
        std::filesystem::remove_all(directory);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL: " << error.what() << "\nTest files: " << directory.string() << '\n';
        return 1;
    }
}
