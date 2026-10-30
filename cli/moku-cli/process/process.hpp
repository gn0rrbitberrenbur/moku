#pragma once

#include <atomic>
#include <chrono>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>

namespace cli
{
class Process
{
public:
    using Observer = std::function<void(const std::string&, const std::string&)>;
    using Clock = std::chrono::steady_clock;
    explicit Process(const std::filesystem::path& executable, Observer observer = {});
    ~Process();
    Process(const Process&) = delete;
    Process& operator=(const Process&) = delete;
    void write(const std::string& command, Clock::time_point deadline,
               const std::atomic<bool>& cancelled);
    std::string readLine(Clock::time_point deadline, const std::atomic<bool>& cancelled);
    std::string diagnostics() const;
    bool poll();
    void finish();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
