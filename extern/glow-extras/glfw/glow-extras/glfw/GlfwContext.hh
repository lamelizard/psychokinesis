#pragma once

#include <string>

struct GLFWwindow;

namespace glow {
namespace glfw{

/**
 * Creates an invisible Glfw window that provides an OpenGL context
 *
 * Also registers itself as a threadlocal global variable so that it can be accessed from everywhere
 *
 * Note: CANNOT be nested currently
 */
class GlfwContext final
{
public:
    GlfwContext();
    ~GlfwContext();

    GLFWwindow* window() const { return mWindow; }

    bool isValid() const { return mWindow != nullptr; }

    static GlfwContext* current();

    void setTitle(std::string const& title);

    void resize(int w, int h);
    void show(int w = -1, int h = -1);
    void hide();
    void swapBuffers();

private:
    GLFWwindow* mWindow = nullptr;
};

}
}
