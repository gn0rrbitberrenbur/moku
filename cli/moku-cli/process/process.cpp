#include "process/process.hpp"

#include "engine/engine_error.hpp"

#include <algorithm>
#include <array>
#include <stdexcept>
#include <thread>

#ifdef _WIN32
#include <mutex>
#include <windows.h>

#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
#define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00000002
#endif
#else
#include <cerrno>
#include <csignal>
#include <cstring>
#include <fcntl.h>
#include <mutex>
#include <poll.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace cli
{
namespace
{
/**
 * Checks whether the current time has exceeded the specified deadline or whether the operation has
 * been cancelled. If either condition is true, an EngineError is thrown with the appropriate kind
 * and message.
 * @param deadline The time point by which the operation must complete.
 * @param cancelled An atomic boolean that can be set to true to cancel the operation.
 * @throws EngineError if the current time has exceeded the deadline or if the operation has been
 */
void checkDeadline(Process::Clock::time_point deadline, const std::atomic<bool>& cancelled)
{
    if (cancelled.load())
    {
        throw EngineError(EngineFailure::Cancelled, "Engine request cancelled.");
    }
    if (Process::Clock::now() >= deadline)
    {
        throw EngineError(EngineFailure::Timeout, "Engine response timed out.");
    }
}

#ifdef _WIN32
class ShortSleep
{
public:
    ShortSleep()
        : timer_(CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
                                        TIMER_ALL_ACCESS))
    {
    }

    ~ShortSleep()
    {
        if (timer_)
        {
            CloseHandle(timer_);
        }
    }

    ShortSleep(const ShortSleep&) = delete;
    ShortSleep& operator=(const ShortSleep&) = delete;

    void wait()
    {
        LARGE_INTEGER due{};
        due.QuadPart = -10000;
        if (timer_ && SetWaitableTimer(timer_, &due, 0, nullptr, nullptr, FALSE) &&
            WaitForSingleObject(timer_, INFINITE) == WAIT_OBJECT_0)
        {
            return;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

private:
    HANDLE timer_;
};

void closeHandle(HANDLE& handle)
{
    if (handle && handle != INVALID_HANDLE_VALUE)
    {
        CloseHandle(handle);
        handle = nullptr;
    }
}

EngineError systemError(const std::string& operation)
{
    return EngineError(EngineFailure::Crash,
                       operation + " (Windows error " + std::to_string(GetLastError()) + ").");
}
#else
void closeFd(int& fd)
{
    if (fd >= 0)
    {
        close(fd);
        fd = -1;
    }
}

/**
 * Constructs an EngineError object with kind EngineFailure::Crash and a message that includes the
 * specified operation and the string representation of the current errno value.
 * @param operation The name of the operation that failed.
 * @return An EngineError object with kind EngineFailure::Crash and a message that includes the
 * specified operation and the string representation of the current errno value.
 */
EngineError systemError(const std::string& operation)
{
    return EngineError(EngineFailure::Crash, operation + ": " + std::strerror(errno));
}
#endif
} // namespace

struct Process::Impl
{
    Process::Observer observer;
    std::string output;
    std::string errors;
    bool eof = false;
#ifdef _WIN32
    ShortSleep sleep;
    HANDLE input = nullptr;
    HANDLE out = nullptr;
    HANDLE err = nullptr;
    HANDLE child = nullptr;
    HANDLE job = nullptr;
#else
    int input = -1;
    int out = -1;
    int err = -1;
    pid_t child = -1;
#endif

    void appendErrors(const char* data, std::size_t count)
    {
        errors.append(data, count);
        if (errors.size() > 4096)
        {
            errors.erase(0, errors.size() - 4096);
        }
    }

    bool pump(bool stderrStream)
    {
        std::array<char, 4096> buffer{};
#ifdef _WIN32
        HANDLE pipe = stderrStream ? err : out;
        DWORD available = 0;
        if (!PeekNamedPipe(pipe, nullptr, 0, nullptr, &available, nullptr))
        {
            if (GetLastError() != ERROR_BROKEN_PIPE)
            {
                throw systemError("Cannot inspect engine pipe");
            }
            if (!stderrStream)
            {
                eof = true;
            }
            return false;
        }
        if (available == 0)
        {
            return false;
        }
        DWORD count = 0;
        if (!ReadFile(pipe, buffer.data(),
                      std::min<DWORD>(available, static_cast<DWORD>(buffer.size())), &count,
                      nullptr))
        {
            throw systemError("Cannot read engine pipe");
        }
#else
        const auto count = read(stderrStream ? err : out, buffer.data(), buffer.size());
        if (count < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
            {
                return false;
            }
            throw systemError("Cannot read engine pipe");
        }
        if (count == 0 && !stderrStream)
        {
            eof = true;
        }
#endif
        if (count > 0 && observer)
        {
            observer(stderrStream ? "stderr" : "stdout",
                     std::string(buffer.data(), static_cast<std::size_t>(count)));
        }
        if (stderrStream)
        {
            appendErrors(buffer.data(), static_cast<std::size_t>(count));
        }
        else
        {
            output.append(buffer.data(), static_cast<std::size_t>(count));
        }
        return count > 0;
    }

    ~Impl()
    {
#ifdef _WIN32
        closeHandle(input);
        if (child)
        {
            if (WaitForSingleObject(child, 150) == WAIT_TIMEOUT)
            {
                TerminateJobObject(job, 1);
                WaitForSingleObject(child, 1000);
            }
        }
        closeHandle(job);
        closeHandle(child);
        closeHandle(out);
        closeHandle(err);
#else
        closeFd(input);
        if (child > 0)
        {
            bool reaped = false;
            for (int i = 0; i < 15; ++i)
            {
                const auto result = waitpid(child, nullptr, WNOHANG);
                if (result == child || (result < 0 && errno == ECHILD))
                {
                    reaped = true;
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            kill(-child, SIGKILL);
            if (!reaped)
            {
                kill(child, SIGKILL);
                while (waitpid(child, nullptr, 0) < 0 && errno == EINTR)
                {
                }
            }
        }
        closeFd(out);
        closeFd(err);
#endif
    }
};

/**
 * Constructs a Process object that launches the specified executable and sets up pipes for
 * communication with it. The observer callback is invoked whenever the process writes to its
 * output or error pipes.
 * @param executable The path to the executable to launch.
 * @param observer An optional callback function that is invoked whenever the process writes to
 * its output or error pipes. The callback receives two string arguments: the first is either
 * "stdout" or "stderr" indicating which pipe the data came from, and the second is the data that
 * was written to the pipe.
 * @throws EngineError if the process cannot be launched or if the pipes cannot be created.
 */
Process::Process(const std::filesystem::path& executable, Observer observer)
    : impl_(std::make_unique<Impl>())
{
    impl_->observer = std::move(observer);
    const auto path = std::filesystem::absolute(executable);
    const auto directory = path.parent_path();
#ifdef _WIN32
    static std::mutex launchMutex;
    const std::lock_guard<std::mutex> lock(launchMutex);
    SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
    HANDLE childInput = nullptr;
    HANDLE childOutput = nullptr;
    HANDLE childError = nullptr;
    const auto cleanup = [&]
    {
        closeHandle(childInput);
        closeHandle(childOutput);
        closeHandle(childError);
    };
    if (!CreatePipe(&childInput, &impl_->input, &security, 65536) ||
        !CreatePipe(&impl_->out, &childOutput, &security, 65536) ||
        !CreatePipe(&impl_->err, &childError, &security, 65536) ||
        !SetHandleInformation(impl_->input, HANDLE_FLAG_INHERIT, 0) ||
        !SetHandleInformation(impl_->out, HANDLE_FLAG_INHERIT, 0) ||
        !SetHandleInformation(impl_->err, HANDLE_FLAG_INHERIT, 0))
    {
        const auto error = systemError("Cannot create engine pipes");
        cleanup();
        throw error;
    }
    impl_->job = CreateJobObjectW(nullptr, nullptr);
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (!impl_->job || !SetInformationJobObject(impl_->job, JobObjectExtendedLimitInformation,
                                                &limits, sizeof(limits)))
    {
        const auto error = systemError("Cannot create engine job");
        cleanup();
        throw error;
    }
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = childInput;
    startup.hStdOutput = childOutput;
    startup.hStdError = childError;
    PROCESS_INFORMATION process{};
    std::wstring command = L"\"" + path.wstring() + L"\"";
    const BOOL created = CreateProcessW(path.c_str(), &command[0], nullptr, nullptr, TRUE,
                                        CREATE_NO_WINDOW | CREATE_SUSPENDED, nullptr,
                                        directory.c_str(), &startup, &process);
    const auto launchError = GetLastError();
    cleanup();
    if (!created)
    {
        throw std::runtime_error("Cannot launch engine (Windows error " +
                                 std::to_string(launchError) + ").");
    }
    impl_->child = process.hProcess;
    if (!AssignProcessToJobObject(impl_->job, impl_->child) ||
        ResumeThread(process.hThread) == static_cast<DWORD>(-1))
    {
        const auto error = systemError("Cannot start engine worker process");
        TerminateProcess(impl_->child, 1);
        CloseHandle(process.hThread);
        throw error;
    }
    CloseHandle(process.hThread);
#else
    static std::once_flag signalFlag;
    std::call_once(signalFlag, [] { std::signal(SIGPIPE, SIG_IGN); });
    static std::mutex launchMutex;
    const std::lock_guard<std::mutex> lock(launchMutex);
    int inPipe[2]{-1, -1};
    int outPipe[2]{-1, -1};
    int errPipe[2]{-1, -1};
    const auto cleanup = [&]
    {
        for (auto* pair : {inPipe, outPipe, errPipe})
        {
            closeFd(pair[0]);
            closeFd(pair[1]);
        }
    };
    if (pipe(inPipe) < 0 || pipe(outPipe) < 0 || pipe(errPipe) < 0)
    {
        const auto error = systemError("Cannot create engine pipes");
        cleanup();
        throw error;
    }
    for (auto* pair : {inPipe, outPipe, errPipe})
    {
        for (int index = 0; index < 2; ++index)
        {
            if (fcntl(pair[index], F_SETFD, FD_CLOEXEC) < 0)
            {
                const auto error = systemError("Cannot protect engine pipes");
                cleanup();
                throw error;
            }
        }
    }
    const std::string pathText = path.string();
    const std::string directoryText = directory.string();
    impl_->child = fork();
    if (impl_->child == 0)
    {
        if (setpgid(0, 0) < 0 || chdir(directoryText.c_str()) < 0 ||
            dup2(inPipe[0], STDIN_FILENO) < 0 || dup2(outPipe[1], STDOUT_FILENO) < 0 ||
            dup2(errPipe[1], STDERR_FILENO) < 0)
        {
            _exit(126);
        }
        for (auto* pair : {inPipe, outPipe, errPipe})
        {
            close(pair[0]);
            close(pair[1]);
        }
        char* const arguments[]{const_cast<char*>(pathText.c_str()), nullptr};
        execv(pathText.c_str(), arguments);
        _exit(127);
    }
    if (impl_->child < 0)
    {
        const auto error = systemError("Cannot launch engine");
        cleanup();
        throw error;
    }
    setpgid(impl_->child, impl_->child);
    impl_->input = inPipe[1];
    inPipe[1] = -1;
    impl_->out = outPipe[0];
    outPipe[0] = -1;
    impl_->err = errPipe[0];
    errPipe[0] = -1;
    cleanup();
    for (const int fd : {impl_->input, impl_->out, impl_->err})
    {
        const int flags = fcntl(fd, F_GETFL);
        if (flags < 0 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0)
        {
            throw systemError("Cannot configure engine pipes");
        }
    }
#endif
}

Process::~Process() = default;

/**
 * Writes a command to the engine's input pipe, followed by a carriage return and line feed
 * sequence. This function blocks until the entire command has been written or the deadline is
 * reached. If the deadline is reached before the entire command has been written, an EngineError
 * with kind EngineFailure::Timeout is thrown. If the cancelled atomic boolean is set to true
 * before the entire command has been written, an EngineError with kind EngineFailure::Cancelled is
 * thrown.
 * @param command The command to write to the engine.
 * @param deadline The time point by which the entire command must be written.
 * @param cancelled An atomic boolean that can be set to true to cancel the write operation.
 * @throws EngineError if the deadline is reached before the entire command has been written or if
 * the cancelled atomic boolean is set to true before the entire command has been written.
 */
void Process::write(const std::string& command, Clock::time_point deadline,
                    const std::atomic<bool>& cancelled)
{
    const std::string data = command + "\r\n";
    std::size_t offset = 0;
    while (offset < data.size())
    {
        checkDeadline(deadline, cancelled);
#ifdef _WIN32
        DWORD written = 0;
        if (!WriteFile(impl_->input, data.data() + offset, static_cast<DWORD>(data.size() - offset),
                       &written, nullptr))
        {
            throw systemError("Cannot write to engine");
        }
#else
        const auto written = ::write(impl_->input, data.data() + offset, data.size() - offset);
        if (written < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
                continue;
            }
            throw systemError("Cannot write to engine");
        }
#endif
        offset += static_cast<std::size_t>(written);
    }
    if (impl_->observer)
    {
        impl_->observer("stdin", data);
    }
}

/**
 * Polls the engine's output pipes for new data. If new data is available, it is read and
 * appended to the output or error buffers, and the observer callback is invoked. If the engine
 * has closed its output pipe, the eof flag is set. This function does not block and returns
 * immediately.
 * @return true if new data was read from either output pipe, false otherwise.
 */
bool Process::poll()
{
    const bool errors = impl_->pump(true);
    const bool output = impl_->pump(false);
    return errors || output;
}

/**
 * Closes the engine's input pipe and waits for the engine to terminate. If the engine does not
 * terminate within 150 milliseconds, it is forcibly terminated. If the engine's output pipes
 * remain active for more than 50 milliseconds after the engine has terminated, an EngineError
 * with kind EngineFailure::Timeout is thrown.
 * @throw EngineError if the engine's output pipes remain active for more than 50 milliseconds
 * after the engine has terminated.
 */
void Process::finish()
{
#ifdef _WIN32
    closeHandle(impl_->input);
#else
    closeFd(impl_->input);
#endif
    const auto deadline = Clock::now() + std::chrono::milliseconds(150);
    while (Clock::now() < deadline)
    {
        poll();
#ifdef _WIN32
        if (WaitForSingleObject(impl_->child, 0) == WAIT_OBJECT_0)
        {
            break;
        }
#else
        const auto result = waitpid(impl_->child, nullptr, WNOHANG);
        if (result == impl_->child || (result < 0 && errno == ECHILD))
        {
            // Descendants may still hold the inherited pipes open.
            kill(-impl_->child, SIGKILL);
            impl_->child = -1;
            break;
        }
        if (result < 0 && errno != EINTR)
        {
            throw systemError("Cannot wait for engine");
        }
#endif
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
#ifdef _WIN32
    if (!TerminateJobObject(impl_->job, 1) ||
        WaitForSingleObject(impl_->child, 1000) != WAIT_OBJECT_0)
    {
        throw systemError("Cannot stop engine");
    }
#else
    if (impl_->child > 0)
    {
        kill(-impl_->child, SIGKILL);
        kill(impl_->child, SIGKILL);
        while (waitpid(impl_->child, nullptr, 0) < 0)
        {
            if (errno != EINTR && errno != ECHILD)
            {
                throw systemError("Cannot stop engine");
            }
            if (errno == ECHILD)
            {
                break;
            }
        }
        impl_->child = -1;
    }
#endif
    const auto drainDeadline = Clock::now() + std::chrono::milliseconds(50);
    while (poll())
    {
        if (Clock::now() >= drainDeadline)
        {
            throw EngineError(EngineFailure::Timeout,
                              "Engine pipes remained active during shutdown.");
        }
    }
}

/**
 * Reads a line of output from the engine, blocking until a complete line is available or the
 * engine closes its output pipe. The line is terminated by either a carriage return or a line
 * feed character, and the terminating character is not included in the returned string. If the
 * engine closes its output pipe before a complete line is available, an EngineError with kind
 * EngineFailure::Crash is thrown. If the engine produces a line that exceeds 8192 bytes,
 * an EngineError with kind EngineFailure::Protocol is thrown. If the deadline is reached
 * before a complete line is available, an EngineError with kind EngineFailure::Timeout is thrown.
 * @param deadline The time point by which a complete line must be available.
 * @param cancelled An atomic boolean that can be set to true to cancel the read operation.
 * @return The line of output from the engine, without the terminating character.
 * @throws EngineError if the engine closes its output pipe, produces a line that exceeds 8192
 * bytes, or if the deadline is reached before a complete line is available.
 *
 */
std::string Process::readLine(Clock::time_point deadline, const std::atomic<bool>& cancelled)
{
    while (true)
    {
        checkDeadline(deadline, cancelled);
        impl_->pump(true);
        const auto end = impl_->output.find_first_of("\r\n");
        if (end != std::string::npos)
        {
            if (end > 8192)
            {
                throw EngineError(EngineFailure::Protocol,
                                  "Engine output line exceeds 8192 bytes.");
            }
            std::string line = impl_->output.substr(0, end);
            impl_->output.erase(0, end + 1);
            return line;
        }
        if (impl_->output.size() > 8192)
        {
            throw EngineError(EngineFailure::Protocol, "Engine output line exceeds 8192 bytes.");
        }
        if (impl_->eof)
        {
            throw EngineError(EngineFailure::Crash, "Engine closed its output pipe.");
        }
        impl_->pump(false);
        if (impl_->output.find_first_of("\r\n") == std::string::npos && !impl_->eof)
        {
#ifdef _WIN32
            impl_->sleep.wait();
#else
            pollfd ready{impl_->out, POLLIN, 0};
            ::poll(&ready, 1, 5);
#endif
        }
    }
}

/**
 * Return the most recent 4096 bytes of engine stderr output, or less if the engine has not
 * produced that much output yet. This is intended to be used for diagnostic purposes when the
 * engine has crashed or otherwise failed to respond to a command.
 * @return string containing the most recent engine stderr output
 */
std::string Process::diagnostics() const
{
    return impl_->errors;
}
} // namespace cli
