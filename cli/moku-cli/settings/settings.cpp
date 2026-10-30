#include "settings/settings.hpp"

#include "file_handling/file_io.hpp"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

#ifndef _WIN32
#include <unistd.h>
#endif

namespace cli
{
namespace
{
/** Checks if a string contains any control characters.
 * @param text The string to check.
 * @return True if the string contains control characters, false otherwise.
 */
bool hasControl(const std::string& text)
{
    return std::any_of(text.begin(), text.end(),
                       [](unsigned char ch) { return ch < 32 || ch == 127; });
}
} // namespace

/**
 * Constructs an EngineStore object by loading engine entries from the specified file.
 * @param file The path to the engine registry file.
 * @throws std::runtime_error if the file cannot be read or contains invalid entries.
 */
EngineStore::EngineStore(std::filesystem::path file) : file_(std::move(file))
{
    if (!std::filesystem::exists(file_))
    {
        return;
    }
    std::ifstream input(file_);
    std::string line;
    if (!std::getline(input, line) || line != "MOKU_ENGINES 1")
    {
        throw std::runtime_error("Invalid engine registry: " + file_.string());
    }
    while (std::getline(input, line))
    {
        std::istringstream row(line);
        std::string name;
        std::string path;
        if (!(row >> std::quoted(name) >> std::quoted(path)))
        {
            throw std::runtime_error("Malformed entry in engine registry: " + file_.string());
        }
        row >> std::ws;
        if (!row.eof() || name.empty() || path.empty() || hasControl(name) || hasControl(path))
        {
            throw std::runtime_error("Invalid entry in engine registry: " + file_.string());
        }
        EngineEntry entry{name, std::filesystem::u8path(path)};
        validateUnique(entry, entries_.size());
        entries_.push_back(std::move(entry));
    }
    if (input.bad())
    {
        throw std::runtime_error("Cannot read engine registry: " + file_.string());
    }
}

/**
 * Returns a constant reference to the vector of engine entries.
 * @return A constant reference to the vector of engine entries.
 */
const std::vector<EngineEntry>& EngineStore::entries() const
{
    return entries_;
}

/**
 * Returns a constant reference to the path of the engine registry file.
 * @return A constant reference to the path of the engine registry file.
 */
const std::filesystem::path& EngineStore::file() const
{
    return file_;
}

/**
 * Validates an engine entry.
 * @param entry The engine entry to validate.
 * @throws std::invalid_argument if the entry is invalid.
 */
void EngineStore::validate(const EngineEntry& entry)
{
    if (entry.name.empty() || entry.name.find_first_not_of(' ') == std::string::npos ||
        hasControl(entry.name) || entry.name.size() > 80)
    {
        throw std::invalid_argument("Use an engine name of 1-80 characters without control codes.");
    }
    if (entry.executable.empty() || hasControl(entry.executable.u8string()) ||
        !std::filesystem::is_regular_file(entry.executable))
    {
        throw std::invalid_argument("The engine path must point to an existing executable file.");
    }
#ifndef _WIN32
    if (access(entry.executable.c_str(), X_OK) != 0)
    {
        throw std::invalid_argument("The engine file does not have execute permission.");
    }
#endif
}

/**
 * Validates that an engine entry has a unique name in the store, ignoring a specific index.
 * @param entry The engine entry to validate.
 * @param ignored The index of the entry to ignore during validation.
 * @throws std::invalid_argument if an entry with the same name already exists.
 */
void EngineStore::validateUnique(const EngineEntry& entry, std::size_t ignored) const
{
    for (std::size_t i = 0; i < entries_.size(); ++i)
    {
        if (i != ignored && entries_[i].name == entry.name)
        {
            throw std::invalid_argument("An engine with that name already exists.");
        }
    }
}

/**
 * Adds a new engine entry to the store and saves the updated list to the file.
 * @param entry The engine entry to add.
 * @throws std::invalid_argument if the entry is invalid.
 * @throws std::runtime_error if the file cannot be written.
 */
void EngineStore::add(EngineEntry entry)
{
    validate(entry);
    validateUnique(entry, entries_.size());
    entry.executable = std::filesystem::absolute(entry.executable).lexically_normal();
    auto next = entries_;
    next.push_back(std::move(entry));
    save(next);
    entries_ = std::move(next);
}

/**
 * Edits an existing engine entry in the store at the specified index and saves the updated list to
 * the file.
 * @param index The index of the engine entry to edit.
 * @param entry The new engine entry to replace the existing one.
 * @throws std::out_of_range if the index is invalid.
 */
void EngineStore::edit(std::size_t index, EngineEntry entry)
{
    if (index >= entries_.size())
    {
        throw std::out_of_range("Unknown engine.");
    }
    validate(entry);
    validateUnique(entry, index);
    entry.executable = std::filesystem::absolute(entry.executable).lexically_normal();
    auto next = entries_;
    next[index] = std::move(entry);
    save(next);
    entries_ = std::move(next);
}

/**
 * Removes an engine entry from the store at the specified index and saves the updated list to the
 * file.
 * @param index The index of the engine entry to remove.
 * @throws std::out_of_range if the index is invalid.
 */
void EngineStore::remove(std::size_t index)
{
    if (index >= entries_.size())
    {
        throw std::out_of_range("Unknown engine.");
    }
    auto next = entries_;
    next.erase(next.begin() + static_cast<std::ptrdiff_t>(index));
    save(next);
    entries_ = std::move(next);
}

/**
 * Saves the current list of engine entries to the engine registry file atomically.
 * @param entries The vector of engine entries to save.
 */
void EngineStore::save(const std::vector<EngineEntry>& entries) const
{
    std::ostringstream output;
    output << "MOKU_ENGINES 1\n";
    for (const auto& entry : entries)
    {
        output << std::quoted(entry.name) << ' ' << std::quoted(entry.executable.u8string())
               << '\n';
    }
    writeFileAtomically(file_, output.str(), "engine registry");
}

std::filesystem::path defaultSettingsPath()
{
#ifdef _WIN32
    if (const wchar_t* directory = _wgetenv(L"APPDATA"))
    {
        if (*directory)
        {
            return std::filesystem::path(directory) / "moku-cli" / "engines.cfg";
        }
    }
#else
    if (const char* directory = std::getenv("XDG_CONFIG_HOME"))
    {
        if (*directory)
        {
            return std::filesystem::u8path(directory) / "moku-cli" / "engines.cfg";
        }
    }
    if (const char* directory = std::getenv("HOME"))
    {
        return std::filesystem::u8path(directory) / ".config" / "moku-cli" / "engines.cfg";
    }
#endif
    throw std::runtime_error("No configuration directory found. Use --config <path>.");
}
} // namespace cli
