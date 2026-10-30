#pragma once

#include "settings/settings.hpp"
#include "tests/test_runner.hpp"
#include "ui/terminal.hpp"

#include <optional>

namespace cli
{
class TestMode
{
public:
    /**
     * @param terminal interactive terminal used by the setup, progress and report screens
     * @param store engine registry used for engine selection
     */
    TestMode(Terminal& terminal, const EngineStore& store);
    void run();

private:
    Terminal& terminal_;
    const EngineStore& store_;
    TestSettings defaults_;
    std::optional<TestSettings> setup();
    void execute(const TestSettings& settings);
    void report(const TestSettings& settings, const TestOutcome& outcome);
};
} // namespace cli
