#pragma once

#include "board/board.hpp"
#include "engine/debug.hpp"
#include "settings/settings.hpp"
#include "tests/test_mode.hpp"
#include "ui/terminal.hpp"

#include <array>
#include <memory>
#include <optional>
#include <string>

namespace cli
{
class Application
{
public:
    /**
     * @param terminal interactive terminal used by all screens
     * @param store persistent engine registry
     */
    Application(Terminal& terminal, EngineStore& store);
    void run();

private:
    struct GameSetup
    {
        int size = 15;
        Rule rule = Rule::Freestyle;
        int turnMilliseconds = 5000;
        bool human = true;
        Stone humanColor = Stone::Black;
        std::array<std::optional<EngineEntry>, 2> engines;
        int debugOutputMilliseconds = 100;
    };

    Terminal& terminal_;
    EngineStore& store_;
    GameSetup defaults_;
    TestMode testMode_;
    void options();
    void debug();
    std::optional<std::size_t> selectEngine(const std::string& title);
    void editEngine(std::optional<std::size_t> index);
    std::optional<GameSetup> setup(bool human, bool debug = false);
    void play(const GameSetup& config, std::shared_ptr<DebugSession> debug = {});
    std::string boardScreen(const Board& board, Move cursor, std::optional<Move> last,
                            const std::array<std::string, 2>& players, Stone turn,
                            const std::string& status, const std::string& input,
                            bool debug = false) const;
};
} // namespace cli
