#pragma once

#include "settings/settings.hpp"
#include "ui/terminal.hpp"

#include <optional>
#include <string>

namespace cli
{
/**
 * Parses a whole number and validates its range.
 *
 * @param text user input without surrounding content other than whitespace
 * @param minimum smallest accepted value
 * @param maximum largest accepted value
 * @return parsed number
 */
int parseNumber(const std::string& text, int minimum, int maximum);

/**
 * @param entry selected engine, if any
 * @return engine name or a selection placeholder
 */
std::string engineName(const std::optional<EngineEntry>& entry);

/**
 * Lets the user pick a registered engine with arrow keys.
 *
 * @param terminal terminal used for the menu
 * @param store engine registry to list
 * @param title menu title
 * @return registry index, or no value when cancelled or empty
 */
std::optional<std::size_t> selectEngine(Terminal& terminal, const EngineStore& store,
                                        const std::string& title);
} // namespace cli
