#include "engine/extended_protocol/extended_protocol.hpp"

#include <emscripten/bind.h>
#include <iostream>
#include <memory>
#include <string>

class WasmEngine : public Engine
{
public:
    WasmEngine()
    {
        g_config.cores = 1;
    }

    void send_command(const std::string& command)
    {
        if (running && !command.empty())
        {
            process_command(command);
            std::cout.flush();
            std::cerr.flush();
        }
    }
};

namespace
{
std::unique_ptr<WasmEngine> engine;
}

/**
 * Initializes board-dependent engine state before processing START.
 */
void start_game(int board_size)
{
    if (board_size < 5 || board_size > 19)
    {
        std::cout << "ERROR unsupported board size " << board_size << std::endl;
        return;
    }

    engine.reset();
    g_config = GameConfig{};
    g_config.board_size = board_size;
    g_config.cores = 1;

    engine = std::make_unique<WasmEngine>();
    engine->send_command("START " + std::to_string(board_size));
}

void send_command(const std::string& command)
{
    if (!engine)
    {
        engine = std::make_unique<WasmEngine>();
    }

    engine->send_command(command);
}

EMSCRIPTEN_BINDINGS(moku_extended)
{
    emscripten::function("start_game", &start_game);
    emscripten::function("send_command", &send_command);
}