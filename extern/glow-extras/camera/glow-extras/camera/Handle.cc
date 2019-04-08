#include "Handle.hh"

#include <algorithm>

glm::quat glow::camera::Handle::forwardToRotation(const glm::vec3 &forward, const glm::vec3 &up)
{
    auto const fwd = glm::normalize(forward);

    glm::vec3 rightVector = glm::normalize(glm::cross(fwd, up));
    glm::vec3 upVector = glm::cross(rightVector, fwd);

    glm::mat3 rotMatrix;
    rotMatrix[0][0] = rightVector.x;
    rotMatrix[0][1] = upVector.x;
    rotMatrix[0][2] = -fwd.x;
    rotMatrix[1][0] = rightVector.y;
    rotMatrix[1][1] = upVector.y;
    rotMatrix[1][2] = -fwd.y;
    rotMatrix[2][0] = rightVector.z;
    rotMatrix[2][1] = upVector.z;
    rotMatrix[2][2] = -fwd.z;

    return glm::quat(rotMatrix);
}

glow::camera::Handle::Handle(const glow::transform &transform)
{
    mTarget.transform = transform;
    snap();
}

void glow::camera::Handle::update(float dt)
{
    // Always slerp forward
    auto alphaRot = std::min(1.f, glow::transform::exponentialDecayAlpha(sensitivity.rotation, dt));
    mPhysical.forward = glm::normalize(glm::lerp(mPhysical.forward, mTarget.forward, alphaRot));

    if (mSmoothingMode == SmoothingMode::FPS)
    {
        // Snap target
        mPhysical.target = mTarget.target;

        // Lerp position
        auto alphaPos = std::min(1.f, glow::transform::exponentialDecayAlpha(sensitivity.position, dt));
        mPhysical.transform.lerpToPosition(mTarget.transform.position, alphaPos);
    }
    else // SmoothingMode::Orbit
    {
        // Lerp target
        auto alphaTarget = std::min(1.f, glow::transform::exponentialDecayAlpha(sensitivity.target, dt));
        mPhysical.target = glm::lerp(mPhysical.target, mTarget.target, alphaTarget);

        // Calculate forward
        // mPhysical.forward = glm::normalize(mPhysical.target - mPhysical.transform.position);

        // Set position
        mPhysical.transform.position = mPhysical.target - mPhysical.forward * mPhysical.targetDistance;
    }

    // Set rotation from forward vector
    mPhysical.transform.rotation = forwardToRotation(mPhysical.forward);

    // Always lerp target distance
    auto alphaDist = std::min(1.f, glow::transform::exponentialDecayAlpha(sensitivity.distance, dt));
    mPhysical.targetDistance = glm::lerp(mPhysical.targetDistance, mTarget.targetDistance, alphaDist);
}

void glow::camera::Handle::move(const glm::vec3 &distance)
{
    auto const transposedRotation = glm::transpose(glm::mat3(mPhysical.transform.rotation));
    auto const delta = transposedRotation * distance;
    mTarget.transform.position += delta;
    mTarget.target += delta;

    mSmoothingMode = SmoothingMode::FPS;
}

void glow::camera::Handle::setPosition(const glm::vec3 &pos)
{
    mTarget.transform.position = pos;
}

void glow::camera::Handle::setLookAt(glm::vec3 const &pos, const glm::vec3 &target)
{
    mTarget.transform.position = pos;
    mTarget.target = target;

    glm::vec3 forwardVector = target - pos;
    mTarget.targetDistance = glm::length(forwardVector);
    if (mTarget.targetDistance < .0001f) // in case target == position
    {
        mTarget.targetDistance = .0001f;
        forwardVector = glm::vec3(mTarget.targetDistance, 0, 0);
    }

    mTarget.forward = forwardVector / mTarget.targetDistance;
}

void glow::camera::Handle::setTarget(const glm::vec3 &target)
{
    mTarget.target = target;

    glm::vec3 forwardVector = target - mTarget.transform.position;
    mTarget.targetDistance = glm::length(forwardVector);

    if (mTarget.targetDistance < .0001f) // target ~= position
    {
        mTarget.targetDistance = .0001f;
        mTarget.forward = glm::vec3(1, 0, 0);
    }
    else
    {
        mTarget.forward = forwardVector / mTarget.targetDistance;
    }
}

void glow::camera::Handle::setTargetDistance(float dist)
{
    mTarget.targetDistance = dist;
}

void glow::camera::Handle::addTargetDistance(float deltaDist)
{
    mTarget.targetDistance += deltaDist;
}

void glow::camera::Handle::lookAround(float dX, float dY)
{
    auto altitude = glm::atan(mTarget.forward.y, length(glm::vec2(mTarget.forward.x, mTarget.forward.z)));
    auto azimuth = glm::atan(mTarget.forward.z, mTarget.forward.x);

    azimuth += dX;
    altitude = glm::clamp(altitude - dY, -0.499f * glm::pi<float>(), 0.499f * glm::pi<float>());

    auto caz = glm::cos(azimuth);
    auto saz = glm::sin(azimuth);
    auto cal = glm::cos(altitude);
    auto sal = glm::sin(altitude);

    mTarget.forward = glm::vec3(cal * caz, sal, cal * saz);
    mTarget.target = mTarget.transform.position + mTarget.forward * mTarget.targetDistance;

    mSmoothingMode = SmoothingMode::FPS;
}

void glow::camera::Handle::orbit(float dX, float dY)
{
    auto azimuth = glm::atan(mTarget.forward.z, mTarget.forward.x) + dX;
    auto altitude = glm::atan(mTarget.forward.y,
                              glm::sqrt(mTarget.forward.x * mTarget.forward.x + mTarget.forward.z * mTarget.forward.z))
                    - dY;
    altitude = glm::clamp(altitude, -0.499f * glm::pi<float>(), 0.499f * glm::pi<float>());

    auto caz = glm::cos(azimuth);
    auto saz = glm::sin(azimuth);
    auto cal = glm::cos(altitude);
    auto sal = glm::sin(altitude);

    mTarget.forward = glm::vec3(cal * caz, sal, cal * saz);
    mTarget.transform.position = mTarget.target - mTarget.forward * mTarget.targetDistance;

    mSmoothingMode = SmoothingMode::Orbit;
}

glm::mat4 glow::camera::Handle::getViewMatrix() const
{
    auto const &position = mPhysical.transform.position;
    glm::mat4 viewMatrix = glm::mat4(glm::toMat3(mPhysical.transform.rotation));
    viewMatrix[3][0] = -(viewMatrix[0][0] * position.x + viewMatrix[1][0] * position.y + viewMatrix[2][0] * position.z);
    viewMatrix[3][1] = -(viewMatrix[0][1] * position.x + viewMatrix[1][1] * position.y + viewMatrix[2][1] * position.z);
    viewMatrix[3][2] = -(viewMatrix[0][2] * position.x + viewMatrix[1][2] * position.y + viewMatrix[2][2] * position.z);

    return viewMatrix;
}
