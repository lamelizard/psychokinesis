#pragma once

#include "../Camera.hh"

namespace glow
{
namespace camera
{
/**
 * Controls a free, flying camera
 * WASD - Forward/Backward, Left/Right
 * QE - Up/Down
 * Hold Shift - Increase Speed
 */
class WASDController
{
private:
    float mCameraMoveSpeed = 30.f;
    float mShiftSpeedMultiplier = 4.f;

public:
    void update(float dt, glow::input::Input const& input, Camera& camera) const
    {
        auto distance = mCameraMoveSpeed * dt;

        if (input.isKeyHeld(input::Key::LShift))
            distance *= mShiftSpeedMultiplier;

        glm::vec3 distanceInput = glm::vec3(0);

        if (input.isKeyHeld(input::Key::S))
            ++distanceInput.z;
        if (input.isKeyHeld(input::Key::W))
            --distanceInput.z;
        if (input.isKeyHeld(input::Key::D))
            ++distanceInput.x;
        if (input.isKeyHeld(input::Key::A))
            --distanceInput.x;
        if (input.isKeyHeld(input::Key::E))
            ++distanceInput.y;
        if (input.isKeyHeld(input::Key::Q))
            --distanceInput.y;

        camera.handle.move(distanceInput * distance);
    }

    void setCameraSpeed(float speed) { mCameraMoveSpeed = speed; }
    void setShiftSpeedMultiplier(float mult) { mShiftSpeedMultiplier = mult; }
};
}
}
