#include "Input.hh"

#include <cassert>
#include <iterator>
#include <utility>

#include <GLFW/glfw3.h>

#include <glow/common/log.hh>

namespace glow
{
namespace input
{
namespace detail
{
std::vector<std::pair<Key, int>> const keyGlfwIndices = {
    {Key::W, GLFW_KEY_W},
    {Key::A, GLFW_KEY_A},
    {Key::S, GLFW_KEY_S},
    {Key::D, GLFW_KEY_D},
    {Key::LShift, GLFW_KEY_LEFT_SHIFT},
    {Key::RShift, GLFW_KEY_RIGHT_SHIFT},
    {Key::Q, GLFW_KEY_Q},
    {Key::E, GLFW_KEY_E},
    {Key::Space, GLFW_KEY_SPACE},
    {Key::LAlt, GLFW_KEY_LEFT_ALT},
    {Key::RAlt, GLFW_KEY_RIGHT_ALT},
    {Key::LCtrl, GLFW_KEY_LEFT_CONTROL},
    {Key::RCtrl, GLFW_KEY_RIGHT_CONTROL},

    {Key::F1, GLFW_KEY_F1},
    {Key::F2, GLFW_KEY_F2},
    {Key::F3, GLFW_KEY_F3},
    {Key::F4, GLFW_KEY_F4},
    {Key::F5, GLFW_KEY_F5},
    {Key::F6, GLFW_KEY_F6},
    {Key::F7, GLFW_KEY_F7},
    {Key::F8, GLFW_KEY_F8},
    {Key::F9, GLFW_KEY_F9},
    {Key::F10, GLFW_KEY_F10},
    {Key::F11, GLFW_KEY_F11},
    {Key::F12, GLFW_KEY_F12},

    {Key::Home, GLFW_KEY_HOME},
};

std::vector<std::pair<Button, int>> const buttonGlfwIndices = {
    {Button::LMB, GLFW_MOUSE_BUTTON_LEFT},
    {Button::RMB, GLFW_MOUSE_BUTTON_RIGHT},
    {Button::MB4, GLFW_MOUSE_BUTTON_4},
    {Button::MB5, GLFW_MOUSE_BUTTON_5},
};

}
}
}

void glow::input::Input::init(GLFWwindow* window)
{
    mWindow = window;
}

void glow::input::Input::pollInputs()
{
    assert(mWindow != nullptr && "Input::pollInputs called before initialization");

    // Keys and Buttons
    {
        mPreviouslyHeldKeys = mHeldKeys;
        mPreviouslyHeldButtons = mHeldButtons;

        mHeldKeys.clear();
        mHeldButtons.clear();
        mPressedKeys.clear();
        mPressedButtons.clear();

        for (auto const& keyPair : detail::keyGlfwIndices)
        {
            if (glfwGetKey(mWindow, keyPair.second) == GLFW_PRESS)
            {
                // The Key is held
                mHeldKeys.insert(keyPair.first);

                // If it wasn't held last time, it is considered pressed
                if (mPreviouslyHeldKeys.find(keyPair.first) == mPreviouslyHeldKeys.end())
                    mPressedKeys.insert(keyPair.first);
            }
        }

        for (auto const& buttonPair : detail::buttonGlfwIndices)
        {
            if (glfwGetMouseButton(mWindow, buttonPair.second) == GLFW_PRESS)
            {
                // The Button is held
                mHeldButtons.insert(buttonPair.first);

                // If it wasn't held last time, it is considered pressed
                if (mPreviouslyHeldButtons.find(buttonPair.first) == mPreviouslyHeldButtons.end())
                    mPressedButtons.insert(buttonPair.first);
            }
        }
    }

    // Mouse Position
    {
        auto const lastMouseX = mMousePositionX;
        auto const lastMouseY = mMousePositionY;

        glfwGetCursorPos(mWindow, &mMousePositionX, &mMousePositionY);

        mMouseLastDeltaX = mMousePositionX - lastMouseX;
        mMouseLastDeltaY = mMousePositionY - lastMouseY;
    }
}

bool glow::input::Input::isKeyHeld(glow::input::Key key) const
{
    return mHeldKeys.find(key) != mHeldKeys.end();
}

bool glow::input::Input::isKeyPressed(glow::input::Key key) const
{
    return mPressedKeys.find(key) != mPressedKeys.end();
}

bool glow::input::Input::isButtonHeld(glow::input::Button button) const
{
    return mHeldButtons.find(button) != mHeldButtons.end();
}

bool glow::input::Input::isButtonPressed(glow::input::Button button) const
{
    return mPressedButtons.find(button) != mPressedButtons.end();
}

glow::input::Input::action_id_t glow::input::Input::mapAction(const std::string& name, glow::input::Key key)
{
    auto const it = mActionIdMap.find(name);
    if (it == mActionIdMap.end())
    {
        // Action is new, create
        auto const newActionId = static_cast<action_id_t>(mActions.size());
        mActions.push_back(key);
        mActionIdMap.insert({name, newActionId});

        return newActionId;
    }
    else
    {
        // Action exists, remap
        auto const actionId = it->second;
        mActions[actionId] = key;

        return actionId;
    }
}

bool glow::input::Input::isActionHeld(action_id_t actionId) const
{
    assert(actionId < mActions.size() && "Attempted to look up invalid action");
    return isKeyHeld(mActions[actionId]);
}

bool glow::input::Input::isActionPressed(action_id_t actionId) const
{
    assert(actionId < mActions.size() && "Attempted to look up invalid action");
    return isKeyPressed(mActions[actionId]);
}

bool glow::input::Input::isActionHeld(const std::string& action) const
{
    return isActionHeld(getActionId(action));
}

bool glow::input::Input::isActionPressed(const std::string& action) const
{
    return isActionPressed(getActionId(action));
}

glow::input::Input::action_id_t glow::input::Input::getActionId(const std::string& name) const
{
    auto const it = mActionIdMap.find(name);
    if (it != mActionIdMap.end())
    {
        return it->second;
    }
    else
    {
        glow::error() << "Tried to retrieve nonexistant action " << name;
        return 0;
    }
}

glow::input::Key glow::input::Input::getActionKey(glow::input::Input::action_id_t actionId) const
{
    assert(actionId < mActions.size() && "Attempted to look up invalid action");
    return mActions[actionId];
}

glow::input::Key glow::input::Input::getActionKey(const std::string& action) const
{
    return getActionKey(getActionId(action));
}
