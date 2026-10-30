#include "engine/debug.hpp"

#include "file_handling/json.hpp"
#include "tests/test_report.hpp"

#include <charconv>
#include <cmath>
#include <locale>
#include <sstream>
#include <stdexcept>

namespace cli
{
namespace
{
/**
 * Get a member of a JSON object.
 * @param object The JSON object.
 * @param key The key of the member to get.
 * @return The member value.
 */
const JsonValue& member(const JsonValue& object, const char* key)
{
    const auto found = object.members.find(key);
    if (object.kind != JsonValue::Kind::Object || found == object.members.end())
    {
        throw std::invalid_argument("Missing metrics field: " + std::string(key));
    }
    return found->second;
}

/**
 * Get a nonnegative integer from a JSON object.
 * @param object The JSON object.
 * @param key The key of the member to get.
 * @return The nonnegative integer value.
 * @throws std::invalid_argument if the member is missing or not a nonnegative integer.
 */
std::uint64_t integer(const JsonValue& object, const char* key)
{
    const auto& value = member(object, key);
    std::uint64_t result = 0;
    const auto parsed =
        std::from_chars(value.text.data(), value.text.data() + value.text.size(), result);
    if (value.kind != JsonValue::Kind::Number || parsed.ec != std::errc{} ||
        parsed.ptr != value.text.data() + value.text.size())
    {
        throw std::invalid_argument("Invalid nonnegative integer: " + std::string(key));
    }
    return result;
}
} // namespace

/**
 * Parse search metrics from a JSON string.
 * @param json The JSON string containing the search metrics.
 * @param size The size of the board (used for validation).
 * @return The parsed SearchMetrics object.
 */
SearchMetrics parseMetrics(const std::string& json, int size)
{
    const auto object = parseJson(json);
    const auto& event = member(object, "event");
    if (event.kind != JsonValue::Kind::String || event.text != "search_progress")
    {
        throw std::invalid_argument("Unsupported metrics event.");
    }
    SearchMetrics result;
    result.elapsed = integer(object, "elapsed_ms");
    result.nodes = integer(object, "nodes_total");
    result.hits = integer(object, "tt_hits_total");
    result.nps = integer(object, "nps");
    result.depth = integer(object, "depth_current");
    const auto& best = member(object, "best_move");
    if (best.kind != JsonValue::Kind::Null)
    {
        const auto x = integer(best, "x");
        const auto y = integer(best, "y");
        if (size < 1 || x >= static_cast<std::uint64_t>(size) ||
            y >= static_cast<std::uint64_t>(size))
        {
            throw std::invalid_argument("Metrics best move is outside the board.");
        }
        result.best = Move{static_cast<int>(x), static_cast<int>(y)};
    }
    const auto& score = member(object, "score");
    if (score.kind != JsonValue::Kind::Null)
    {
        std::istringstream input(score.text);
        input.imbue(std::locale::classic());
        double number = 0;
        if (score.kind != JsonValue::Kind::Number || !(input >> number) || !std::isfinite(number))
        {
            throw std::invalid_argument("Invalid metrics score.");
        }
        // Preserve the engine's evaluation without rounding.
        result.score = score.text;
    }
    return result;
}

/**
 * Format search metrics into a human-readable string.
 * @param metrics The SearchMetrics object to format.
 * @return A string representation of the search metrics.
 */
std::string metricsStatus(const SearchMetrics& metrics)
{
    return "Time: " + std::to_string(metrics.elapsed) +
           " ms | Nodes: " + std::to_string(metrics.nodes) +
           "\n  TT hits: " + std::to_string(metrics.hits) +
           " | NPS: " + std::to_string(metrics.nps) +
           "\n  Depth: " + std::to_string(metrics.depth) +
           " | Best: " + (metrics.best ? coordinate(*metrics.best) : "-") +
           "\n  Score: " + (metrics.score == "null" ? "-" : metrics.score);
}

/**
 * Construct a DebugSession object for recording debug information.
 * @param directory The directory where the debug report will be saved.
 */
DebugSession::DebugSession(const std::filesystem::path& directory)
{
    std::filesystem::create_directories(directory);
    const auto base =
        "debug-" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    file_ = directory / (base + ".json");
    for (unsigned suffix = 1; std::filesystem::exists(file_); ++suffix)
    {
        file_ = directory / (base + "-" + std::to_string(suffix) + ".json");
    }
    output_.open(file_, std::ios::binary);
    output_ << "{\"started_at\":" << jsonQuote(utcTimestamp()) << ",\"events\":[";
    output_ << "\n]}\n";
    output_.flush();
    output_.seekp(-4, std::ios::cur);
    check();
}

/**
 * Check if the debug report file is writable and throw an exception if not.
 * @throws std::runtime_error if the debug report file cannot be written to.
 */
void DebugSession::check()
{
    if (!output_)
    {
        failure_ = "Cannot write debug report: " + file_.u8string();
    }
    if (!failure_.empty())
    {
        throw std::runtime_error(failure_);
    }
}

/**
 * Record an event in the debug report.
 * @param engine The engine index (0 or 1).
 * @param name The name of the event.
 * @param stream The stream to which the event belongs.
 * @param text The text of the event.
 */
void DebugSession::record(std::size_t engine, const std::string& name, const std::string& stream,
                          const std::string& text)
{
    std::lock_guard<std::mutex> lock(mutex_);
    check();
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                             std::chrono::steady_clock::now() - started_)
                             .count();
    output_ << (first_ ? "\n" : ",\n") << "{\"timestamp\":" << jsonQuote(utcTimestamp())
            << ",\"elapsed_ms\":" << elapsed << ",\"engine\":" << engine
            << ",\"name\":" << jsonQuote(name) << ",\"stream\":" << jsonQuote(stream)
            << ",\"text\":" << jsonQuote(text) << "}";
    first_ = false;
    const auto end = output_.tellp();
    output_ << "\n]}\n";
    output_.flush();
    output_.seekp(end);
    check();
}

/**
 * Reset the search metrics for a specific engine.
 * @param engine The engine index (0 or 1).
 */
void DebugSession::reset(std::size_t engine)
{
    std::lock_guard<std::mutex> lock(mutex_);
    metrics_.at(engine).reset();
}

/**
 * Set the search metrics for a specific engine.
 * @param engine The engine index (0 or 1).
 * @param metrics The search metrics to set.
 */
void DebugSession::metrics(std::size_t engine, const SearchMetrics& metrics)
{
    std::lock_guard<std::mutex> lock(mutex_);
    metrics_.at(engine) = metrics;
}

/**
 * Get the search metrics for a specific engine.
 * @param engine The engine index (0 or 1).
 * @return The search metrics for the specified engine, or std::nullopt if not found.
 */
std::optional<SearchMetrics> DebugSession::metrics(std::size_t engine) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return metrics_.at(engine);
}

/**
 * Finish the debug session and close the debug report file.
 * @throws std::runtime_error if the debug report file cannot be closed.
 */
void DebugSession::finish()
{
    std::lock_guard<std::mutex> lock(mutex_);
    output_.flush();
    check();
    output_.close();
    if (output_.fail())
    {
        throw std::runtime_error("Cannot close debug report: " + file_.u8string());
    }
}
} // namespace cli
