#include "tests/test_mode.hpp"

#include "ui/ui_helpers.hpp"

#include <array>
#include <cctype>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <random>
#include <sstream>
#include <stdexcept>

namespace cli
{
namespace
{
std::filesystem::path defaultOutput()
{
    const auto now = std::time(nullptr);
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    std::ostringstream name;
    name << "test-" << std::put_time(&local, "%Y%m%d-%H%M%S") << ".json";
    return std::filesystem::current_path() / "results" / name.str();
}

std::uint32_t parseSeed(const std::string& text)
{
    std::istringstream input(text);
    input >> std::ws;
    unsigned long long seed = 0;
    if (!std::isdigit(input.peek()) || !(input >> seed) || seed > 0xffffffffULL)
    {
        throw std::invalid_argument("Seed must be a whole number between 0 and 4294967295.");
    }
    input >> std::ws;
    if (!input.eof())
    {
        throw std::invalid_argument("Seed must be a whole number between 0 and 4294967295.");
    }
    return static_cast<std::uint32_t>(seed);
}

std::string ruleName(Rule rule)
{
    return rule == Rule::Freestyle ? "freestyle" : "exact five";
}

std::string fixed(double value, int precision)
{
    std::ostringstream output;
    output << std::fixed << std::setprecision(precision) << value;
    return output.str();
}

std::string signedFixed(double value)
{
    return (value >= 0 ? "+" : "") + fixed(value, 1);
}

std::string pad(const std::string& text, std::size_t width)
{
    const auto clipped = clippedText(text, width - 1);
    return clipped + std::string(width - clipped.size(), ' ');
}

std::string label(std::size_t engine)
{
    return engine == 0 ? "A" : "B";
}

std::string describe(const GameRecord& game)
{
    std::string text = "Game " + std::to_string(game.index) + ": ";
    if (!game.winner)
    {
        return text + "draw (board full)";
    }
    text += label(*game.winner) + " wins";
    if (game.end == GameEnd::Forfeit)
    {
        return text + " (" + label(1 - *game.winner) +
               " forfeit: " + failureName(game.forfeitCause) + ")";
    }
    return text + " (five in a row)";
}

std::string progressScreen(const TestSettings& settings, const TestProgress& progress,
                           std::chrono::steady_clock::time_point started)
{
    std::array<int, 2> wins{};
    int draws = 0;
    for (const auto& game : progress.finished)
    {
        if (game.winner)
        {
            ++wins[*game.winner];
        }
        else
        {
            ++draws;
        }
    }
    const int done = static_cast<int>(progress.finished.size());
    const int width = 30;
    const int filled = done * width / settings.games;
    const auto elapsed =
        std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - started)
            .count();
    std::ostringstream screen;
    screen << heading("Test mode") << "  A: " << clippedText(settings.engines[0].name, 60)
           << "\n  B: " << clippedText(settings.engines[1].name, 60) << "\n\n  Game "
           << progress.game << " of " << settings.games << "  [" << std::string(filled, '#')
           << std::string(width - filled, '-') << "]  " << done * 100 / settings.games
           << "%\n  Now: " << label(progress.black) << " as Black vs " << label(1 - progress.black)
           << " as White, stones on board: " << progress.stones << "\n  Score: A "
           << fixed(wins[0] + 0.5 * draws, 1) << " - " << fixed(wins[1] + 0.5 * draws, 1)
           << " B   (wins " << wins[0] << "/" << wins[1] << ", draws " << draws << ")\n  Last: "
           << (progress.finished.empty() ? "-" : describe(progress.finished.back()))
           << "\n  Elapsed: " << elapsed << " s\n\n  Esc: abort (finished games are saved)\n";
    return screen.str();
}
} // namespace

TestMode::TestMode(Terminal& terminal, const EngineStore& store)
    : terminal_(terminal), store_(store)
{
}

void TestMode::run()
{
    if (const auto settings = setup())
    {
        execute(*settings);
    }
}

std::optional<TestSettings> TestMode::setup()
{
    if (store_.entries().empty())
    {
        terminal_.message("No engines", "Add an engine in Options first.");
        return std::nullopt;
    }
    TestSettings config = defaults_;
    config.seed = std::random_device{}();
    config.output = defaultOutput();
    std::array<std::optional<EngineEntry>, 2> engines;
    std::size_t selection = 0;
    while (true)
    {
        const std::vector<std::string> items{
            "Games: " + std::to_string(config.games),
            "Board size: " + std::to_string(config.size),
            std::string("Rule: ") +
                (config.rule == Rule::Freestyle ? "Freestyle (5 or more)" : "Standard (exactly 5)"),
            "Engine time per move: " + std::to_string(config.turnMilliseconds) + " ms",
            "Random opening stones: " + std::to_string(config.openingStones),
            "Seed: " + std::to_string(config.seed),
            "Engine A: " + engineName(engines[0]),
            "Engine B: " + engineName(engines[1]),
            "Result file: " + config.output.u8string(),
            "Start test",
            "Back"};
        const auto selected =
            terminal_.menu("Test setup", items,
                           "Engines alternate Black; each opening is played twice.", selection);
        if (!selected || *selected == 10)
        {
            return std::nullopt;
        }
        selection = *selected;
        try
        {
            if (*selected == 0 || *selected == 1 || *selected == 3 || *selected == 4)
            {
                struct Field
                {
                    const char* label;
                    int minimum;
                    int maximum;
                    int* value;
                };
                const std::array<Field, 5> fields{{
                    {"Number of games (1-10000)", 1, 10000, &config.games},
                    {"Board size (5-19)", 5, 19, &config.size},
                    {"", 0, 0, nullptr},
                    {"Engine time per move (1-600000 ms)", 1, 600000, &config.turnMilliseconds},
                    {"Random opening stones (0-8)", 0, 8, &config.openingStones},
                }};
                const auto& field = fields[*selected];
                const auto value =
                    terminal_.prompt("Test setup", field.label, std::to_string(*field.value));
                if (value)
                {
                    *field.value = parseNumber(*value, field.minimum, field.maximum);
                }
            }
            else if (*selected == 2)
            {
                const auto rule =
                    terminal_.menu("Rules", {"Freestyle: 5 or more", "Standard: exactly 5"});
                if (rule)
                {
                    config.rule = *rule == 0 ? Rule::Freestyle : Rule::ExactFive;
                }
            }
            else if (*selected == 5)
            {
                const auto value = terminal_.prompt("Test setup", "Random seed (0-4294967295)",
                                                    std::to_string(config.seed));
                if (value)
                {
                    config.seed = parseSeed(*value);
                }
            }
            else if (*selected == 6 || *selected == 7)
            {
                if (const auto engine = selectEngine(terminal_, store_, "Select engine"))
                {
                    engines[*selected - 6] = store_.entries()[*engine];
                }
            }
            else if (*selected == 8)
            {
                const auto value =
                    terminal_.prompt("Test setup", "Result file (JSON)", config.output.u8string());
                if (value)
                {
                    if (value->find_first_not_of(' ') == std::string::npos)
                    {
                        throw std::invalid_argument("Enter a result file path.");
                    }
                    config.output = std::filesystem::absolute(std::filesystem::u8path(*value));
                }
            }
            else if (*selected == 9)
            {
                if (!engines[0] || !engines[1])
                {
                    terminal_.message("Select engines", "Choose engine A and engine B first.");
                    continue;
                }
                for (const auto& entry : engines)
                {
                    EngineStore::validate(*entry);
                }
                if (std::filesystem::is_directory(config.output))
                {
                    throw std::invalid_argument("The result file path is a directory.");
                }
                if (std::filesystem::exists(config.output))
                {
                    const auto confirm = terminal_.menu(
                        "Result file exists", {"Cancel", "Overwrite"}, config.output.u8string());
                    if (!confirm || *confirm == 0)
                    {
                        continue;
                    }
                }
                defaults_.games = config.games;
                defaults_.size = config.size;
                defaults_.rule = config.rule;
                defaults_.turnMilliseconds = config.turnMilliseconds;
                defaults_.openingStones = config.openingStones;
                config.engines = {*engines[0], *engines[1]};
                return config;
            }
        }
        catch (const std::exception& error)
        {
            terminal_.message("Invalid setting", error.what());
        }
    }
}

void TestMode::execute(const TestSettings& settings)
{
    const auto started = std::chrono::steady_clock::now();
    const auto poll = [&](const TestProgress& progress)
    {
        terminal_.draw(progressScreen(settings, progress, started));
        const auto key = terminal_.read(std::chrono::milliseconds(0));
        return key.type == KeyType::Escape || key.type == KeyType::End;
    };
    const auto outcome = runTest(settings, poll);
    if (!outcome.completed && outcome.abortReason != TestAborted().what())
    {
        terminal_.message("Test aborted", outcome.abortReason);
    }
    report(settings, outcome);
}

void TestMode::report(const TestSettings& settings, const TestOutcome& outcome)
{
    const auto summary = summarize(outcome.games);
    const auto row =
        [](const std::string& name, const std::string& first, const std::string& second)
    { return pad(name, 22) + pad(first, 27) + second; };
    std::vector<std::string> lines;
    lines.push_back((outcome.completed ? "Completed: " : "Aborted after ") +
                    std::to_string(summary.games) + " of " + std::to_string(settings.games) +
                    " games");
    lines.push_back("Board " + std::to_string(settings.size) + "x" + std::to_string(settings.size) +
                    ", " + ruleName(settings.rule) + ", " +
                    std::to_string(settings.turnMilliseconds) + " ms/move, " +
                    std::to_string(settings.openingStones) + " opening stones, seed " +
                    std::to_string(settings.seed));
    lines.emplace_back();
    lines.push_back(row("", "A: " + settings.engines[0].name, "B: " + settings.engines[1].name));
    std::array<std::array<std::string, 2>, 8> cells;
    for (std::size_t engine = 0; engine < 2; ++engine)
    {
        const auto& stats = summary.engines[engine];
        const auto colors = [](const ColorStats& color)
        {
            return std::to_string(color.wins) + "/" + std::to_string(color.losses) + "/" +
                   std::to_string(color.draws);
        };
        cells[0][engine] = std::to_string(stats.wins);
        cells[1][engine] = std::to_string(stats.losses);
        cells[2][engine] = std::to_string(stats.draws);
        cells[3][engine] =
            fixed(stats.score, 1) +
            (summary.games ? " (" + fixed(100.0 * stats.score / summary.games, 1) + "%)" : "");
        cells[4][engine] = colors(stats.asBlack);
        cells[5][engine] = colors(stats.asWhite);
        cells[6][engine] =
            std::to_string(stats.forfeits[0]) + "/" + std::to_string(stats.forfeits[1]) + "/" +
            std::to_string(stats.forfeits[2]) + "/" + std::to_string(stats.forfeits[3]);
        cells[7][engine] = fixed(stats.moveTime.average, 1) + " / " +
                           fixed(stats.moveTime.median, 1) + " / " +
                           fixed(stats.moveTime.maximum, 1);
    }
    const std::array<const char*, 8> names{"Wins",
                                           "Losses",
                                           "Draws",
                                           "Score",
                                           "As Black W/L/D",
                                           "As White W/L/D",
                                           "Forfeits T/I/C/P",
                                           "Move ms avg/med/max"};
    for (std::size_t i = 0; i < names.size(); ++i)
    {
        lines.push_back(row(names[i], cells[i][0], cells[i][1]));
    }
    lines.emplace_back();
    if (summary.games > 0)
    {
        lines.push_back("Game length (stones): avg " + fixed(summary.averageLength, 1) + ", min " +
                        std::to_string(summary.minimumLength) + ", max " +
                        std::to_string(summary.maximumLength));
        if (summary.elo)
        {
            lines.push_back("Elo A vs B: " + signedFixed(*summary.elo) + "  (95% CI " +
                            (summary.eloLower ? signedFixed(*summary.eloLower) : "-inf") + " to " +
                            (summary.eloUpper ? signedFixed(*summary.eloUpper) : "+inf") + ")");
        }
        else
        {
            lines.push_back("Elo A vs B: n/a (one engine scored 0% or 100%)");
        }
    }
    lines.push_back("Results: " + settings.output.u8string());
    terminal_.page(outcome.completed ? "Test complete" : "Test aborted", lines);
}
} // namespace cli
