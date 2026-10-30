#pragma once

#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace cli
{
enum class KeyType
{
    None,
    Up,
    Down,
    Left,
    Right,
    Enter,
    Escape,
    Backspace,
    Character,
    End
};

struct Key
{
    KeyType type = KeyType::None;
    std::string text;
};

class Terminal
{
public:
    Terminal();
    ~Terminal();
    Terminal(const Terminal&) = delete;
    Terminal& operator=(const Terminal&) = delete;
    Key read(std::chrono::milliseconds timeout = std::chrono::milliseconds(100));
    void draw(const std::string& content) const;
    std::optional<std::size_t> menu(const std::string& title, const std::vector<std::string>& items,
                                    const std::string& detail = "", std::size_t selected = 0);
    std::optional<std::string> prompt(const std::string& title, const std::string& label,
                                      const std::string& initial = "");
    void message(const std::string& title, const std::string& text);
    /**
     * Shows preformatted lines until Enter or Esc is pressed.
     *
     * @param title screen title
     * @param lines lines shown in order; each is clipped to the screen width
     */
    void page(const std::string& title, const std::vector<std::string>& lines);
    bool fits(int columns, int rows) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

std::string safeText(const std::string& text);
std::string clippedText(const std::string& text, std::size_t limit);
std::string heading(const std::string& title);
} // namespace cli
