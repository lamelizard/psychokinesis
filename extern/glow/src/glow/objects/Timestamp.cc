#include "Timestamp.hh"

#include <cassert>
#include <limits>

namespace glow
{
void Timestamp::save()
{
    checkValidGLOW();
    glQueryCounter(mObjectName, GL_TIMESTAMP);
}

bool Timestamp::isAvailable()
{
    checkValidGLOW();

    GLuint resultAvailable;
    glGetQueryObjectuiv(mObjectName, GL_QUERY_RESULT_AVAILABLE, &resultAvailable);
    return resultAvailable;
}

GLuint64 Timestamp::getNanoseconds()
{
    checkValidGLOW();

    GLuint64 queryResult;
    glGetQueryObjectui64v(mObjectName, GL_QUERY_RESULT, &queryResult);
    return queryResult;
}

double Timestamp::getSeconds()
{
    return getNanoseconds() * (1 / (1000. * 1000. * 1000.));
}

Timestamp::Timestamp()
{
    checkValidGLOW();

    mObjectName = std::numeric_limits<decltype(mObjectName)>::max();
    glGenQueries(1, &mObjectName);
    assert(mObjectName != std::numeric_limits<decltype(mObjectName)>::max() && "No OpenGL Context?");

    // use once to ensure object is created
    glQueryCounter(mObjectName, GL_TIMESTAMP);
}

Timestamp::~Timestamp()
{
    glDeleteSamplers(1, &mObjectName);
}

SharedTimestamp Timestamp::create()
{
    return std::make_shared<Timestamp>();
}
}
