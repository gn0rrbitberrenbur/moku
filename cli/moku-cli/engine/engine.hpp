#pragma once

#include "board/board.hpp"
#include "engine/debug.hpp"
#include "settings/settings.hpp"

#include <atomic>
#include <condition_variable>
#include <exception>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <thread>
#include <vector>

namespace cli
{
struct EngineConfig
{
    int size = 15;
    Rule rule = Rule::Freestyle;
    int turnMilliseconds = 5000;
    bool humanOpponent = true;
    int startupMilliseconds = 10000;
    std::shared_ptr<DebugSession> debug;
    std::size_t debugEngine = 0;
    int debugOutputMilliseconds = 100;
};

struct EngineReply
{
    Move move;
    double milliseconds;
};

struct PlacedStone
{
    Move move;
    Stone stone;
};

class EngineWorker
{
public:
    /**
     * Starts a dedicated worker thread that launches and initializes the engine process.
     *
     * @param entry registered engine executable
     * @param config board, rule and timing parameters sent to the engine
     */
    EngineWorker(EngineEntry entry, EngineConfig config);
    ~EngineWorker();
    EngineWorker(const EngineWorker&) = delete;
    EngineWorker& operator=(const EngineWorker&) = delete;
    std::future<void> ready();
    /**
     * Requests a move with BEGIN or TURN.
     *
     * @param opponent previous opponent move, or no value for BEGIN on an empty board
     */
    std::future<EngineReply> request(std::optional<Move> opponent);
    /**
     * Requests a move after imposing a complete position with BOARD.
     *
     * @param position stones in the order they were played
     * @param own color played by this engine
     */
    std::future<EngineReply> requestBoard(std::vector<PlacedStone> position, Stone own);
    void stop();

private:
    struct Request
    {
        std::optional<Move> opponent;
        std::optional<std::vector<PlacedStone>> position;
        Stone own = Stone::Empty;
        std::promise<EngineReply> result;
    };

    EngineEntry entry_;
    EngineConfig config_;
    std::atomic<bool> cancelled_{false};
    std::promise<void> startup_;
    std::mutex mutex_;
    std::condition_variable changed_;
    std::queue<Request> requests_;
    std::exception_ptr failure_;
    std::thread thread_;
    std::future<EngineReply> enqueue(Request request);
    void run();
};
} // namespace cli
