#include "Query.hh"

#include <glow/glow.hh>

#include <cassert>
#include <limits>

glow::Query::Query(GLenum _defaultTarget) : mTarget(_defaultTarget)
{
    checkValidGLOW();

    mObjectName = std::numeric_limits<decltype(mObjectName)>::max();
    glGenQueries(1, &mObjectName);
    assert(mObjectName != std::numeric_limits<decltype(mObjectName)>::max() && "No OpenGL Context?");

    // ensure query exists
    glBeginQuery(mTarget, mObjectName);
    glEndQuery(mTarget);
}
