#pragma once

#include "Query.hh"

namespace glow
{
GLOW_SHARED(class, TimerQuery);
/**
 * TimerQueries can measure GPU execution speed.
 *
 * Only available since OpenGL 3.3 or GL_ARB_timer_query (on OpenGL 3.2)
 */
class TimerQuery final : public Query
{
public:
    TimerQuery() : Query(GL_TIME_ELAPSED) {}

    /// Converts a ns time value to seconds
    static double toSeconds(GLint64 timeInNs) { return timeInNs / (1000. * 1000. * 1000.); }

public:
    static SharedTimerQuery create() { return std::make_shared<TimerQuery>(); }
};
}
