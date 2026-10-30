#include "engine/engine.hpp"

#include "engine/engine_error.hpp"
#include "process/process.hpp"

#include <algorithm>
#include <stdexcept>

namespace cli
{
namespace
{
/**
 * Reads a line from the engine process, ignoring messages and debug output, and returns the first
 * non-empty line that is not a message or debug output. If the engine returns an error or unknown
 * command, throws an EngineError with kind Protocol.
 * @param process The engine process to read from.
 * @param deadline The time point by which the read operation must complete.
 * @param cancelled An atomic boolean that indicates whether the operation has been cancelled.
 * @param debugEnabled Optional pointer to a boolean that will be set to true if the engine
 * indicates that debug output is enabled, or false if it is disabled.
 * @return The first non-empty line from the engine that is not a message or debug output.
 * @throws EngineError with kind Protocol if the engine returns an error or unknown command.
 */
std::string response(Process& process, Process::Clock::time_point deadline,
                     const std::atomic<bool>& cancelled, bool* debugEnabled = nullptr)
{
    while (true)
    {
        const auto line = process.readLine(deadline, cancelled);
        if (debugEnabled && (line == "DEBUG_OUTPUT True" || line == "DEBUG_OUTPUT False"))
        {
            *debugEnabled = line == "DEBUG_OUTPUT True";
            continue;
        }
        if (debugEnabled && line.find("DEBUG_OUTPUT") != std::string::npos &&
            (line.rfind("ERROR", 0) == 0 || line.rfind("UNKNOWN", 0) == 0))
        {
            continue;
        }
        if (line.empty() || line == "MESSAGE" || line.rfind("MESSAGE ", 0) == 0 ||
            line == "DEBUG" || line.rfind("DEBUG ", 0) == 0)
        {
            continue;
        }
        if (line.rfind("ERROR", 0) == 0 || line.rfind("UNKNOWN", 0) == 0)
        {
            throw EngineError(EngineFailure::Protocol, line);
        }
        return line;
    }
}

/**
 * Classifies an exception into an EngineFailure category.
 * @param error The exception to classify.
 * @return The EngineFailure category corresponding to the exception.
 */
EngineFailure classify(const std::exception& error)
{
    if (const auto* engineError = dynamic_cast<const EngineError*>(&error))
    {
        return engineError->kind();
    }
    if (dynamic_cast<const std::out_of_range*>(&error))
    {
        return EngineFailure::IllegalMove;
    }
    return EngineFailure::Protocol;
}

/**
 * Converts a Move object to a string representation in the format "x,y".
 * @param move The Move object to convert.
 * @return A string representation of the move in the format "x,y".
 */
std::string coordinates(Move move)
{
    return std::to_string(move.x) + "," + std::to_string(move.y);
}
} // namespace

EngineWorker::EngineWorker(EngineEntry entry, EngineConfig config)
    : entry_(std::move(entry)), config_(config)
{
    EngineStore::validate(entry_);
    if (config_.size < 5 || config_.size > 19 || config_.turnMilliseconds < 1 ||
        config_.turnMilliseconds > 600000 || config_.startupMilliseconds < 1)
    {
        throw std::invalid_argument("Invalid engine configuration.");
    }
    thread_ = std::thread(&EngineWorker::run, this);
}

/**
 * Destroys the EngineWorker object, stopping the engine process and joining the worker thread.
 * If the engine process is still running, it will be terminated.
 */
EngineWorker::~EngineWorker()
{
    stop();
    if (thread_.joinable())
    {
        thread_.join();
    }
}

/**
 * Stops the engine worker by setting the cancelled flag and notifying the worker thread.
 * This will cause the worker thread to exit its main loop and terminate the engine process.
 * @return A future that will be fulfilled when the engine worker has stopped.
 */
std::future<void> EngineWorker::ready()
{
    return startup_.get_future();
}

/**
 * Requests a move from the engine by sending the appropriate command (BEGIN or TURN) and
 * returns a future that will be fulfilled with the engine's reply (move and elapsed time).
 * @param opponent The previous opponent move, or no value for BEGIN on an empty board.
 * @return A future that will be fulfilled with the engine's reply (move and elapsed time
 */
std::future<EngineReply> EngineWorker::request(std::optional<Move> opponent)
{
    Request request;
    request.opponent = opponent;
    return enqueue(std::move(request));
}

/**
 * Requests a move from the engine after imposing a complete position with BOARD and returns a
 * future that will be fulfilled with the engine's reply (move and elapsed time).
 * @param position The stones in the order they were played.
 * @param own The color played by this engine (must not be Stone::Empty).
 * @return A future that will be fulfilled with the engine's reply (move and elapsed time).
 * @throws std::invalid_argument if own is Stone::Empty.
 */
std::future<EngineReply> EngineWorker::requestBoard(std::vector<PlacedStone> position, Stone own)
{
    if (own == Stone::Empty)
    {
        throw std::invalid_argument("BOARD requests need the engine color.");
    }
    Request request;
    request.position = std::move(position);
    request.own = own;
    return enqueue(std::move(request));
}

/**
 * Enqueues a request to the engine worker's request queue and returns a future that will be
 * fulfilled with the engine's reply (move and elapsed time). If the engine worker has already
 * been stopped, the future will be fulfilled with an error.
 * @param request The request to enqueue.
 * @return A future that will be fulfilled with the engine's reply (move and elapsed time).
 * @throws EngineError with kind Cancelled if the engine worker has been stopped.
 */
std::future<EngineReply> EngineWorker::enqueue(Request request)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (failure_)
    {
        std::rethrow_exception(failure_);
    }
    if (cancelled_.load())
    {
        throw EngineError(EngineFailure::Cancelled, "Engine has been stopped.");
    }
    auto result = request.result.get_future();
    requests_.push(std::move(request));
    changed_.notify_one();
    return result;
}

/**
 * Stops the engine worker by setting the cancelled flag and notifying the worker thread.
 * This will cause the worker thread to exit its main loop and terminate the engine process.
 * If the engine worker has already been stopped, this function has no effect.
 */
void EngineWorker::stop()
{
    cancelled_.store(true);
    changed_.notify_all();
}

/**
 * Runs the engine worker's main loop, which initializes the engine process, handles requests,
 * and processes responses. If an error occurs, it sets the failure state and fulfills any pending
 * requests with the error. This function is executed in a separate thread.
 */
void EngineWorker::run()
{
    bool initialized = false;
    bool debugEnabled = false;
    bool discarding = false;
    std::string stderrLine;
    std::unique_ptr<Process> process;
    std::optional<Request> active;
    try
    {
        Process::Observer observer;
        if (config_.debug)
        {
            observer = [&](const std::string& stream, const std::string& text)
            {
                config_.debug->record(config_.debugEngine, entry_.name, stream, text);
                if (stream != "stderr")
                {
                    return;
                }
                for (const char ch : text)
                {
                    if (ch == '\r' || ch == '\n')
                    {
                        if (!discarding && debugEnabled && stderrLine.rfind("[METRICS] ", 0) == 0)
                        {
                            std::optional<SearchMetrics> metrics;
                            try
                            {
                                metrics = parseMetrics(stderrLine.substr(10), config_.size);
                            }
                            catch (const std::invalid_argument& error)
                            {
                                config_.debug->record(config_.debugEngine, entry_.name,
                                                      "metrics_error", error.what());
                                config_.debug->reset(config_.debugEngine);
                            }
                            if (metrics)
                            {
                                config_.debug->metrics(config_.debugEngine, *metrics);
                            }
                        }
                        stderrLine.clear();
                        discarding = false;
                    }
                    else if (!discarding)
                    {
                        stderrLine += ch;
                        if (stderrLine.size() > 8192)
                        {
                            if (stderrLine.rfind("[METRICS] ", 0) == 0)
                            {
                                config_.debug->record(config_.debugEngine, entry_.name,
                                                      "metrics_error",
                                                      "Metrics line exceeds 8192 bytes.");
                                config_.debug->reset(config_.debugEngine);
                            }
                            stderrLine.clear();
                            discarding = true;
                        }
                    }
                }
            };
        }
        process = std::make_unique<Process>(entry_.executable, std::move(observer));
        const auto startupDeadline =
            Process::Clock::now() + std::chrono::milliseconds(config_.startupMilliseconds);
        process->write("START " + std::to_string(config_.size), startupDeadline, cancelled_);
        const auto answer = response(*process, startupDeadline, cancelled_);
        if (answer != "OK" && answer.rfind("OK ", 0) != 0)
        {
            throw std::runtime_error("Expected START acknowledgement, received: " + answer);
        }
        const std::string info =
            "INFO timeout_turn " + std::to_string(config_.turnMilliseconds) +
            "\r\nINFO timeout_match 0\r\nINFO max_memory 0\r\nINFO game_type " +
            std::to_string(config_.humanOpponent ? 0 : 1) + "\r\nINFO rule " +
            std::to_string(config_.rule == Rule::ExactFive ? 1 : 0);
        process->write(info, startupDeadline, cancelled_);
        if (config_.debug)
        {
            const std::string debugCommand =
                "DEBUG_OUTPUT " + std::to_string(config_.debugOutputMilliseconds);
            const auto debugDeadline =
                std::min(startupDeadline, Process::Clock::now() + std::chrono::milliseconds(200));
            process->write(debugCommand, debugDeadline, cancelled_);
            try
            {
                while (true)
                {
                    const auto line = process->readLine(debugDeadline, cancelled_);
                    if (line == "DEBUG_OUTPUT True")
                    {
                        debugEnabled = true;
                        break;
                    }
                    if (line == "DEBUG_OUTPUT False")
                    {
                        process->write("DEBUG_OUTPUT", debugDeadline, cancelled_);
                        continue;
                    }
                    if (line.rfind("ERROR", 0) == 0 || line.rfind("UNKNOWN", 0) == 0)
                    {
                        config_.debug->record(config_.debugEngine, entry_.name, "debug_status",
                                              "DEBUG_OUTPUT unsupported; using Thinking.");
                        break;
                    }
                }
            }
            catch (const EngineError& error)
            {
                if (error.kind() != EngineFailure::Timeout)
                {
                    throw;
                }
                config_.debug->record(config_.debugEngine, entry_.name, "debug_status",
                                      "No DEBUG_OUTPUT acknowledgement; using Thinking.");
            }
        }
        startup_.set_value();
        initialized = true;
        while (true)
        {
            {
                std::unique_lock<std::mutex> lock(mutex_);
                if (config_.debug)
                {
                    changed_.wait_for(lock, std::chrono::milliseconds(10),
                                      [&] { return cancelled_.load() || !requests_.empty(); });
                }
                else
                {
                    changed_.wait(lock, [&] { return cancelled_.load() || !requests_.empty(); });
                }
                if (cancelled_.load())
                {
                    throw EngineError(EngineFailure::Cancelled, "Engine request cancelled.");
                }
                if (requests_.empty())
                {
                    lock.unlock();
                    process->poll();
                    continue;
                }
                active.emplace(std::move(requests_.front()));
                requests_.pop();
            }
            const auto started = Process::Clock::now();
            const auto deadline = started + std::chrono::milliseconds(config_.turnMilliseconds);
            std::string command = "INFO time_left 2147483647\r\n";
            if (active->position)
            {
                command += "BOARD";
                for (const auto& placed : *active->position)
                {
                    command += "\r\n" + coordinates(placed.move) +
                               (placed.stone == active->own ? ",1" : ",2");
                }
                command += "\r\nDONE";
            }
            else if (active->opponent)
            {
                command += "TURN " + coordinates(*active->opponent);
            }
            else
            {
                command += "BEGIN";
            }
            if (config_.debug)
            {
                process->poll();
                config_.debug->reset(config_.debugEngine);
                stderrLine.clear();
                discarding = false;
            }
            process->write(command, deadline, cancelled_);
            const Move move = parseProtocolMove(
                response(*process, deadline, cancelled_, config_.debug ? &debugEnabled : nullptr),
                config_.size);
            const std::chrono::duration<double, std::milli> elapsed =
                Process::Clock::now() - started;
            active->result.set_value({move, elapsed.count()});
            active.reset();
        }
    }
    catch (const std::exception& error)
    {
        const auto kind = classify(error);
        std::string message = entry_.name + ": " + error.what();
        if (process)
        {
            try
            {
                const std::atomic<bool> ending{false};
                process->write("END", Process::Clock::now() + std::chrono::milliseconds(50),
                               ending);
            }
            catch (const std::exception& shutdownError)
            {
                message += "\nShutdown: " + std::string(shutdownError.what());
            }
            if (config_.debug)
            {
                try
                {
                    process->finish();
                }
                catch (const std::exception& shutdownError)
                {
                    message += "\nShutdown capture: " + std::string(shutdownError.what());
                }
            }
        }
        if (process && !process->diagnostics().empty())
        {
            message += "\nEngine stderr: " + process->diagnostics();
        }
        const auto failure = std::make_exception_ptr(EngineError(kind, message));
        if (!initialized)
        {
            startup_.set_exception(failure);
        }
        if (active)
        {
            active->result.set_exception(failure);
        }
        std::lock_guard<std::mutex> lock(mutex_);
        failure_ = failure;
        while (!requests_.empty())
        {
            requests_.front().result.set_exception(failure);
            requests_.pop();
        }
    }
}
} // namespace cli
