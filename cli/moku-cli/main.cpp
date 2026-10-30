#include "ui/application.hpp"

#include <filesystem>
#include <iostream>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>

namespace
{
template <typename Character> int run(int argc, Character** argv)
{
    try
    {
        std::filesystem::path configuration;
        for (int i = 1; i < argc; ++i)
        {
            const std::filesystem::path argument = argv[i];
            if (argument == "--help")
            {
                std::cout << "moku_cli - interactive Gomoku manager\n\n"
                             "Usage: moku_cli [--config <registry-path>]\n"
                             "       moku_cli --help\n\n"
                             "Run in an interactive ANSI terminal. Engines are external\n";
                return 0;
            }
            if (argument != "--config" || i + 1 >= argc || !configuration.empty())
            {
                throw std::invalid_argument("Invalid arguments. Use --help for usage.");
            }
            configuration = argv[++i];
        }
        cli::EngineStore store(configuration.empty() ? cli::defaultSettingsPath() : configuration);
        cli::Terminal terminal;
        cli::Application application(terminal, store);
        application.run();
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "moku_cli: " << error.what() << '\n';
        return 1;
    }
}
} // namespace

#ifdef _WIN32
int wmain(int argc, wchar_t** argv)
{
    return run(argc, argv);
}
#else
int main(int argc, char** argv)
{
    return run(argc, argv);
}
#endif