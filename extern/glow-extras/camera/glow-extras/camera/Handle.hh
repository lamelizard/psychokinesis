#pragma once

#include <glm/glm.hpp>

#include <glow/math/transform.hh>

namespace glow
{
namespace camera
{
/**
 * The Handle
 *
 * Component of the camera responsible for position and orientation
 */
class Handle
{
private:
    enum class SmoothingMode
    {
        FPS,
        Orbit
    };

    struct HandleState
    {
        glow::transform transform = glow::transform::Identity;
        glm::vec3 forward{1, 0, 0};
        glm::vec3 target{0};
        float targetDistance = 20.f;
    };

    HandleState mPhysical;
    HandleState mTarget;

    SmoothingMode mSmoothingMode = SmoothingMode::FPS;

public:
    /// Smoothing sensitivities per category
    struct
    {
        float distance = 10.f;
        float position = 25.f;
        float rotation = 35.f;
        float target = 25.f;
    } sensitivity;

private:
    static glm::quat forwardToRotation(glm::vec3 const& forward, glm::vec3 const& up = glow::transform::Up());

public:
    Handle(glow::transform const& transform = glow::transform::Identity);

    /// Performs smoothing
    void update(float dt);

    void snap() { mPhysical = mTarget; }  ///< Snap to the target state
    void abort() { mTarget = mPhysical; } ///< Abort smoothing to the target state

public:
    // -- Manipulators --
    void move(glm::vec3 const& distance);
    void setPosition(glm::vec3 const& pos);
    void setLookAt(glm::vec3 const& pos, glm::vec3 const& target);
    void setTarget(glm::vec3 const& target);
    void setTargetDistance(float dist);
    void addTargetDistance(float deltaDist);

    // -- Input-based manipulators --
    /// Rotate on the spot, like a FPS camera
    void lookAround(float dX, float dY);
    /// Orbit around the current target
    void orbit(float dX, float dY);

public:
    // -- Getters --
    glm::mat4 getViewMatrix() const;
    glm::vec3 const& getPosition() const { return mPhysical.transform.position; }
    glm::vec3 const& getTarget() const { return mPhysical.target; }
    float getLookAtDistance() const { return mPhysical.targetDistance; }

    glow::transform const& getTransform() const { return mPhysical.transform; }
};
}
}
