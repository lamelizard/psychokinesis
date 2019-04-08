#pragma once

#include <glow/math/transform.hh>

#include <glow-extras/input/Input.hh>

#include "CameraBase.hh"
#include "Handle.hh"
#include "Lens.hh"

namespace glow
{
namespace camera
{
/**
 * The Camera
 *
 * Consists of
 *  - the Lens (Projection)
 *  - the Handle (Positionig, View Matrix)
 *
 */
GLOW_SHARED(class, Camera);
class Camera : public CameraBase
{
public:
    Lens lens;
    Handle handle;

public:
    Camera() = default;
    GLOW_SHARED_CREATOR(Camera)

public:
    // -- CameraBase interface --
    glm::vec3 getPosition() const override { return handle.getPosition(); }
    glm::mat4 getViewMatrix() const override { return handle.getViewMatrix(); }
    glm::mat4 getProjectionMatrix() const override { return lens.getProjectionMatrix(); }
    glm::uvec2 getViewportSize() const override { return lens.getViewportSize(); }
    float getNearClippingPlane() const override { return lens.getNearPlane(); }
    float getFarClippingPlane() const override { return lens.getFarPlane(); }
    float getVerticalFieldOfView() const override { return lens.getVerticalFov(); }
    float getHorizontalFieldOfView() const override { return lens.getHorizontalFov(); }

public:
    // -- Handle pass-through --
    glow::transform const& transform() const { return handle.getTransform(); }
    void setPosition(glm::vec3 const& pos) { handle.setPosition(pos); }
    void setLookAt(glm::vec3 const& pos, glm::vec3 const& target) { handle.setLookAt(pos, target); }
    float getLookAtDistance() const { return handle.getLookAtDistance(); }
    glm::vec3 getRightVector() const { return transform().getRightVector(); }
    glm::vec3 getForwardVector() const { return transform().getForwardVector(); }
    glm::vec3 getUpVector() const { return transform().getUpVector(); }

    // -- Lens pass-through --
    void setNearPlane(float nearPlane) { lens.setNearPlane(nearPlane); }
    void setFarPlane(float farPlane) { lens.setFarPlane(farPlane); }
    void setFoV(float fov) { lens.setFoV(fov); }
    void setViewportSize(unsigned w, unsigned h) { lens.setViewportSize(w, h); }
    void setViewportSize(int w, int h) { setViewportSize(static_cast<unsigned>(w), static_cast<unsigned>(h)); }
    unsigned getViewportWidth() const { return lens.getViewportSize().x; }
    unsigned getViewportHeight() const { return lens.getViewportSize().y; }

public:
    void update(float dt) { handle.update(dt); }

    // -- Controller update --
    template <class... Controllers>
    void update(float dt, glow::input::Input const& input, Controllers&... controllers)
    {
        applyControllers(dt, input, controllers...);
        handle.update(dt);
    }

private:
    template <class Controller, class... Controllers>
    void applyControllers(float dt, glow::input::Input const& input, Controller& controller, Controllers&... controllers)
    {
        controller.update(dt, input, *this);
        applyControllers(dt, input, controllers...);
    }

    void applyControllers(float, glow::input::Input const&)
    {
        // noop, recursion end
    }
};
}
}
