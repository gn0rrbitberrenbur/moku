#include <chrono>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

int main(int argc, char** argv)
{
    if (argc < 1)
    {
        return 2;
    }
    const std::string executable = argv[0];
    const auto separator = executable.find_last_of("\\/");
    const std::string mode =
        separator == std::string::npos ? executable : executable.substr(separator + 1);
    int size = 0;
    int rule = -1;
    int type = -1;
    int time = -1;
    bool timeLeft = false;
    bool debug = mode.find("debug-enabled") != std::string::npos;
    int searches = 0;
    std::vector<int> board;
    std::string line;
    const std::string newline = mode.find("cr-only") != std::string::npos ? "\r" : "\r\n";
    const auto reply = [&](const std::string& text) { std::cout << text << newline << std::flush; };
    while (std::getline(std::cin, line))
    {
        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }
        std::istringstream input(line);
        std::string command;
        input >> command;
        if (command == "END")
        {
            std::cerr << "mock shutdown complete\n" << std::flush;
            return 0;
        }
        if (command == "START")
        {
            input >> size;
            board.assign(static_cast<std::size_t>(size * size), 0);
            if (mode.find("startup-timeout") != std::string::npos)
            {
                std::this_thread::sleep_for(std::chrono::seconds(60));
            }
            if (mode.find("bad-start") != std::string::npos)
            {
                reply("UNKNOWN START");
                continue;
            }
            if (mode.find("long-line") != std::string::npos)
            {
                reply(std::string(20000, 'A'));
                continue;
            }
            reply("");
            reply("DEBUG preparing board");
            reply("MESSAGE ready");
            reply("OK - mock ready");
        }
        else if (command == "DEBUG_OUTPUT")
        {
            if (mode.find("debug-delayed") != std::string::npos)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(300));
            }
            if (mode.find("debug-silent") != std::string::npos)
            {
                continue;
            }
            if (mode.find("metrics") == std::string::npos)
            {
                reply(mode.find("debug-delayed") == std::string::npos
                          ? "UNKNOWN DEBUG_OUTPUT"
                          : "ERROR unsupported command: DEBUG_OUTPUT");
                continue;
            }
            debug = !debug;
            reply(debug ? "DEBUG_OUTPUT True" : "DEBUG_OUTPUT False");
        }
        else if (command == "INFO")
        {
            std::string key;
            int value = 0;
            input >> key >> value;
            if (key == "rule")
            {
                rule = value;
            }
            if (key == "game_type")
            {
                type = value;
            }
            if (key == "timeout_turn")
            {
                time = value;
            }
            if (key == "time_left")
            {
                timeLeft = value == 2147483647;
            }
        }
        else if (command == "BEGIN" || command == "TURN" || command == "BOARD")
        {
            if (command == "BOARD")
            {
                board.assign(static_cast<std::size_t>(size * size), 0);
                bool valid = true;
                while (std::getline(std::cin, line))
                {
                    if (!line.empty() && line.back() == '\r')
                    {
                        line.pop_back();
                    }
                    if (line == "DONE")
                    {
                        break;
                    }
                    int x = -1;
                    int y = -1;
                    int field = 0;
                    char first = 0;
                    char second = 0;
                    std::istringstream stone(line);
                    stone >> x >> first >> y >> second >> field;
                    if (first != ',' || second != ',' || x < 0 || y < 0 || x >= size || y >= size ||
                        field < 1 || field > 2 ||
                        board[static_cast<std::size_t>(y * size + x)] != 0)
                    {
                        valid = false;
                        continue;
                    }
                    board[static_cast<std::size_t>(y * size + x)] = field;
                }
                if (!valid)
                {
                    reply("ERROR invalid BOARD");
                    continue;
                }
            }
            if (rule < 0 || rule > 1 || type < 0 || type > 1 || time < 1 || !timeLeft)
            {
                reply("ERROR missing or invalid INFO");
                continue;
            }
            timeLeft = false;
            ++searches;
            if (debug && mode.find("metrics") != std::string::npos)
            {
                if (mode.find("malformed") != std::string::npos)
                {
                    std::cerr << "[METRICS] {broken}\n" << std::flush;
                    std::this_thread::sleep_for(std::chrono::milliseconds(150));
                }
                else if (mode.find("absent") == std::string::npos &&
                         (mode.find("once") == std::string::npos || searches == 1))
                {
                    for (int step = 1; step <= 3; ++step)
                    {
                        const std::string json =
                            "[METRICS] {\"event\":\"search_progress\",\"elapsed_ms\":" +
                            std::to_string(step * 150) +
                            ",\"nodes_total\":" + std::to_string(step * 1000) +
                            ",\"tt_hits_total\":" + std::to_string(step * 100) +
                            ",\"nps\":6666,\"depth_current\":" + std::to_string(step) +
                            ",\"best_move\":" + (step == 1 ? "null" : "{\"x\":0,\"y\":0}") +
                            ",\"score\":" + (step == 1 ? "null" : "-42") + "}\n";
                        std::cerr << json.substr(0, 27) << std::flush;
                        std::this_thread::sleep_for(std::chrono::milliseconds(5));
                        std::cerr << json.substr(27) << std::flush;
                        std::this_thread::sleep_for(std::chrono::milliseconds(150));
                    }
                }
            }
            if (mode.find("move-timeout") != std::string::npos)
            {
                std::this_thread::sleep_for(std::chrono::seconds(60));
            }
            if (mode.find("crash") != std::string::npos)
            {
                std::cerr << "mock diagnostic: unexpected exit\n" << std::flush;
                return 3;
            }
            if (mode.find("invalid") != std::string::npos)
            {
                reply("99,-1");
                continue;
            }
            if (mode.find("noise") != std::string::npos)
            {
                for (int i = 0; i < 200; ++i)
                {
                    reply("DEBUG still thinking");
                    std::cerr << std::string(1024, 'x') << '\n' << std::flush;
                }
            }
            if (command == "TURN")
            {
                int x = -1;
                int y = -1;
                char comma = 0;
                input >> x >> comma >> y;
                if (comma != ',' || x < 0 || y < 0 || x >= size || y >= size ||
                    board[static_cast<std::size_t>(y * size + x)] != 0)
                {
                    reply("ERROR inconsistent TURN state");
                    continue;
                }
                board[static_cast<std::size_t>(y * size + x)] = 2;
                if (mode.find("occupied") != std::string::npos)
                {
                    reply(std::to_string(x) + "," + std::to_string(y));
                    continue;
                }
            }
            if (mode.find("move-error") != std::string::npos)
            {
                reply("ERROR mock refuses to move");
                continue;
            }
            bool played = false;
            for (int i = 0; i < size * size; ++i)
            {
                if (board[static_cast<std::size_t>(i)] == 0)
                {
                    board[static_cast<std::size_t>(i)] = 1;
                    reply(std::to_string(i % size) + "," + std::to_string(i / size));
                    played = true;
                    break;
                }
            }
            if (!played)
            {
                reply("ERROR board full");
            }
        }
        else
        {
            reply("UNKNOWN " + command);
        }
    }
}
