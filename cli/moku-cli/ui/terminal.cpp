#include "ui/terminal.hpp"

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <stdexcept>

#ifdef _WIN32
#include <windows.h>
#else
#include <cerrno>
#include <poll.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#endif

namespace cli
{
/**
 * Sanitizes terminal text by removing control characters and converting unsafe whitespace to
 * spaces.
 *
 * @param text raw text to prepare for display
 * @return safe text that can be shown in the UI without corrupting the terminal output
 */
std::string safeText(const std::string& text)
{
    std::string result;
    for (const unsigned char ch : text)
    {
        if (ch >= 32 && ch != 127)
        {
            result += static_cast<char>(ch);
        }
        else if (ch == '\n' || ch == '\r' || ch == '\t')
        {
            result += ' ';
        }
    }
    return result;
}

/**
 * Clips text to a maximum visual width while preserving valid UTF-8 character boundaries.
 *
 * @param text text to truncate
 * @param limit maximum allowed character count before trimming
 * @return shortened text that fits the limit
 */
std::string clippedText(const std::string& text, std::size_t limit)
{
    const auto safe = safeText(text);
    if (safe.size() <= limit)
    {
        return safe;
    }
    while (limit > 0 && (static_cast<unsigned char>(safe[limit]) & 0xc0) == 0x80)
    {
        --limit;
    }
    return safe.substr(0, limit);
}

/**
 * Creates the common header banner that is displayed above the UI screens.
 *
 * @param title short caption shown below the MOKU banner
 * @return ANSI-formatted header string
 */
std::string heading(const std::string& title)
{
    return "\x1b[1;36m\n  M O K U\x1b[0m  /  " + safeText(title) +
           "\n  ----------------------------------------------------------\n\n";
}

struct Terminal::Impl
{
    std::string frame;
#ifdef _WIN32
    HANDLE input = GetStdHandle(STD_INPUT_HANDLE);
    HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD inputMode = 0;
    DWORD outputMode = 0;
    UINT outputCodePage = 0;
    INPUT_RECORD repeated{};
    WORD repeats = 0;
#else
    termios original{};
#endif
};

/**
 * Initializes the terminal in alternate-screen mode and enables raw keyboard handling.
 */
Terminal::Terminal() : impl_(std::make_unique<Impl>())
{
#ifdef _WIN32
    if (!GetConsoleMode(impl_->input, &impl_->inputMode) ||
        !GetConsoleMode(impl_->output, &impl_->outputMode))
    {
        throw std::runtime_error("Run moku_cli in an interactive terminal.");
    }
    impl_->outputCodePage = GetConsoleOutputCP();
    if (!SetConsoleMode(impl_->output, impl_->outputMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING))
    {
        throw std::runtime_error(
            "This console does not support ANSI output. Use Windows Terminal.");
    }
    if (!SetConsoleMode(impl_->input,
                        impl_->inputMode & ~(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT |
                                             ENABLE_PROCESSED_INPUT | ENABLE_QUICK_EDIT_MODE)))
    {
        SetConsoleMode(impl_->output, impl_->outputMode);
        throw std::runtime_error("Cannot enable terminal keyboard input.");
    }
    if (!SetConsoleOutputCP(CP_UTF8))
    {
        SetConsoleMode(impl_->input, impl_->inputMode);
        SetConsoleMode(impl_->output, impl_->outputMode);
        throw std::runtime_error("Cannot enable UTF-8 terminal output.");
    }
#else
    if (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO) ||
        tcgetattr(STDIN_FILENO, &impl_->original) < 0)
    {
        throw std::runtime_error("Run moku_cli in an interactive terminal.");
    }
    termios raw = impl_->original;
    raw.c_lflag &= static_cast<tcflag_t>(~(ECHO | ICANON | ISIG));
    raw.c_iflag &= static_cast<tcflag_t>(~(IXON | ICRNL));
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) < 0)
    {
        throw std::runtime_error("Cannot enable terminal keyboard input.");
    }
#endif
    std::cout << "\x1b[?1049h\x1b[?25l" << std::flush;
}

/**
 * Restores the original terminal state and leaves the alternate screen cleanly.
 */
Terminal::~Terminal()
{
    std::cout << "\x1b[0m\x1b[?25h\x1b[?1049l" << std::flush;
#ifdef _WIN32
    SetConsoleMode(impl_->input, impl_->inputMode);
    SetConsoleMode(impl_->output, impl_->outputMode);
    SetConsoleOutputCP(impl_->outputCodePage);
#else
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &impl_->original);
#endif
}

/**
 * Waits for keyboard input and converts terminal events into the project's normalized key types.
 *
 * @param timeout maximum time to wait before returning no input
 * @return mapped key event, or an empty key when the timeout expires
 */
Key Terminal::read(std::chrono::milliseconds timeout)
{
#ifdef _WIN32
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (true)
    {
        INPUT_RECORD event{};
        if (impl_->repeats > 0)
        {
            event = impl_->repeated;
            --impl_->repeats;
        }
        else
        {
            const auto remaining =
                std::max<std::int64_t>(0, std::chrono::duration_cast<std::chrono::milliseconds>(
                                              deadline - std::chrono::steady_clock::now())
                                              .count());
            const auto status = WaitForSingleObject(impl_->input, static_cast<DWORD>(remaining));
            if (status == WAIT_TIMEOUT)
            {
                return {};
            }
            if (status != WAIT_OBJECT_0)
            {
                throw std::runtime_error("Cannot wait for keyboard input.");
            }
            DWORD count = 0;
            if (!ReadConsoleInputW(impl_->input, &event, 1, &count))
            {
                throw std::runtime_error("Cannot read keyboard input.");
            }
            if (event.EventType == KEY_EVENT && event.Event.KeyEvent.bKeyDown &&
                event.Event.KeyEvent.wRepeatCount > 1)
            {
                impl_->repeated = event;
                impl_->repeats = event.Event.KeyEvent.wRepeatCount - 1;
            }
        }
        if (event.EventType != KEY_EVENT || !event.Event.KeyEvent.bKeyDown)
        {
            continue;
        }
        const auto& key = event.Event.KeyEvent;
        switch (key.wVirtualKeyCode)
        {
        case VK_UP:
            return {KeyType::Up, {}};
        case VK_DOWN:
            return {KeyType::Down, {}};
        case VK_LEFT:
            return {KeyType::Left, {}};
        case VK_RIGHT:
            return {KeyType::Right, {}};
        case VK_RETURN:
            return {KeyType::Enter, {}};
        case VK_ESCAPE:
            return {KeyType::Escape, {}};
        case VK_BACK:
            return {KeyType::Backspace, {}};
        default:
            break;
        }
        const wchar_t ch = key.uChar.UnicodeChar;
        if (ch == 3 || ch == 4)
        {
            return {KeyType::End, {}};
        }
        if (ch >= 32)
        {
            char utf8[4]{};
            const int length = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, &ch, 1, utf8,
                                                   sizeof(utf8), nullptr, nullptr);
            if (length > 0)
            {
                return {KeyType::Character, std::string(utf8, static_cast<std::size_t>(length))};
            }
        }
    }
#else
    const auto byte = [](int wait) -> int
    {
        pollfd descriptor{STDIN_FILENO, POLLIN, 0};
        int status = 0;
        do
        {
            status = poll(&descriptor, 1, wait);
        } while (status < 0 && errno == EINTR);
        if (status < 0)
        {
            throw std::runtime_error("Cannot wait for keyboard input.");
        }
        if (status == 0)
        {
            return -1;
        }
        unsigned char ch = 0;
        const auto count = ::read(STDIN_FILENO, &ch, 1);
        if (count < 0)
        {
            throw std::runtime_error("Cannot read keyboard input.");
        }
        return count == 0 ? -2 : ch;
    };
    const int ch = byte(static_cast<int>(timeout.count()));
    switch (ch)
    {
    case -1:
        return {};
    case -2:
    case 3:
    case 4:
        return {KeyType::End, {}};
    case '\r':
    case '\n':
        return {KeyType::Enter, {}};
    case 8:
    case 127:
        return {KeyType::Backspace, {}};
    case 27:
    {
        const int prefix = byte(30);
        if (prefix == '[' || prefix == 'O')
        {
            const int code = byte(30);
            switch (code)
            {
            case 'A':
                return {KeyType::Up, {}};
            case 'B':
                return {KeyType::Down, {}};
            case 'C':
                return {KeyType::Right, {}};
            case 'D':
                return {KeyType::Left, {}};
            default:
                return {};
            }
        }
        return {KeyType::Escape, {}};
    }
    default:
        if (ch >= 32)
        {
            return {KeyType::Character, std::string(1, static_cast<char>(ch))};
        }
        return {};
    }
#endif
}

/**
 * Clears and redraws the terminal only when the screen content actually changed.
 *
 * @param content fully rendered text to display
 */
void Terminal::draw(const std::string& content) const
{
    if (impl_->frame == content)
    {
        return;
    }
    impl_->frame = content;
    std::cout << "\x1b[H\x1b[2J" << content << "\x1b[0m" << std::flush;
}

/**
 * Renders a selectable menu and returns the chosen index while handling navigation keys.
 *
 * @param title heading shown above the menu entries
 * @param items available choices shown in the menu
 * @param detail optional additional help text beneath the title
 * @param selected currently highlighted item
 * @return selected index or std::nullopt if the user cancels
 */
std::optional<std::size_t> Terminal::menu(const std::string& title,
                                          const std::vector<std::string>& items,
                                          const std::string& detail, std::size_t selected)
{
    if (items.empty())
    {
        throw std::invalid_argument("A menu needs at least one item.");
    }
    selected = std::min(selected, items.size() - 1);
    while (true)
    {
        std::string screen = heading(title);
        if (!detail.empty())
        {
            screen += "  " + clippedText(detail, 60) + "\n\n";
        }
        const std::size_t first = selected > 7 ? selected - 7 : 0;
        const std::size_t last = std::min(items.size(), first + 12);
        if (first > 0)
        {
            screen += "      ...\n";
        }
        for (std::size_t i = first; i < last; ++i)
        {
            screen += i == selected ? "\x1b[1;30;46m  > " : "    ";
            screen += clippedText(items[i], 58) + "\x1b[0m\n";
        }
        if (last < items.size())
        {
            screen += "      ...\n";
        }
        screen += "\n  Up/Down: select    Enter: confirm    Esc: back\n";
        draw(screen);
        const auto key = read();
        if (key.type == KeyType::Up)
        {
            selected = selected == 0 ? items.size() - 1 : selected - 1;
        }
        else if (key.type == KeyType::Down)
        {
            selected = (selected + 1) % items.size();
        }
        else if (key.type == KeyType::Enter)
        {
            return selected;
        }
        else if (key.type == KeyType::Escape || key.type == KeyType::End)
        {
            return std::nullopt;
        }
    }
}

/**
 * Opens an inline text prompt that supports editing, backspace, and Enter to confirm.
 *
 * @param title dialog header
 * @param label description of the field being edited
 * @param initial pre-filled text content
 * @return edited string or std::nullopt when cancelled
 */
std::optional<std::string> Terminal::prompt(const std::string& title, const std::string& label,
                                            const std::string& initial)
{
    std::string value = initial;
    while (true)
    {
        const auto display = value.size() > 56 ? "... " + value.substr(value.size() - 52) : value;
        draw(heading(title) + "  " + safeText(label) + "\n\n  > " + safeText(display) +
             "\x1b[7m \x1b[0m\n\n  Enter: save    Backspace: delete    Esc: cancel\n");
        const auto key = read();
        if (key.type == KeyType::Enter)
        {
            return value;
        }
        if (key.type == KeyType::Escape || key.type == KeyType::End)
        {
            return std::nullopt;
        }
        if (key.type == KeyType::Backspace && !value.empty())
        {
            do
            {
                const auto ch = static_cast<unsigned char>(value.back());
                value.pop_back();
                if ((ch & 0xc0) != 0x80)
                {
                    break;
                }
            } while (!value.empty());
        }
        if (key.type == KeyType::Character && value.size() + key.text.size() <= 4096)
        {
            value += key.text;
        }
    }
}

/**
 * Displays a modal message box and waits until the user acknowledges it.
 *
 * @param title short title shown in the dialog header
 * @param text message body, which is wrapped and truncated when necessary
 */
void Terminal::message(const std::string& title, const std::string& text)
{
    const auto safe = clippedText(text, 720);
    std::string body;
    std::size_t offset = 0;
    while (offset < safe.size())
    {
        const auto line = clippedText(safe.substr(offset), 60);
        body += "  " + line + "\n";
        offset += line.size();
    }
    if (safeText(text).size() > safe.size())
    {
        body += "  ... (diagnostic truncated)\n";
    }
    draw(heading(clippedText(title, 45)) + body + "\n  Press Enter or Esc to continue.\n");
    while (true)
    {
        const auto key = read();
        if (key.type == KeyType::Enter || key.type == KeyType::Escape || key.type == KeyType::End)
        {
            return;
        }
    }
}

/**
 * Shows a scrollable text page with an Enter/Esc acknowledgment flow.
 *
 * @param title header for the page
 * @param lines lines of content to display one after another
 */
void Terminal::page(const std::string& title, const std::vector<std::string>& lines)
{
    std::string body;
    for (const auto& line : lines)
    {
        body += "  " + clippedText(line, 76) + "\n";
    }
    draw(heading(clippedText(title, 45)) + body + "\n  Press Enter or Esc to continue.\n");
    while (true)
    {
        const auto key = read();
        if (key.type == KeyType::Enter || key.type == KeyType::Escape || key.type == KeyType::End)
        {
            return;
        }
    }
}

/**
 * Checks whether the terminal is large enough to render a UI with the requested dimensions.
 *
 * @param columns minimum number of columns needed
 * @param rows minimum number of rows needed
 * @return true if the terminal meets the requested size requirements
 */
bool Terminal::fits(int columns, int rows) const
{
#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO info{};
    if (!GetConsoleScreenBufferInfo(impl_->output, &info))
    {
        throw std::runtime_error("Cannot determine terminal dimensions.");
    }
    return info.srWindow.Right - info.srWindow.Left + 1 >= columns &&
           info.srWindow.Bottom - info.srWindow.Top + 1 >= rows;
#else
    winsize size{};
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &size) < 0)
    {
        throw std::runtime_error("Cannot determine terminal dimensions.");
    }
    return size.ws_col >= columns && size.ws_row >= rows;
#endif
}
} // namespace cli
