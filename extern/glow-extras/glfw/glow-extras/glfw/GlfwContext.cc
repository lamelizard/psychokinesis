#include "GlfwContext.hh"

#include <cassert>

#include <glow/common/log.hh>
#include <glow/common/thread_local.hh>
#include <glow/gl.hh>
#include <glow/glow.hh>
#include <glow/util/AsyncTextureLoader.hh>

#include <GLFW/glfw3.h>

using namespace glow;
using namespace glfw;

namespace
{
GLOW_THREADLOCAL GlfwContext *gContext = nullptr;
}

GlfwContext::GlfwContext()
{
    assert(!gContext);
    if (gContext)
    {
        std::cerr << "A GlfwContext already exists and cannot be created twice" << std::endl;
        return;
    }

    auto title = "GLFW Window";

    // Taken from http://www.glfw.org/documentation.html
    // Initialize the library
    if (!glfwInit())
    {
        std::cerr << "Unable to initialize GLFW" << std::endl;
        return;
    }

    // invisible window
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

    // Request debug context
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);

    // Try to get core context 4.5
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);

    // Create a windowed mode window and its OpenGL context
    mWindow = glfwCreateWindow(1, 1, title, NULL, NULL);
    if (!mWindow)
    {
        std::cerr << "Unable to get OpenGL 4.5 Core Debug Context. Trying again with Compat." << std::endl;
        glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_FALSE);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_ANY_PROFILE);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 1);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
        mWindow = glfwCreateWindow(1, 1, "GLFW Window", NULL, NULL);

        if (!mWindow)
        {
            std::cerr << "Unable to create a GLFW window" << std::endl;
            glfwTerminate();
            return;
        }
    }

    // Make the window's context current
    glfwMakeContextCurrent(mWindow);

    // WORKAROUND for Intel bug (3.3 available but 3.0 returned UNLESS explicitly requested)
#define GL_CALL
#ifdef GLOW_COMPILER_MSVC
#if _WIN32
#undef GL_CALL
#define GL_CALL __stdcall // 32bit windows needs a special calling convention
#endif
#endif
    using glGetIntegerFunc = void GL_CALL(GLenum, GLint *);
    auto getGlInt = (glGetIntegerFunc *)glfwGetProcAddress("glGetIntegerv");
    GLint gmajor, gminor;
    getGlInt(GL_MAJOR_VERSION, &gmajor);
    getGlInt(GL_MINOR_VERSION, &gminor);
    if (gmajor * 10 + gminor < 33)
    {
        std::cerr << "OpenGL Version below 3.3. Trying to get at least 3.3." << std::endl;

        // destroy current window
        glfwDestroyWindow(mWindow);

        // request vanilla 3.3 context
        glfwDefaultWindowHints();
        glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_FALSE);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_ANY_PROFILE);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);

        // re-try window creation
        mWindow = glfwCreateWindow(1, 1, title, NULL, NULL);
        if (!mWindow)
        {
            std::cerr << "Unable to create a GLFW window with OpenGL 3.3. (GLOW requires at least 3.3)" << std::endl;
            glfwTerminate();
            return;
        }

        // Make the window's context current (again)
        glfwMakeContextCurrent(mWindow);
    }

    // Initialize GLOW
    if (!glow::initGLOW())
    {
        std::cerr << "Unable to initialize GLOW" << std::endl;
        return;
    }

    // restore ogl state
    glow::restoreDefaultOpenGLState();

    gContext = this;
}

GlfwContext::~GlfwContext()
{
    assert(gContext == this);
    gContext = nullptr;

    glfwTerminate();
	AsyncTextureLoader::shutdown();
    mWindow = nullptr;
}

GlfwContext *GlfwContext::current()
{
    return gContext;
}

void GlfwContext::setTitle(const std::string &title)
{
    assert(isValid());
    glfwSetWindowTitle(mWindow, title.c_str());
}

void GlfwContext::resize(int w, int h)
{
    assert(isValid());
    glfwSetWindowSize(mWindow, w, h);
}

void GlfwContext::show(int w, int h)
{
    assert(isValid());
    if (w > 0 && h > 0)
        resize(w, h);
    glfwShowWindow(mWindow);
}

void GlfwContext::hide()
{
    assert(isValid());
    glfwHideWindow(mWindow);
}

void GlfwContext::swapBuffers()
{
    assert(isValid());
    glfwSwapBuffers(mWindow);
}
