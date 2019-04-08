#pragma once

#include "../Camera.hh"

namespace glow
{
namespace camera
{
/**
 * Lets the camera look around while standing still, FPS-style
 * Hold RMB - Look around
 */
class LookAroundController
{
private:
    float mCameraTurnSpeed = 5.f;

public:
    void update(float, glow::input::Input const& input, Camera& camera) const
    {
        if (input.isButtonHeld(input::Button::RMB))
        {
            float dX = input.getLastMouseDelta().x / camera.getViewportWidth() * mCameraTurnSpeed;
            float dY = input.getLastMouseDelta().y / camera.getViewportHeight() * mCameraTurnSpeed;

            camera.handle.lookAround(dX, dY);
        }
    }

    void setTurnSpeed(float speed) { mCameraTurnSpeed = speed; }
};
}
}
