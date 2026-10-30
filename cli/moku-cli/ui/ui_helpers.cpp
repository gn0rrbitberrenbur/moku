#include "ui/ui_helpers.hpp"

#include <sstream>
#include <stdexcept>
#include <vector>

namespace cli
{
/**
 * Parses a whole number from user input and ensures it stays within the allowed range.
 *
 * @param text user-provided numeric text
 * @param minimum smallest accepted value
 * @param maximum largest accepted value
 * @return parsed integer
 * @throws std::invalid_argument when the value is missing, malformed, or out of range
 */
int parseNumber(const std::string& text, int minimum, int maximum)
{
    std::istringstream input(text);
    long long number = 0;
    if (!(input >> number))
    {
        throw std::invalid_argument("Enter a whole number.");
    }
    input >> std::ws;
    if (!input.eof() || number < minimum || number > maximum)
    {
        throw std::invalid_argument("Value must be between " + std::to_string(minimum) + " and " +
                                    std::to_string(maximum) + ".");
    }
    return static_cast<int>(number);
}

/**
 * Returns the display name for a selected engine or a placeholder when no engine has been chosen.
 *
 * @param entry engine selection, if any
 * @return selected engine name or a default prompt string
 */
std::string engineName(const std::optional<EngineEntry>& entry)
{
    return entry ? entry->name : "<select engine>";
}

/**
 * Lists all registered engines and lets the user choose one from the menu.
 *
 * @param terminal terminal used to render the selection screen
 * @param store registry containing all available engines
 * @param title heading displayed above the engine list
 * @return selected engine index or std::nullopt when cancelled
 */
std::optional<std::size_t> selectEngine(Terminal& terminal, const EngineStore& store,
                                        const std::string& title)
{
    if (store.entries().empty())
    {
        terminal.message("No engines", "Add an engine in Options first.");
        return std::nullopt;
    }
    std::vector<std::string> names;
    for (const auto& entry : store.entries())
    {
        names.push_back(entry.name + "  |  " + entry.executable.u8string());
    }
    return terminal.menu(title, names);
}
} // namespace cli
