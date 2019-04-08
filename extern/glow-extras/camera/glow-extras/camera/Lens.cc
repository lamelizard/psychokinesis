#include "Lens.hh"

#include <glm/ext.hpp>

void glow::camera::Lens::updateVerticalFieldOfView()
{
    mVerticalFov = glm::degrees(atan(tan(glm::radians(0.5f * mHorizontalFov)) / mAspectRatio) * 2.0f);
    updateProjectionMatrix();
}

void glow::camera::Lens::updateProjectionMatrix()
{
    if (std::isinf(mFarPlane))
    {
        float const e = 1.0f / tan(glm::radians(mVerticalFov * 0.5f));

        // infinite Perspective matrix reversed mapping to 1..-1
        mProjectionMatrix
            = {e / mAspectRatio,   0.0f, 0.0f, 0.0f, 0.0f, e, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, -1.0f, 0.0f, 0.0f,
               -2.0f * mNearPlane, 0.0f};
    }
    else
    {
        mProjectionMatrix = glm::perspective(glm::radians(mHorizontalFov), mAspectRatio, mNearPlane, mFarPlane);
    }
}

glow::camera::Lens::Lens()
{
    updateVerticalFieldOfView();
}

void glow::camera::Lens::setNearPlane(float nearPlane)
{
    mNearPlane = nearPlane;
    updateProjectionMatrix();
}

void glow::camera::Lens::setFarPlane(float farPlane)
{
    mFarPlane = farPlane;
    updateProjectionMatrix();
}

void glow::camera::Lens::setFoV(float fov)
{
    mHorizontalFov = fov;
    updateVerticalFieldOfView();
}

void glow::camera::Lens::setViewportSize(unsigned w, unsigned h)
{
    mViewportSize = glm::uvec2(w, h);
    mAspectRatio = static_cast<float>(w) / static_cast<float>(h);
    updateProjectionMatrix();
}
