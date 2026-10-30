#pragma once

#include "board/board.hpp"

#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <optional>
#include <string>

namespace cli
{
struct SearchMetrics
{
    std::uint64_t elapsed = 0;
    std::uint64_t nodes = 0;
    std::uint64_t hits = 0;
    std::uint64_t nps = 0;
    std::uint64_t depth = 0;
    std::optional<Move> best;
    std::string score = "null";
};

SearchMetrics parseMetrics(const std::string& json, int size);
std::string metricsStatus(const SearchMetrics& metrics);

class DebugSession
{
public:
    explicit DebugSession(const std::filesystem::path& directory);
    void record(std::size_t engine, const std::string& name, const std::string& stream,
                const std::string& text);
    void reset(std::size_t engine);
    void metrics(std::size_t engine, const SearchMetrics& metrics);
    std::optional<SearchMetrics> metrics(std::size_t engine) const;
    void finish();
    const std::filesystem::path& file() const
    {
        return file_;
    }

private:
    std::filesystem::path file_;
    std::ofstream output_;
    std::chrono::steady_clock::time_point started_ = std::chrono::steady_clock::now();
    mutable std::mutex mutex_;
    std::array<std::optional<SearchMetrics>, 2> metrics_;
    bool first_ = true;
    std::string failure_;
    void check();
};
} // namespace cli
