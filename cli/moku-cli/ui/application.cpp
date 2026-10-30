#include "ui/application.hpp"

#include "engine/engine.hpp"
#include "ui/ui_helpers.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <future>
#include <iomanip>
#include <limits>
#include <memory>
#include <sstream>
#include <stdexcept>

namespace cli
{
namespace
{
/**
 * Maps a stone color to the matching player slot in the engine array.
 *
 * @param stone color to translate
 * @return index of the corresponding player entry
 */
std::size_t playerIndex(Stone stone)
{
    return stone == Stone::Black ? 0 : 1;
}
} // namespace

/**
 * Creates the main application object and initializes the test-mode helper with the shared
 * terminal.
 *
 * @param terminal interactive UI used by the application
 * @param store persistent registry of available engines
 */
Application::Application(Terminal& terminal, EngineStore& store)
    : terminal_(terminal), store_(store), testMode_(terminal, store)
{
}

/**
 * Runs the top-level main menu and dispatches into game setup, debug mode, test mode, or options.
 */
void Application::run()
{
    while (true)
    {
        const auto selected = terminal_.menu(
            "GOMOKU",
            {"Play vs engine", "Engine vs engine", "Test mode", "Options", "Debug", "Quit"},
            "Five stones. One winning line.");
        if (!selected || *selected == 5)
        {
            return;
        }
        try
        {
            if (*selected == 4)
            {
                debug();
            }
            else if (*selected == 3)
            {
                options();
            }
            else if (*selected == 2)
            {
                testMode_.run();
            }
            else if (const auto config = setup(*selected == 0))
            {
                play(*config);
            }
        }
        catch (const std::exception& error)
        {
            terminal_.message("Error", error.what());
        }
    }
}

/**
 * Displays the debug menu and runs a full debug session for a played match when requested.
 */
void Application::debug()
{
    while (true)
    {
        const auto selected =
            terminal_.menu("Debug", {"Play vs engine", "Engine vs engine", "Back"},
                           "Live search metrics and full JSON engine transcript.");
        if (!selected || *selected == 2)
        {
            return;
        }
        if (const auto config = setup(*selected == 0, true))
        {
            auto session =
                std::make_shared<DebugSession>(std::filesystem::current_path() / "debug-reports");
            try
            {
                play(*config, session);
            }
            catch (const std::exception&)
            {
                session->finish();
                throw;
            }
            session->finish();
            terminal_.message("Debug report saved", session->file().u8string());
        }
    }
}

/**
 * Opens the engine-selection menu using the current application registry.
 *
 * @param title heading displayed in the selection window
 * @return selected engine index or std::nullopt when cancelled
 */
std::optional<std::size_t> Application::selectEngine(const std::string& title)
{
    return cli::selectEngine(terminal_, store_, title);
}

/**
 * Adds a new engine or edits an existing registry entry through terminal prompts.
 *
 * @param index entry to edit; std::nullopt creates a new engine
 */
void Application::editEngine(std::optional<std::size_t> index)
{
    const EngineEntry original = index ? store_.entries().at(*index) : EngineEntry{};
    const auto name = terminal_.prompt("Engine", "Display name", original.name);
    if (!name)
    {
        return;
    }
    const auto path =
        terminal_.prompt("Engine", "Executable path (no arguments or surrounding quotes)",
                         original.executable.u8string());
    if (!path)
    {
        return;
    }
    EngineEntry entry{*name, std::filesystem::u8path(*path)};
    if (index)
    {
        store_.edit(*index, std::move(entry));
    }
    else
    {
        store_.add(std::move(entry));
    }
    terminal_.message("Saved", "Engine registry updated.");
}

/**
 * Lets the user add, edit, remove, and manage engine entries from the registry.
 */
void Application::options()
{
    while (true)
    {
        const auto selected =
            terminal_.menu("Options", {"Add engine", "Remove engine", "Edit engine", "Back"},
                           "Registry: " + store_.file().u8string());
        if (!selected || *selected == 3)
        {
            return;
        }
        try
        {
            if (*selected == 0)
            {
                editEngine(std::nullopt);
            }
            else if (const auto engine =
                         selectEngine(*selected == 1 ? "Remove engine" : "Edit engine"))
            {
                if (*selected == 2)
                {
                    editEngine(engine);
                }
                else
                {
                    const auto confirm = terminal_.menu(
                        "Remove " + store_.entries()[*engine].name + "?", {"Cancel", "Remove"});
                    if (confirm && *confirm == 1)
                    {
                        store_.remove(*engine);
                    }
                }
            }
        }
        catch (const std::exception& error)
        {
            terminal_.message("Cannot update engines", error.what());
        }
    }
}

/**
 * Collects board, rule, timing, and engine choices for a match in an interactive setup menu.
 *
 * @param human true for a human-vs-engine game, false for engine-vs-engine
 * @param debug true to include debug timing settings in the setup screen
 * @return chosen game configuration or std::nullopt when cancelled
 */
std::optional<Application::GameSetup> Application::setup(bool human, bool debug)
{
    if (store_.entries().empty())
    {
        terminal_.message("No engines", "Add an engine in Options first.");
        return std::nullopt;
    }
    GameSetup config = defaults_;
    config.human = human;
    config.engines = {};
    std::size_t selection = 0;
    while (true)
    {
        std::vector<std::string> items{
            "Board size: " + std::to_string(config.size),
            std::string("Rule: ") +
                (config.rule == Rule::Freestyle ? "Freestyle (5 or more)" : "Standard (exactly 5)"),
            "Engine time per move: " + std::to_string(config.turnMilliseconds) + " ms"};
        if (human)
        {
            items.push_back(std::string("Your color: ") +
                            (config.humanColor == Stone::Black ? "Black (first)" : "White"));
            items.push_back("Opponent: " + engineName(config.engines[0]));
        }
        else
        {
            items.push_back("Black engine: " + engineName(config.engines[0]));
            items.push_back("White engine: " + engineName(config.engines[1]));
        }
        const std::size_t debugIntervalIndex = items.size();
        if (debug)
        {
            items.push_back(
                "Debug output interval: " + std::to_string(config.debugOutputMilliseconds) + " ms");
        }

        const std::size_t startIndex = items.size();
        items.push_back("Start game");

        const std::size_t backIndex = items.size();
        items.push_back("Back");
        const auto selected = terminal_.menu(
            "Game setup", items, human ? "Play vs engine" : "Engine vs engine", selection);
        if (!selected || *selected == backIndex)
        {
            return std::nullopt;
        }
        selection = *selected;
        try
        {
            if (*selected == 0 || *selected == 2)
            {
                const bool size = *selected == 0;
                const auto value = terminal_.prompt(
                    "Game setup", size ? "Board size (5-19)" : "Engine time per move (1-600000 ms)",
                    std::to_string(size ? config.size : config.turnMilliseconds));
                if (value)
                {
                    const int number = parseNumber(*value, size ? 5 : 1, size ? 19 : 600000);
                    if (size)
                    {
                        config.size = number;
                    }
                    else
                    {
                        config.turnMilliseconds = number;
                    }
                }
            }
            else if (*selected == 1)
            {
                const auto rule =
                    terminal_.menu("Rules", {"Freestyle: 5 or more", "Standard: exactly 5"});
                if (rule)
                {
                    config.rule = *rule == 0 ? Rule::Freestyle : Rule::ExactFive;
                }
            }
            else if (human && *selected == 3)
            {
                config.humanColor = opposite(config.humanColor);
            }
            else if (*selected == 3 || *selected == 4)
            {
                const std::size_t slot = human ? 0 : *selected - 3;
                if (const auto engine = selectEngine("Select engine"))
                {
                    config.engines[slot] = store_.entries()[*engine];
                }
            }
            else if (debug && *selected == debugIntervalIndex)
            {
                const auto value = terminal_.prompt(
                    "Game setup", "Debug output interval in ms (positive whole number)",
                    std::to_string(config.debugOutputMilliseconds));

                if (value)
                {
                    config.debugOutputMilliseconds =
                        parseNumber(*value, 1, std::numeric_limits<int>::max());
                }
            }
            else if (*selected == startIndex)
            {
                if (!config.engines[0] || (!human && !config.engines[1]))
                {
                    terminal_.message("Select engines", "Choose all engines before starting.");
                    continue;
                }
                for (const auto& entry : config.engines)
                {
                    if (entry)
                    {
                        EngineStore::validate(*entry);
                    }
                }
                defaults_.size = config.size;
                defaults_.rule = config.rule;
                defaults_.turnMilliseconds = config.turnMilliseconds;
                defaults_.humanColor = config.humanColor;
                defaults_.debugOutputMilliseconds = config.debugOutputMilliseconds;
                return config;
            }
        }
        catch (const std::exception& error)
        {
            terminal_.message("Invalid setting", error.what());
        }
    }
}

/**
 * Builds the full ANSI-formatted board screen showing the board, cursor, last move, players,
 * the active turn, and any current status text.
 *
 * @param board current board state
 * @param cursor selected board position
 * @param last most recent move, if any
 * @param players display names for both sides
 * @param turn stone that is currently active
 * @param status text displayed beneath the board
 * @param input current player-entered move text
 * @param debug true to use the debug variant with extra status rows
 * @return rendered screen ready to display in the terminal
 */
std::string Application::boardScreen(const Board& board, Move cursor, std::optional<Move> last,
                                     const std::array<std::string, 2>& players, Stone turn,
                                     const std::string& status, const std::string& input,
                                     bool debug) const
{
    std::ostringstream screen;
    screen << heading(debug ? "DEBUG" : "GOMOKU") << "  \x1b[1;33mX\x1b[0m "
           << clippedText(players[0], 22) << "   vs   \x1b[1;37mO\x1b[0m "
           << clippedText(players[1], 22) << "\n\n     ";
    for (int x = 0; x < board.size(); ++x)
    {
        screen << ' ' << static_cast<char>('A' + x) << ' ';
    }
    screen << '\n';
    for (int y = 0; y < board.size(); ++y)
    {
        screen << "  " << std::setw(2) << y + 1 << ' ';
        for (int x = 0; x < board.size(); ++x)
        {
            const Move move{x, y};
            const bool selected = cursor.x == x && cursor.y == y;
            const bool recent = last && last->x == x && last->y == y;
            const Stone stone = board.at(move);
            if (selected)
            {
                screen << "\x1b[46m";
            }
            else if (recent)
            {
                screen << "\x1b[44m";
            }
            screen << (stone == Stone::Black   ? "\x1b[1;33m"
                       : stone == Stone::White ? "\x1b[1;37m"
                                               : "\x1b[90m");
            screen << ' '
                   << (stone == Stone::Black   ? 'X'
                       : stone == Stone::White ? 'O'
                                               : '.')
                   << " \x1b[0m";
        }
        screen << '\n';
    }
    screen << "\n  Moves: " << board.moves() << " | "
           << (turn == Stone::Black ? "Black (X)" : "White (O)") << '\n';
    std::istringstream lines(status);
    for (int i = 0; i < (debug ? 4 : 1); ++i)
    {
        std::string line;
        std::getline(lines, line);
        screen << "  " << clippedText(line, 60) << '\n';
    }
    screen << "  Arrows + Enter: play | Type H8 + Enter | Esc: leave\n"
           << "  Space: pause engines | Input: " << safeText(input) << '\n';
    return screen.str();
}

/**
 * Runs the actual game loop, handles human and engine turns, and manages pause, terminal resizing,
 * and optional debug metrics while the match is active.
 *
 * @param config game setup used for the match
 * @param debug optional debug session that tracks engine metrics and transcripts
 */
void Application::play(const GameSetup& config, std::shared_ptr<DebugSession> debug)
{
    const int minimumRows = config.size + (debug ? 16 : 13);
    while (!terminal_.fits(65, minimumRows))
    {
        terminal_.draw(heading("Resize terminal") + "  Please enlarge the terminal to 65 x " +
                       std::to_string(minimumRows) + " or more.\n  Esc: back to menu.\n");
        const auto key = terminal_.read();
        if (key.type == KeyType::Escape || key.type == KeyType::End)
        {
            return;
        }
    }
    Board board(config.size, config.rule);
    std::array<std::unique_ptr<EngineWorker>, 2> workers;
    std::array<std::string, 2> players;
    EngineConfig engineConfig;
    engineConfig.size = config.size;
    engineConfig.rule = config.rule;
    engineConfig.turnMilliseconds = config.turnMilliseconds;
    engineConfig.humanOpponent = config.human;
    engineConfig.debug = debug;
    engineConfig.debugOutputMilliseconds = config.debugOutputMilliseconds;
    Move cursor{config.size / 2, config.size / 2};
    std::optional<Move> last;
    Stone turn = Stone::Black;
    std::string status = "Starting engines...";
    std::string input;
    std::string outcome;
    bool paused = false;
    std::optional<std::future<EngineReply>> thinking;
    std::array<std::optional<std::future<void>>, 2> starting;

    for (std::size_t i = 0; i < 2; ++i)
    {
        if (config.human && i == playerIndex(config.humanColor))
        {
            players[i] = "You";
            continue;
        }
        const auto& entry = config.engines[config.human ? 0 : i].value();
        players[i] = entry.name;
        engineConfig.debugEngine = i;
        workers[i] = std::make_unique<EngineWorker>(entry, engineConfig);
        starting[i].emplace(workers[i]->ready());
    }

    while (true)
    {
        const bool boardVisible = terminal_.fits(65, minimumRows);
        if (!boardVisible)
        {
            terminal_.draw(heading("Resize terminal") + "  Please enlarge the terminal to 65 x " +
                           std::to_string(minimumRows) +
                           " or more.\n  Engine deadlines remain active. Esc: leave.\n");
        }
        else
        {
            auto liveStatus = status;
            if (debug && thinking && outcome.empty())
            {
                const auto metrics = debug->metrics(playerIndex(turn));
                liveStatus = metrics ? metricsStatus(*metrics) : "Thinking... (Esc cancels)";
            }
            else if (debug && config.human && !thinking && outcome.empty() &&
                     turn == config.humanColor)
            {
                // Keep the engine's last metrics visible until it starts thinking again.
                const auto metrics = debug->metrics(playerIndex(opposite(turn)));
                if (metrics)
                {
                    liveStatus = metricsStatus(*metrics);
                }
            }
            terminal_.draw(boardScreen(board, cursor, last, players, turn,
                                       paused ? "Paused (Space to resume)" : liveStatus, input,
                                       static_cast<bool>(debug)));
        }
        const auto key = terminal_.read(std::chrono::milliseconds(40));
        if (key.type == KeyType::Escape || key.type == KeyType::End)
        {
            for (auto& worker : workers)
            {
                if (worker)
                {
                    worker->stop();
                }
            }
            return;
        }
        if (!outcome.empty())
        {
            if (key.type == KeyType::Enter)
            {
                return;
            }
            continue;
        }
        bool initialized = true;
        for (std::size_t i = 0; i < 2; ++i)
        {
            if (starting[i])
            {
                if (starting[i]->wait_for(std::chrono::milliseconds(0)) ==
                    std::future_status::ready)
                {
                    try
                    {
                        starting[i]->get();
                        starting[i].reset();
                    }
                    catch (const std::exception& error)
                    {
                        for (auto& worker : workers)
                        {
                            if (worker)
                            {
                                worker->stop();
                            }
                        }
                        terminal_.message("Engine startup failed", error.what());
                        return;
                    }
                }
                else
                {
                    initialized = false;
                }
            }
        }
        if (!initialized)
        {
            continue;
        }
        if (!config.human && key.type == KeyType::Character && key.text == " ")
        {
            paused = !paused;
        }
        const bool humanTurn = config.human && turn == config.humanColor;
        std::optional<Move> move;
        if (humanTurn)
        {
            if (!boardVisible)
            {
                continue;
            }
            status = "Your turn";
            if (key.type == KeyType::Up)
            {
                cursor.y = (cursor.y + config.size - 1) % config.size;
                input.clear();
            }
            else if (key.type == KeyType::Down)
            {
                cursor.y = (cursor.y + 1) % config.size;
                input.clear();
            }
            else if (key.type == KeyType::Left)
            {
                cursor.x = (cursor.x + config.size - 1) % config.size;
                input.clear();
            }
            else if (key.type == KeyType::Right)
            {
                cursor.x = (cursor.x + 1) % config.size;
                input.clear();
            }
            else if (key.type == KeyType::Backspace && !input.empty())
            {
                input.pop_back();
            }
            else if (key.type == KeyType::Character && key.text.size() == 1 && input.size() < 3 &&
                     std::isalnum(static_cast<unsigned char>(key.text[0])))
            {
                input += static_cast<char>(std::toupper(static_cast<unsigned char>(key.text[0])));
            }
            else if (key.type == KeyType::Enter)
            {
                move = input.empty() ? std::optional<Move>(cursor)
                                     : parseCoordinate(input, config.size);
                if (!move || !board.legal(*move))
                {
                    status = "Invalid or occupied square. Use A1-" +
                             coordinate({config.size - 1, config.size - 1});
                    input.clear();
                    terminal_.draw(boardScreen(board, cursor, last, players, turn, status, input,
                                               static_cast<bool>(debug)));
                    terminal_.message("Illegal move", status);
                    continue;
                }
                input.clear();
            }
        }
        else
        {
            const auto index = playerIndex(turn);
            try
            {
                if (!thinking && !paused && boardVisible)
                {
                    if (debug)
                    {
                        debug->reset(index);
                    }
                    thinking.emplace(workers[index]->request(last));
                    status = "Thinking... (Esc cancels)";
                }
                if (thinking &&
                    thinking->wait_for(std::chrono::milliseconds(0)) == std::future_status::ready &&
                    !paused)
                {
                    move = thinking->get().move;
                    thinking.reset();
                    if (!board.legal(*move))
                    {
                        throw std::runtime_error(players[index] + " played an occupied square.");
                    }
                }
            }
            catch (const std::exception& error)
            {
                for (auto& worker : workers)
                {
                    if (worker)
                    {
                        worker->stop();
                    }
                }
                terminal_.message("Engine forfeits", error.what());
                outcome = players[playerIndex(opposite(turn))] + " wins by engine forfeit.";
                status = outcome + " Enter: return.";
                continue;
            }
        }
        if (move)
        {
            board.play(*move, turn);
            last = move;
            cursor = *move;
            if (board.wins(*move))
            {
                outcome = players[playerIndex(turn)] + " wins!";
            }
            else if (board.full())
            {
                outcome = "Draw - board is full.";
            }
            if (!outcome.empty())
            {
                for (auto& worker : workers)
                {
                    if (worker)
                    {
                        worker->stop();
                    }
                }
                status = outcome + " Enter: return.";
            }
            else
            {
                turn = opposite(turn);
                status = config.human && turn == config.humanColor ? "Your turn" : "Engine turn";
            }
        }
    }
}
} // namespace cli
