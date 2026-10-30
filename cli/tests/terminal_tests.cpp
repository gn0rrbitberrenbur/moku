#include "settings/settings.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
#include <windows.h>

namespace
{
std::string readText(const std::filesystem::path& file)
{
    std::ifstream input(file, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

class Session
{
public:
    Session(const std::filesystem::path& executable, const std::filesystem::path& configuration)
    {
        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        startup.dwFlags = STARTF_USESHOWWINDOW;
        startup.wShowWindow = SW_HIDE;
        PROCESS_INFORMATION process{};
        std::wstring command =
            L"\"" + executable.wstring() + L"\" --config \"" + configuration.wstring() + L"\"";
        if (!CreateProcessW(executable.c_str(), &command[0], nullptr, nullptr, FALSE,
                            CREATE_NEW_CONSOLE, nullptr, configuration.parent_path().c_str(),
                            &startup, &process))
        {
            throw std::runtime_error("Cannot launch UI test console.");
        }
        child_ = process.hProcess;
        CloseHandle(process.hThread);
        FreeConsole();
        bool attached = false;
        for (int i = 0; i < 100; ++i)
        {
            if (AttachConsole(process.dwProcessId))
            {
                attached = true;
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        if (!attached)
        {
            TerminateProcess(child_, 1);
            CloseHandle(child_);
            child_ = nullptr;
            throw std::runtime_error("Cannot attach UI test console.");
        }
        input_ =
            CreateFileW(L"CONIN$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                        nullptr, OPEN_EXISTING, 0, nullptr);
        output_ =
            CreateFileW(L"CONOUT$", GENERIC_READ | GENERIC_WRITE,
                        FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
        if (input_ == INVALID_HANDLE_VALUE || output_ == INVALID_HANDLE_VALUE)
        {
            cleanup();
            throw std::runtime_error("Cannot access UI test console.");
        }
        const SMALL_RECT window{0, 0, 99, 44};
        if (!SetConsoleScreenBufferSize(output_, COORD{120, 60}) ||
            !SetConsoleWindowInfo(output_, TRUE, &window))
        {
            cleanup();
            throw std::runtime_error("Cannot resize UI test console.");
        }
    }

    ~Session()
    {
        cleanup();
    }

    void key(WORD code, wchar_t character = 0)
    {
        INPUT_RECORD events[2]{};
        events[0].EventType = KEY_EVENT;
        events[0].Event.KeyEvent.bKeyDown = TRUE;
        events[0].Event.KeyEvent.wRepeatCount = 1;
        events[0].Event.KeyEvent.wVirtualKeyCode = code;
        events[0].Event.KeyEvent.uChar.UnicodeChar = character;
        events[1] = events[0];
        events[1].Event.KeyEvent.bKeyDown = FALSE;
        DWORD written = 0;
        if (!WriteConsoleInputW(input_, events, 2, &written) || written != 2)
        {
            throw std::runtime_error("Cannot inject UI test key.");
        }
    }

    void text(const std::wstring& value)
    {
        for (const auto ch : value)
        {
            key(0, ch);
        }
    }

    void down(int count)
    {
        for (int i = 0; i < count; ++i)
        {
            key(VK_DOWN);
        }
    }

    void erase(int count)
    {
        for (int i = 0; i < count; ++i)
        {
            key(VK_BACK, L'\b');
        }
    }

    std::wstring wait(const std::wstring& expected)
    {
        std::cout << "UI: waiting for " << std::string(expected.begin(), expected.end()) << '\n';
        std::wstring screen;
        for (int i = 0; i < 250; ++i)
        {
            CloseHandle(output_);
            output_ =
                CreateFileW(L"CONOUT$", GENERIC_READ | GENERIC_WRITE,
                            FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
            CONSOLE_SCREEN_BUFFER_INFO info{};
            if (!GetConsoleScreenBufferInfo(output_, &info))
            {
                throw std::runtime_error("Cannot inspect UI test screen.");
            }
            screen.clear();
            for (SHORT row = info.srWindow.Top; row <= info.srWindow.Bottom; ++row)
            {
                std::wstring line(static_cast<std::size_t>(info.dwSize.X), L' ');
                DWORD count = 0;
                if (!ReadConsoleOutputCharacterW(output_, &line[0], static_cast<DWORD>(line.size()),
                                                 COORD{0, row}, &count))
                {
                    throw std::runtime_error("Cannot read UI test screen.");
                }
                screen += line.substr(0, count) + L'\n';
            }
            if (screen.find(expected) != std::wstring::npos)
            {
                return screen;
            }
            if (WaitForSingleObject(child_, 0) != WAIT_TIMEOUT)
            {
                throw std::runtime_error("CLI exited unexpectedly.");
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        std::string excerpt;
        bool space = false;
        for (const auto ch : screen)
        {
            if (ch == L' ' || ch == L'\n')
            {
                if (!space)
                {
                    excerpt += ' ';
                }
                space = true;
            }
            else
            {
                excerpt += static_cast<char>(ch);
                space = false;
            }
        }
        std::string needle(expected.begin(), expected.end());
        throw std::runtime_error("UI screen did not show '" + needle + "': " + excerpt);
    }

    void exitSuccessfully()
    {
        if (WaitForSingleObject(child_, 3000) != WAIT_OBJECT_0)
        {
            throw std::runtime_error("CLI did not exit after Quit.");
        }
        DWORD code = 1;
        if (!GetExitCodeProcess(child_, &code) || code != 0)
        {
            throw std::runtime_error("CLI returned a nonzero exit code.");
        }
    }

private:
    HANDLE child_ = nullptr;
    HANDLE input_ = INVALID_HANDLE_VALUE;
    HANDLE output_ = INVALID_HANDLE_VALUE;

    void cleanup()
    {
        if (child_)
        {
            if (WaitForSingleObject(child_, 0) == WAIT_TIMEOUT)
            {
                TerminateProcess(child_, 1);
                WaitForSingleObject(child_, 1000);
            }
            CloseHandle(child_);
            child_ = nullptr;
        }
        if (input_ != INVALID_HANDLE_VALUE)
        {
            CloseHandle(input_);
            input_ = INVALID_HANDLE_VALUE;
        }
        if (output_ != INVALID_HANDLE_VALUE)
        {
            CloseHandle(output_);
            output_ = INVALID_HANDLE_VALUE;
        }
        FreeConsole();
    }
};
} // namespace

int main(int argc, char** argv)
{
    if (argc != 3)
    {
        std::cerr << "Usage: cli_terminal_tests <cli> <mock-engine>\n";
        return 2;
    }
    const auto directory =
        std::filesystem::temp_directory_path() /
        ("moku-cli-ui-" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    try
    {
        std::filesystem::create_directories(directory);
        const auto configuration = directory / std::filesystem::u8path(u8"engines-\u00e4.cfg");
        const auto mock = std::filesystem::absolute(argv[2]);
        const auto report = directory / "results" / std::filesystem::u8path(u8"test-\u00e4.json");
        {
            Session session(std::filesystem::absolute(argv[1]), configuration);
            session.wait(L"Five stones.");
            session.down(3);
            session.key(VK_RETURN);
            session.wait(L"Add engine");
            session.key(VK_RETURN);
            session.wait(L"Display name");
            session.text(L"Mock UI");
            session.key(VK_RETURN);
            session.wait(L"Executable path");
            session.text(mock.wstring());
            session.key(VK_RETURN);
            session.wait(L"Engine registry updated.");
            session.key(VK_RETURN);
            session.wait(L"Add engine");
            session.key(VK_ESCAPE);
            session.wait(L"Five stones.");
            session.key(VK_RETURN);
            session.wait(L"Board size: 15");
            session.down(4);
            session.key(VK_RETURN);
            session.wait(L"Select engine");
            session.key(VK_RETURN);
            session.wait(L"Opponent: Mock UI");
            session.down(1);
            session.key(VK_RETURN);
            session.wait(L"Your turn");
            session.text(L"H8");
            session.key(VK_RETURN);
            session.wait(L"Moves: 2");
            session.wait(L"Your turn");
            session.key(VK_RIGHT);
            session.key(VK_RETURN);
            session.wait(L"Moves: 4");
            session.key(VK_ESCAPE);
            session.wait(L"Five stones.");
            session.down(1);
            session.key(VK_RETURN);
            session.wait(L"Black engine:");
            session.down(3);
            session.key(VK_RETURN);
            session.wait(L"Select engine");
            session.key(VK_RETURN);
            session.wait(L"Black engine: Mock UI");
            session.down(1);
            session.key(VK_RETURN);
            session.wait(L"Select engine");
            session.key(VK_RETURN);
            session.wait(L"White engine: Mock UI");
            session.down(1);
            session.key(VK_RETURN);
            session.wait(L"wins!");
            session.key(VK_RETURN);
            session.wait(L"Five stones.");
            session.down(2);
            session.key(VK_RETURN);
            session.wait(L"Games: 10");
            session.key(VK_RETURN);
            session.wait(L"Number of games");
            session.erase(5);
            session.text(L"2");
            session.key(VK_RETURN);
            session.wait(L"Games: 2");
            session.down(1);
            session.key(VK_RETURN);
            session.wait(L"Board size (5-19)");
            session.erase(5);
            session.text(L"5");
            session.key(VK_RETURN);
            session.wait(L"Board size: 5");
            session.down(5);
            session.key(VK_RETURN);
            session.wait(L"Select engine");
            session.key(VK_RETURN);
            session.wait(L"Engine A: Mock UI");
            session.down(1);
            session.key(VK_RETURN);
            session.wait(L"Select engine");
            session.key(VK_RETURN);
            session.wait(L"Engine B: Mock UI");
            session.down(1);
            session.key(VK_RETURN);
            session.wait(L"Result file (JSON)");
            session.erase(400);
            session.text(report.wstring());
            session.key(VK_RETURN);
            session.wait(L"Engine B: Mock UI");
            session.down(1);
            session.key(VK_RETURN);
            session.wait(L"Test complete");
            session.wait(L"Elo A vs B");
            session.key(VK_RETURN);
            session.wait(L"Five stones.");
            session.down(3);
            session.key(VK_RETURN);
            session.wait(L"Add engine");
            session.down(2);
            session.key(VK_RETURN);
            session.wait(L"Mock UI");
            session.key(VK_RETURN);
            session.wait(L"Display name");
            session.text(L" edited");
            session.key(VK_RETURN);
            session.wait(L"Executable path");
            session.key(VK_RETURN);
            session.wait(L"Engine registry updated.");
            session.key(VK_RETURN);
            session.wait(L"Add engine");
            session.down(1);
            session.key(VK_RETURN);
            session.wait(L"Mock UI edited");
            session.key(VK_RETURN);
            session.wait(L"Cancel");
            session.down(1);
            session.key(VK_RETURN);
            session.wait(L"Add engine");
            session.key(VK_ESCAPE);
            session.wait(L"Five stones.");
            session.down(5);
            session.key(VK_RETURN);
            session.exitSuccessfully();
        }
        if (!cli::EngineStore(configuration).entries().empty())
        {
            throw std::runtime_error("UI removal was not persisted.");
        }
        const auto json = readText(report);
        if (json.find("\"completed\": true") == std::string::npos ||
            json.find("\"gamesPlayed\": 2") == std::string::npos ||
            json.find("\"boardSize\": 5") == std::string::npos)
        {
            throw std::runtime_error("UI test report is incomplete.");
        }
        const auto debugConfiguration = directory / "debug.cfg";
        const auto metricsEngine = directory / "mock-metrics.exe";
        const auto fallbackEngine = directory / "mock-debug-rejected-move-timeout.exe";
        std::filesystem::copy_file(mock, metricsEngine);
        std::filesystem::copy_file(mock, fallbackEngine);
        {
            cli::EngineStore debugStore(debugConfiguration);
            debugStore.add({"Metrics", metricsEngine});
            debugStore.add({"Fallback", fallbackEngine});
        }
        {
            Session session(std::filesystem::absolute(argv[1]), debugConfiguration);
            session.wait(L"Five stones.");
            session.down(4);
            session.key(VK_RETURN);
            session.wait(L"Live search metrics");
            session.key(VK_RETURN);
            session.wait(L"Your color: Black");
            session.key(VK_RETURN);
            session.wait(L"Board size (5-19)");
            session.erase(5);
            session.text(L"9");
            session.key(VK_RETURN);
            session.wait(L"Board size: 9");
            session.down(3);
            session.key(VK_RETURN);
            session.wait(L"Your color: White");
            session.down(1);
            session.key(VK_RETURN);
            session.wait(L"Select engine");
            session.key(VK_RETURN);
            session.wait(L"Opponent: Metrics");
            session.down(1);
            session.key(VK_RETURN);
            session.wait(L"Nodes: 1000");
            const auto updated = session.wait(L"Nodes: 2000");
            const auto nodes = updated.find(L"Nodes:");
            if (updated.find(L"Nodes: 1000") != std::wstring::npos ||
                updated.find(L"Nodes:", nodes + 1) != std::wstring::npos)
            {
                throw std::runtime_error("Debug metrics must update in place, not accumulate.");
            }
            session.wait(L"Score: -42");
            session.wait(L"Your turn");
            session.key(VK_ESCAPE);
            session.wait(L"Debug report saved");
            session.key(VK_RETURN);
            session.wait(L"Live search metrics");
            session.down(1);
            session.key(VK_RETURN);
            session.wait(L"Black engine:");
            session.down(3);
            session.key(VK_RETURN);
            session.wait(L"Select engine");
            session.down(1);
            session.key(VK_RETURN);
            session.wait(L"Black engine: Fallback");
            session.down(1);
            session.key(VK_RETURN);
            session.wait(L"Select engine");
            session.down(1);
            session.key(VK_RETURN);
            session.wait(L"White engine: Fallback");
            session.down(1);
            session.key(VK_RETURN);
            session.wait(L"Thinking...");
            session.key(VK_ESCAPE);
            session.wait(L"Debug report saved");
            session.key(VK_RETURN);
            session.wait(L"Live search metrics");
            session.key(VK_ESCAPE);
            session.wait(L"Five stones.");
            session.down(5);
            session.key(VK_RETURN);
            session.exitSuccessfully();
        }
        unsigned debugReports = 0;
        for (const auto& entry : std::filesystem::directory_iterator(directory / "debug-reports"))
        {
            const auto transcript = readText(entry.path());
            if (transcript.find("\"stream\":\"stdin\"") == std::string::npos ||
                transcript.find("\"stream\":\"stdout\"") == std::string::npos ||
                transcript.find("DEBUG_OUTPUT") == std::string::npos ||
                transcript.find("END\\r\\n") == std::string::npos)
            {
                throw std::runtime_error("Debug UI transcript is incomplete.");
            }
            ++debugReports;
        }
        if (debugReports != 2)
        {
            throw std::runtime_error("Each debug game must have its own JSON report.");
        }
        std::filesystem::remove_all(directory);
        std::cout << "PASS terminal menus, engine CRUD, human input and engine match\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL: " << error.what() << "\nTest files: " << directory.string() << '\n';
        return 1;
    }
}
