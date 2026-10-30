#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace cli
{
struct EngineEntry
{
    std::string name;
    std::filesystem::path executable;
};

class EngineStore
{
public:
    explicit EngineStore(std::filesystem::path file);
    const std::vector<EngineEntry>& entries() const;
    const std::filesystem::path& file() const;
    void add(EngineEntry entry);
    void edit(std::size_t index, EngineEntry entry);
    void remove(std::size_t index);
    static void validate(const EngineEntry& entry);

private:
    std::filesystem::path file_;
    std::vector<EngineEntry> entries_;
    void save(const std::vector<EngineEntry>& entries) const;
    void validateUnique(const EngineEntry& entry, std::size_t ignored) const;
};

std::filesystem::path defaultSettingsPath();
}
