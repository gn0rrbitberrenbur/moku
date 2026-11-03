#include "engine/extended_protocol/extended_protocol.hpp"

#include <emscripten/bind.h>
#include <iostream>

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

void send_command(const std::string& command)
{
    static WasmEngine engine;
    engine.send_command(command);
}

EMSCRIPTEN_BINDINGS(moku_extended)
{
    emscripten::function("send_command", &send_command);
}