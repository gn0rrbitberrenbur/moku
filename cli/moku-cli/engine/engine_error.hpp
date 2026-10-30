#pragma once

#include <stdexcept>
#include <string>

namespace cli
{
enum class EngineFailure
{
    Timeout,
    IllegalMove,
    Crash,
    Protocol,
    Cancelled
};

class EngineError : public std::runtime_error
{
public:
    /**
     * @param kind failure category
     * @param message human-readable description
     */
    EngineError(EngineFailure kind, const std::string& message)
        : std::runtime_error(message), kind_(kind)
    {
    }

    EngineFailure kind() const
    {
        return kind_;
    }

private:
    EngineFailure kind_;
};

/**
 * @param kind failure category
 * @return stable snake_case identifier used in reports
 */
inline const char* failureName(EngineFailure kind)
{
    switch (kind)
    {
    case EngineFailure::Timeout:
        return "timeout";
    case EngineFailure::IllegalMove:
        return "illegal_move";
    case EngineFailure::Crash:
        return "crash";
    case EngineFailure::Protocol:
        return "protocol";
    case EngineFailure::Cancelled:
        return "cancelled";
    }
    return "unknown";
}
} // namespace cli
