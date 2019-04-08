#include "GlfwApp.hh"

#ifdef GLOW_EXTRAS_HAS_ANTTWEAKBAR
#include <AntTweakBar.h>
#endif

#ifdef GLOW_EXTRAS_HAS_IMGUI
#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_opengl3.h>
#endif

#include <glm/ext.hpp>
#include <glm/glm.hpp>

#include <cassert>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>

#include <glow/gl.hh>

// NOTE: AFTER gl.hh
#ifdef GLOW_EXTRAS_HAS_NANOGUI // nanogui support
#include <nanogui/nanogui.h>
#endif

// NOTE: AFTER gl.hh
#include <GLFW/glfw3.h>

#ifdef GLOW_EXTRAS_HAS_AION
#include <aion/ActionAnalyzer.hh>
#endif

#include <glow/common/log.hh>
#include <glow/common/str_utils.hh>
#include <glow/glow.hh>

#include <glow/util/DefaultShaderParser.hh>

#include <glow/objects/OcclusionQuery.hh>
#include <glow/objects/PrimitiveQuery.hh>
#include <glow/objects/Timestamp.hh>

#include <glow-extras/debugging/DebugOverlay.hh>

#include "GlfwContext.hh"

using namespace glow;
using namespace glow::glfw;

static GlfwApp* sCurrApp = nullptr;

#if GLOW_EXTRAS_HAS_ANTTWEAKBAR
static void TW_CALL GlfwAppTweakCallback(void* clientData)
{
    (*(std::function<void()>*)clientData)();
}
#endif

static std::string thousandSep(size_t val)
{
    auto s = std::to_string(val);
    auto l = s.size();
    while (l > 3)
    {
        s = s.substr(0, l - 3) + "'" + s.substr(l - 3);
        l -= 3;
    }
    return s;
}

void GlfwApp::setTitle(const std::string& title)
{
    mTitle = title;
    if (mWindow)
        glfwSetWindowTitle(mWindow, title.c_str());
}

void GlfwApp::setClipboardString(const std::string& s) const
{
    glfwSetClipboardString(mWindow, s.c_str());
}

std::string GlfwApp::getClipboardString() const
{
    auto s = glfwGetClipboardString(mWindow);
    return s ? s : "";
}

bool GlfwApp::isMouseButtonPressed(int button) const
{
    return glfwGetMouseButton(mWindow, button) == GLFW_PRESS;
}

bool GlfwApp::isKeyPressed(int key) const
{
    return glfwGetKey(mWindow, key) == GLFW_PRESS;
}

bool GlfwApp::shouldClose() const
{
    return glfwWindowShouldClose(mWindow);
}

bool GlfwApp::isFullscreen() const
{
    return mFullscreenActive;
}

bool GlfwApp::isMinimized() const
{
    return mMinimized;
}

void GlfwApp::requestClose()
{
    glfwSetWindowShouldClose(mWindow, true);
}

#if GLOW_EXTRAS_HAS_ANTTWEAKBAR
void GlfwApp::tweak(int& value, std::string const& name, std::string const& options)
{
    TwAddVarRW(tweakbar(), name.c_str(), TW_TYPE_INT32, &value, options.c_str());
}

void GlfwApp::tweak(bool& value, std::string const& name, std::string const& options)
{
    TwAddVarRW(tweakbar(), name.c_str(), TW_TYPE_BOOLCPP, &value, options.c_str());
}

void GlfwApp::tweak(float& value, std::string const& name, std::string const& options)
{
    TwAddVarRW(tweakbar(), name.c_str(), TW_TYPE_FLOAT, &value, options.c_str());
}

void GlfwApp::tweak(double& value, std::string const& name, std::string const& options)
{
    TwAddVarRW(tweakbar(), name.c_str(), TW_TYPE_DOUBLE, &value, options.c_str());
}

void GlfwApp::tweak(glm::quat& value, std::string const& name, std::string const& options)
{
    TwAddVarRW(tweakbar(), name.c_str(), TW_TYPE_QUAT4F, &value, options.c_str());
}

void GlfwApp::tweak(std::string& value, std::string const& name, std::string const& options)
{
    TwAddVarRW(tweakbar(), name.c_str(), TW_TYPE_STDSTRING, &value, options.c_str());
}

void GlfwApp::tweak_dir(glm::vec3& value, std::string const& name, std::string const& options)
{
    TwAddVarRW(tweakbar(), name.c_str(), TW_TYPE_DIR3F, &value, options.c_str());
}

void GlfwApp::tweak_color(glm::vec3& value, std::string const& name, std::string const& options)
{
    TwAddVarRW(tweakbar(), name.c_str(), TW_TYPE_COLOR3F, &value, options.c_str());
}

void GlfwApp::tweak_color(glm::vec4& value, std::string const& name, std::string const& options)
{
    TwAddVarRW(tweakbar(), name.c_str(), TW_TYPE_COLOR4F, &value, options.c_str());
}

void GlfwApp::tweak_color(uint32_t& value, std::string const& name, std::string const& options)
{
    TwAddVarRW(tweakbar(), name.c_str(), TW_TYPE_COLOR32, &value, options.c_str());
}

void GlfwApp::tweak_button(std::string const& name, std::function<void()> const& fun, std::string const& options)
{
    TwAddButton(tweakbar(), name.c_str(), GlfwAppTweakCallback, new std::function<void()>(fun), options.c_str());
}
#endif

void GlfwApp::init()
{
    if (mUseDefaultCamera)
    {
        // create camera with some sensible defaults
        mCamera = camera::Camera::create();
        mCamera->handle.setLookAt(glm::vec3{2}, glm::vec3{0});
    }

    mPrimitiveQuery = PrimitiveQuery::create();
    mOcclusionQuery = OcclusionQuery::create();
    mGpuTimer = timing::GpuTimer::create();

    // init UI
    switch (mGui)
    {
    // anttweakbar
    case Gui::AntTweakBar:
#ifdef GLOW_EXTRAS_HAS_ANTTWEAKBAR
        assert(mTweakbar == nullptr);
        TwInit(TW_OPENGL_CORE, nullptr); // for core profile
        TwWindowSize(mWindowWidth, mWindowHeight);
        mTweakbar = TwNewBar("Tweakbar");
#else
        glow::warning() << "AntTweakBar GUI not supported (are you missing a dependency?)";
#endif
        break;

    // nanogui
    case Gui::NanoGui:
#ifdef GLOW_EXTRAS_HAS_NANOGUI
        mNanoScreen = new nanogui::Screen();
        mNanoScreen->initialize(mWindow, false);
        mNanoForm.reset(new nanogui::FormHelper(mNanoScreen));
#else
        glow::warning() << "NanoGUI not supported (are you missing a dependency?)";
#endif
        break;

    // imgui
    case Gui::ImGui:
#ifdef GLOW_EXTRAS_HAS_IMGUI
        ImGui::CreateContext();
        ImGui_ImplGlfw_InitForOpenGL(window(), false);
        ImGui_ImplOpenGL3_Init();
#else
        glow::warning() << "ImGUI not supported (are you missing a dependency?)";
#endif
        break;

    case Gui::None:
        break;
    }
}

void GlfwApp::update(float) {}

void GlfwApp::render(float) {}

void GlfwApp::scheduledUpdate() {}

void GlfwApp::onResize(int w, int h)
{
#if GLOW_EXTRAS_HAS_NANOGUI
    if (mGui == Gui::NanoGui)
        mNanoScreen->resizeCallbackEvent(w, h);
#endif

    if (mCamera)
        mCamera->setViewportSize(static_cast<unsigned>(w), static_cast<unsigned>(h));
}

void GlfwApp::onClose()
{
#ifdef GLOW_EXTRAS_HAS_IMGUI
    ImGui_ImplGlfw_Shutdown();
    ImGui_ImplOpenGL3_Shutdown();
#endif
}

void GlfwApp::onGui() {}

bool GlfwApp::onKey(int key, int scancode, int action, int mods)
{
#if GLOW_EXTRAS_HAS_NANOGUI
    if (mGui == Gui::NanoGui && mNanoScreen->keyCallbackEvent(key, scancode, action, mods))
        return true;
#endif

#if GLOW_EXTRAS_HAS_ANTTWEAKBAR
    if (mGui == Gui::AntTweakBar && TwEventKeyGLFW(mWindow, key, scancode, action, mods))
        return true;
#endif

#if GLOW_EXTRAS_HAS_IMGUI
    if (mGui == Gui::ImGui)
    {
        ImGui_ImplGlfw_KeyCallback(window(), key, scancode, action, mods);

        if (ImGui::GetIO().WantCaptureKeyboard || ImGui::GetIO().WantTextInput)
            return true;
    }

    if (mEnableDebugOverlay)
    {
        if (key == GLFW_KEY_F12 && action == GLFW_PRESS)
        {
            debugging::DebugOverlay::ToggleVisibility();
            return true;
        }
    }
#endif

    if (key == GLFW_KEY_HOME && action != GLFW_RELEASE)
    {
        onResetView();
        return true;
    }

    return false;
}

bool GlfwApp::onChar(unsigned int codepoint, int /*mods*/)
{
#if GLOW_EXTRAS_HAS_NANOGUI
    if (mGui == Gui::NanoGui && mNanoScreen->charCallbackEvent(codepoint))
        return true;
#endif

#if GLOW_EXTRAS_HAS_ANTTWEAKBAR
    if (mGui == Gui::AntTweakBar && TwEventCharGLFW(mWindow, codepoint))
        return true;
#endif

#if GLOW_EXTRAS_HAS_IMGUI
    if (mGui == Gui::ImGui)
    {
        ImGui_ImplGlfw_CharCallback(window(), codepoint);

        if (ImGui::GetIO().WantTextInput)
            return true;
    }
#endif

    return false;
}

bool GlfwApp::onMousePosition(double x, double y)
{
#if GLOW_EXTRAS_HAS_NANOGUI
    if (mGui == Gui::NanoGui && mNanoScreen->cursorPosCallbackEvent(x, y))
        return true;
#endif

#if GLOW_EXTRAS_HAS_ANTTWEAKBAR
    if (mGui == Gui::AntTweakBar && TwEventMousePosGLFW(mWindow, x, y))
        return true;
#endif

#if GLOW_EXTRAS_HAS_IMGUI
    if (mGui == Gui::ImGui)
    {
        if (ImGui::GetIO().WantCaptureMouse)
            return true;
    }
#endif

    return false;
}

bool GlfwApp::onMouseButton(double x, double y, int button, int action, int mods, int clickCount)
{
#if GLOW_EXTRAS_HAS_NANOGUI
    if (mGui == Gui::NanoGui && mCursorMode == glfw::CursorMode::Normal && mNanoScreen->mouseButtonCallbackEvent(button, action, mods))
        return true;
#endif

#if GLOW_EXTRAS_HAS_ANTTWEAKBAR
    if (mGui == Gui::AntTweakBar && mCursorMode == glfw::CursorMode::Normal && TwEventMouseButtonGLFW(mWindow, button, action, mods))
        return true;
#endif

#if GLOW_EXTRAS_HAS_IMGUI
    if (mGui == Gui::ImGui && mCursorMode == glfw::CursorMode::Normal)
    {
        ImGui_ImplGlfw_MouseButtonCallback(window(), button, action, mods);

        if (ImGui::GetIO().WantCaptureMouse)
            return true;
    }
#endif

    // Double [MMB] (no mods) -> reset view
    if (button == GLFW_MOUSE_BUTTON_MIDDLE && action != GLFW_RELEASE && clickCount > 1 && mods == 0)
        onResetView();

    return false;
}

bool GlfwApp::onMouseScroll(double sx, double sy)
{
#if GLOW_EXTRAS_HAS_NANOGUI
    if (mGui == Gui::NanoGui && mNanoScreen->scrollCallbackEvent(sx, sy))
        return true;
#endif

#if GLOW_EXTRAS_HAS_ANTTWEAKBAR
    if (mGui == Gui::AntTweakBar && TwEventMouseWheelGLFW(mWindow, sx, sy))
        return true;
#endif

#if GLOW_EXTRAS_HAS_IMGUI
    if (mGui == Gui::ImGui)
    {
        ImGui_ImplGlfw_ScrollCallback(window(), sx, sy);

        if (ImGui::GetIO().WantCaptureMouse)
            return true;
    }
#endif

    // camera handling
    if (mCamera && mUseDefaultCameraHandling && sy != 0.)
    {
        // TODO
        //        auto f = float(glm::pow(1 + mCameraScrollSpeed / 100.0, -sy));
        //        float camDis = mCamera->getLookAtDistance() * f;
        //        camDis = glm::clamp(camDis, mCamera->getNearClippingPlane() * 2, mCamera->getFarClippingPlane() / 2);

        //        // update target AND lookAtDis
        //        mCamera->setPosition(mCamera->getTarget() - mCamera->getForwardDirection() * camDis);
        //        mCamera->setLookAtDistance(camDis);
    }

    return false;
}

bool GlfwApp::onMouseEnter()
{
    return false;
}

bool GlfwApp::onMouseExit()
{
    return false;
}

bool GlfwApp::onFocusGain()
{
    return false;
}

bool GlfwApp::onFocusLost()
{
    return false;
}

bool GlfwApp::onFileDrop(const std::vector<std::string>& files)
{
#if GLOW_EXTRAS_HAS_NANOGUI
    if (mGui == Gui::NanoGui && mNanoScreen->dropEvent(files))
        return true;
#endif

    return false;
}

void GlfwApp::onResetView()
{
    mCamera->setLookAt(-glow::transform::Forward() * mCamera->getLookAtDistance(), {0, 0, 0});
}

void GlfwApp::mainLoop()
{
    // Loop until the user closes the window
    int frames = 0;
    double lastTime = glfwGetTime();
    double lastStatsTime = lastTime;
    double lastScheduledUpdateTime = lastTime;
    double timeAccum = 0.000001;
    mCurrentTime = 0.0;
    size_t primitives = 0;
    size_t fragments = 0;
    double cpuRenderTime = 0;
    int updatesPerFrame = 1;
    double renderTimestep = 1.0 / mUpdateRate; // tracks time between renders, init is for first frame only
    while (!shouldClose())
    {
        updateInput();

        // Update
        {
            double dt = 1.0 / mUpdateRate;

            // # of updates
            auto updates = updatesPerFrame;
            if (timeAccum > updatesPerFrame * dt) // lags one behind: do one more
                ++updates;
            if (timeAccum < -dt) // is more than one ahead: skip one
                --updates;

            // do updates
            for (auto i = 0; i < updates; ++i)
            {
                update(float(dt));
                timeAccum -= dt;
                mCurrentTime += dt;
            }

            // update adjustment (AFTER updates! timeAccum should be within -dt..dt now)
            if (timeAccum > 2.5 * dt)
            {
                ++updatesPerFrame;
                // glow::info() << "increasing frames per sec";
            }
            else if (timeAccum < -2.5 * dt)
            {
                if (updatesPerFrame > 0)
                    --updatesPerFrame;
                // glow::info() << "decreasing frames per sec";
            }

            // frameskip
            if (timeAccum > mMaxFrameSkip * dt)
            {
                glow::warning() << "Too many updates queued, frame skip of " << timeAccum << " secs";
                timeAccum = mMaxFrameSkip * dt * 0.5;
            }

            // glow::info() << updates << ", " << timeAccum / dt;
        }

        // Camera Update
        if (mCamera && mUseDefaultCameraHandling)
        {
            auto doCameraHandling = true;
#if GLOW_EXTRAS_HAS_IMGUI
            if (mGui == Gui::ImGui)
            {
                if (ImGui::GetIO().WantCaptureMouse || ImGui::GetIO().WantCaptureKeyboard)
                    doCameraHandling = false;
            }
#endif

            if (doCameraHandling)
                mCamera->update(static_cast<float>(renderTimestep), mInput, mWASDController, mLookAroundController, mTargetOrbitController);
        }

        if (!mMinimized)
        {
            beginRender();

            // Render here
            {
                if (mPrimitiveQueryStats && mPrimitiveQuery)
                {
                    mPrimitiveQuery->begin();
                    mOcclusionQuery->begin();
                }

                {
                    auto cpuStart = glfwGetTime();
                    auto timerScope = mGpuTimer->scope();

                    render(float(renderTimestep));

                    cpuRenderTime += glfwGetTime() - cpuStart;
                }

                if (mPrimitiveQueryStats && mPrimitiveQuery)
                {
                    mPrimitiveQuery->end();
                    mOcclusionQuery->end();
                }
            }

            // GUI
            // after render, before rendering UI
#ifdef GLOW_EXTRAS_HAS_IMGUI
            if (mGui == Gui::ImGui)
            {
                ImGui_ImplOpenGL3_NewFrame();
                ImGui_ImplGlfw_NewFrame();
                ImGui::NewFrame();

                onGui();
                internalOnGui();

                ImGui::EndFrame();
            }
#endif

            endRender();
        }

        // timing
        auto now = glfwGetTime();
        renderTimestep = now - lastTime;
        timeAccum += now - lastTime;
        lastTime = now;
        ++frames;

        if (mPrimitiveQueryStats && mPrimitiveQuery)
        {
            primitives += mPrimitiveQuery->getResult64();
            fragments += mOcclusionQuery->getResult64();
        }

        if (mScheduledUpdateInterval > 0 && lastTime > lastScheduledUpdateTime + mScheduledUpdateInterval)
        {
            scheduledUpdate();
            lastScheduledUpdateTime = lastTime;
        }

        if (mOutputStatsInterval > 0 && lastTime > lastStatsTime + mOutputStatsInterval)
        {
            double fps = frames / (lastTime - lastStatsTime);
            std::ostringstream ss;
            ss << std::setprecision(3);
            ss << "FPS: " << fps;
            ss << ", frametime: " << 1000.0 / fps << " ms";
            ss << ", CPU: " << cpuRenderTime / frames * 1000.f << " ms";
            ss << ", GPU: " << mGpuTimer->elapsedSeconds() * 1000.f << " ms";
            if (mPrimitiveQueryStats)
            {
                ss << ", primitives: " << thousandSep(primitives / frames);
                ss << ", frags: " << thousandSep(fragments / frames);
            }
            info() << ss.str();

            lastStatsTime = lastTime;
            frames = 0;
            primitives = 0;
            fragments = 0;
            cpuRenderTime = 0;
        }
    }

    // clean up
    internalCleanUp();
}

void GlfwApp::internalInit(bool createWindow)
{
    assert(sCurrApp == nullptr && "cannot run multiple apps simulatenously");
    sCurrApp = this;

    assert(mWindow == nullptr);

    // create and get glfw context
    mInternalContext = GlfwContext::current();
    mInternalContextOwner = mInternalContext == nullptr;
    if (mInternalContextOwner)
        mInternalContext = new GlfwContext;

    mWindow = mInternalContext->window();

    // Initialize Input
    mInput.init(mWindow);

    // make it visible
    if (createWindow)
    {
        mInternalContext->show(mWindowWidth, mWindowHeight);
    }

    // create secondary context
    if (mCreateSecondaryContext)
    {
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
        mSecondaryContext = glfwCreateWindow(15, 15, "Loader Thread Context", nullptr, mWindow);
    }

    // unbind any ogl object (especially from AntTweakBar)
    glow::unbindOpenGLObjects();

    // input callbacks
    {
        glfwSetKeyCallback(mWindow, [](GLFWwindow*, int key, int scancode, int action, int mods) { sCurrApp->onKey(key, scancode, action, mods); });
        glfwSetCharModsCallback(mWindow, [](GLFWwindow*, unsigned int codepoint, int mods) { sCurrApp->onChar(codepoint, mods); });
        glfwSetMouseButtonCallback(mWindow, [](GLFWwindow*, int button, int action, int mods) {
            sCurrApp->internalOnMouseButton(sCurrApp->mInput.getMousePositionX(), sCurrApp->mInput.getMousePositionY(), button, action, mods);
        });
        glfwSetCursorEnterCallback(mWindow, [](GLFWwindow*, int entered) {
            if (entered)
                sCurrApp->onMouseEnter();
            else
                sCurrApp->onMouseExit();
        });

        glfwSetCursorPosCallback(mWindow, [](GLFWwindow*, double x, double y) { sCurrApp->onMousePosition(x, y); });
        glfwSetScrollCallback(mWindow, [](GLFWwindow*, double sx, double sy) { sCurrApp->onMouseScroll(sx, sy); });
        glfwSetFramebufferSizeCallback(mWindow, [](GLFWwindow*, int w, int h) {
            sCurrApp->mWindowWidth = w;
            sCurrApp->mWindowHeight = h;

            sCurrApp->onResize(w, h);
#if GLOW_EXTRAS_HAS_ANTTWEAKBAR
            if (sCurrApp->mGui == Gui::AntTweakBar)
                TwWindowSize(w, h);
#endif
        });
        glfwSetWindowFocusCallback(mWindow, [](GLFWwindow*, int focused) {
            if (focused)
                sCurrApp->onFocusGain();
            else
                sCurrApp->onFocusLost();
        });
        glfwSetDropCallback(mWindow, [](GLFWwindow*, int count, const char** paths) {
            std::vector<std::string> files;
            for (auto i = 0; i < count; ++i)
                files.push_back(paths[i]);
            sCurrApp->onFileDrop(files);
        });
        glfwSetWindowIconifyCallback(mWindow, [](GLFWwindow*, int iconified) { sCurrApp->mMinimized = static_cast<bool>(iconified); });
    }

    // init app
    init();

#ifdef GLOW_EXTRAS_HAS_IMGUI
    if (mEnableDebugOverlay && mGui == Gui::ImGui)
    {
        debugging::DebugOverlay::Init();
    }
#endif

    glfwGetFramebufferSize(mWindow, &mWindowWidth, &mWindowHeight);
    onResize(mWindowWidth, mWindowHeight);

#if GLOW_EXTRAS_HAS_NANOGUI
    if (mGui == Gui::NanoGui)
        mNanoScreen->performLayout();
#endif
}

void GlfwApp::internalCleanUp()
{
    if (mIsCleaned)
        return;
    mIsCleaned = true;

    onClose();

    // cleanup UI
    switch (mGui)
    {
    // anttweakbar
    case Gui::AntTweakBar:
#ifdef GLOW_EXTRAS_HAS_ANTTWEAKBAR
        TwTerminate();
#endif
        break;

    // nanogui
    case Gui::NanoGui:
#ifdef GLOW_EXTRAS_HAS_NANOGUI
        mNanoForm = nullptr;
        mNanoScreen = nullptr;
        nanogui::shutdown();
#endif
        break;

    // imgui
    case Gui::ImGui:
#ifdef GLOW_EXTRAS_HAS_IMGUI
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
#endif
        break;

    case Gui::None:
        break;
    }

    // aion dump
#ifdef GLOW_EXTRAS_HAS_AION
    if (mDumpTimingsOnShutdown)
        aion::ActionAnalyzer::dumpSummary(std::cout, false);
#endif

    // remove close flag
    glfwSetWindowShouldClose(mWindow, GLFW_FALSE);

    // clear callbacks
    glfwSetKeyCallback(mWindow, nullptr);
    glfwSetCharModsCallback(mWindow, nullptr);
    glfwSetMouseButtonCallback(mWindow, nullptr);
    glfwSetCursorEnterCallback(mWindow, nullptr);

    glfwSetCursorPosCallback(mWindow, nullptr);
    glfwSetScrollCallback(mWindow, nullptr);
    glfwSetFramebufferSizeCallback(mWindow, nullptr);
    glfwSetWindowFocusCallback(mWindow, nullptr);
    glfwSetDropCallback(mWindow, nullptr);

    // release context
    if (mInternalContextOwner)
        delete mInternalContext;
    else
        mInternalContext->hide();

    sCurrApp = nullptr;
}

void GlfwApp::internalOnMouseButton(double x, double y, int button, int action, int mods)
{
    // check double click
    if (distance(mClickPos, glm::vec2(x, y)) > 5) // too far
        mClickCount = 0;
    if (mClickTimer.elapsedSecondsD() > mDoubleClickTime) // too slow
        mClickCount = 0;
    if (mClickButton != button) // wrong button
        mClickCount = 0;

    mClickTimer.restart();
    mClickButton = button;
    mClickPos = {x, y};
    mClickCount++;

    onMouseButton(x, y, button, action, mods, mClickCount);
}

void GlfwApp::internalOnGui()
{
    if (mEnableDebugOverlay)
        debugging::DebugOverlay::OnGui();
}

void GlfwApp::updateInput()
{
    // update cursor mode
    switch (mCursorMode)
    {
    case CursorMode::Normal:
        glfwSetInputMode(mWindow, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        break;
    case CursorMode::Hidden:
        glfwSetInputMode(mWindow, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
        break;
    case CursorMode::Disabled:
        glfwSetInputMode(mWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        break;
    }

    // Poll for and process events
    glfwPollEvents();
    mInput.pollInputs();
}

void GlfwApp::beginRender()
{
    // vsync
    glfwSwapInterval(mVSync ? mSwapInterval : 0);

    // viewport
    glViewport(0, 0, mWindowWidth, mWindowHeight);
}

void GlfwApp::endRender()
{
    if (mDrawGui)
    {
        // draw the tweak bar(s)
#ifdef GLOW_EXTRAS_HAS_ANTTWEAKBAR
        if (mGui == Gui::AntTweakBar)
        {
            TwDraw();

            // unbind TweakBar stuff
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, 0);
            glUseProgram(0);
            glBindVertexArray(0);
        }
#endif

        // draw nano GUI
#ifdef GLOW_EXTRAS_HAS_NANOGUI
        if (mGui == Gui::NanoGui)
        {
            mNanoScreen->drawContents();
            mNanoScreen->drawWidgets();
            glDisable(GL_BLEND);
        }
#endif

        // draw imgui
#ifdef GLOW_EXTRAS_HAS_IMGUI
        if (mGui == Gui::ImGui)
        {
            ImGui::Render();
            glViewport(0, 0, getWindowWidth(), getWindowHeight());
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        }
#endif
    }

    // Swap front and back buffers
    glfwSwapBuffers(mWindow);
}

void GlfwApp::sleepSeconds(double seconds) const
{
    if (seconds <= 0.0)
        return;

    std::this_thread::sleep_for(std::chrono::microseconds(static_cast<int64_t>(seconds * 1000 * 1000)));
}

void GlfwApp::toggleFullscreen()
{
    if (mFullscreenActive)
    {
        glfwSetWindowAttrib(mWindow, GLFW_DECORATED, GL_TRUE);
        glfwSetWindowMonitor(mWindow, nullptr, mCachedWindowPosX, mCachedWindowPosY, mCachedWindowWidth, mCachedWindowHeight, GLFW_DONT_CARE);

        mFullscreenActive = false;
    }
    else
    {
        mCachedWindowWidth = mWindowWidth;
        mCachedWindowHeight = mWindowHeight;
        glfwGetWindowPos(mWindow, &mCachedWindowPosX, &mCachedWindowPosY);

        GLFWmonitor* primaryMonitor = glfwGetPrimaryMonitor();
        const GLFWvidmode* videoMode = glfwGetVideoMode(primaryMonitor);

        glfwSetWindowAttrib(mWindow, GLFW_DECORATED, GL_FALSE);
        glfwSetWindowAttrib(mWindow, GLFW_AUTO_ICONIFY, GL_FALSE);
        glfwSetWindowMonitor(mWindow, nullptr, 0, 0, videoMode->width, videoMode->height, GLFW_DONT_CARE);

        mFullscreenActive = true;
    }
}

void GlfwApp::run()
{
    internalInit();
    mainLoop();
}

void GlfwApp::startHeadless()
{
    internalInit(false);
}

GlfwApp::~GlfwApp()
{
    internalCleanUp();
}
