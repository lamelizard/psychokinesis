#pragma once

#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <glm/vec2.hpp>

#include <glow/common/non_copyable.hh>

struct GLFWwindow;

namespace glow
{
namespace input
{
enum class Key
{
    W,
    A,
    S,
    D,
    LShift,
    RShift,
    Q,
    E,
    Space,
    LAlt,
    RAlt,
    LCtrl,
    RCtrl,
    F1,
    F2,
    F3,
    F4,
    F5,
    F6,
    F7,
    F8,
    F9,
    F10,
    F11,
    F12,
    Home,
};

enum class Button
{
    LMB,
    RMB,
    MB4,
    MB5
};

class Input final
{
public:
    using action_id_t = unsigned;

private:
    using key_set = std::unordered_set<Key>;
    using btn_set = std::unordered_set<Button>;

    GLFWwindow* mWindow = nullptr;

    key_set mPreviouslyHeldKeys; ///< Keys that have been held down in the previous polling interval
    key_set mHeldKeys;           ///< Keys that are held down
    key_set mPressedKeys;        ///< Keys that are pressed (Held && !Previously held)

    btn_set mPreviouslyHeldButtons; ///< Buttons that have been held down in the previous polling interval
    btn_set mHeldButtons;           ///< Buttons that are held down
    btn_set mPressedButtons;        ///< Buttons that are pressed (Held && !Previously held)

    double mMousePositionX;
    double mMousePositionY;

    double mMouseLastDeltaX;
    double mMouseLastDeltaY;

private:
    // -- Actions --

    std::unordered_map<std::string, action_id_t> mActionIdMap; ///< Maps the action name to its index in mActions
    std::vector<Key> mActions;                                 ///< Actions, accessed by their indices

private:
    GLOW_NON_COPYABLE(Input);

public:
    Input() = default;
    void init(GLFWwindow* window);

    void pollInputs();

    bool isKeyHeld(Key key) const;
    bool isKeyPressed(Key key) const;

    bool isButtonHeld(Button button) const;
    bool isButtonPressed(Button button) const;

    double getMousePositionX() const { return mMousePositionX; }
    double getMousePositionY() const { return mMousePositionY; }

    glm::vec2 getMousePosition() const { return {mMousePositionX, mMousePositionY}; }
    glm::vec2 getLastMouseDelta() const { return {mMouseLastDeltaX, mMouseLastDeltaY}; }

public:
    // -- Actions --

    /// Maps an action to a key, returns its id
    action_id_t mapAction(std::string const& name, Key key);

    /// Query action state by id
    bool isActionHeld(action_id_t actionId) const;
    bool isActionPressed(action_id_t actionId) const;

    /// Query action state by name, slightly slower
    bool isActionHeld(std::string const& action) const;
    bool isActionPressed(std::string const& action) const;

    /// Returns id of the given action
    action_id_t getActionId(std::string const& name) const;

    /// Returns key the given action is mapped to
    Key getActionKey(action_id_t actionId) const;
    Key getActionKey(std::string const& action) const;
};

}
}
