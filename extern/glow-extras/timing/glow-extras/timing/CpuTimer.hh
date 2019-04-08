#pragma once

#include <chrono>

namespace glow
{
namespace timing
{
/**
 * Accurately time CPU operations
 *
 * Usage:
 *
 * timer.restart()
 * // timed code
 * delta = timer.elapsedSeconds()
 *
 */
class CpuTimer
{
private:
    std::chrono::time_point<std::chrono::high_resolution_clock> mStart;

public:
    CpuTimer() { restart(); }

    /// Restart the timer
    void restart() { mStart = std::chrono::high_resolution_clock::now(); }

    /// -- Get the duration since last restart --

    float elapsedSeconds() const
    {
        return std::chrono::duration<float>(std::chrono::high_resolution_clock::now() - mStart).count();
    }

    double elapsedSecondsD() const
    {
        return std::chrono::duration<double>(std::chrono::high_resolution_clock::now() - mStart).count();
    }

    long long elapsedNanoseconds() const
    {
        return std::chrono::nanoseconds{std::chrono::high_resolution_clock::now() - mStart}.count();
    }
};
}
}
