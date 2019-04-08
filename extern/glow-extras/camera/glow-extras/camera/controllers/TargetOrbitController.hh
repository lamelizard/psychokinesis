#pragma once

#include <glow/common/log.hh>

#include "../Camera.hh"

namespace glow
{
namespace camera
{
/**
 * Lets the camera orbit around its target
 * Hold LMB - Orbit
 */
class TargetOrbitController
{
private:
    float mCameraOrbitSpeed = 10.f;

public:
    void update(float, glow::input::Input const& input, Camera& camera) const
    {
        if (input.isButtonHeld(input::Button::LMB))
        {
            float dX = input.getLastMouseDelta().x / camera.getViewportWidth() * mCameraOrbitSpeed;
            float dY = input.getLastMouseDelta().y / camera.getViewportHeight() * mCameraOrbitSpeed;

            camera.handle.orbit(dX, dY);
        }
    }

    void setOrbitSpeed(float speed) { mCameraOrbitSpeed = speed; }
};
}
}
