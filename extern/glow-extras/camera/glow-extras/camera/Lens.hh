#pragma once

#include <glm/glm.hpp>

#include <glow/common/property.hh>

namespace glow
{
namespace camera
{
/**
 * The Lens
 *
 * Component of the camera responsible for projection
 */
class Lens
{
private:
    glm::uvec2 mViewportSize{1};
    float mAspectRatio = 1.f;

    glm::mat4 mProjectionMatrix{1};

    float mHorizontalFov = 80.f;
    float mVerticalFov = 0.f;

    float mNearPlane = .1f;
    float mFarPlane = 1000.f;

private:
    void updateVerticalFieldOfView();
    void updateProjectionMatrix();

public:
    Lens();

    void setNearPlane(float nearPlane);
    void setFarPlane(float farPlane);
    void setFoV(float fov);
    void setViewportSize(unsigned w, unsigned h);
    void setViewportSize(int w, int h) { setViewportSize(static_cast<unsigned>(w), static_cast<unsigned>(h)); }

    GLOW_GETTER(ViewportSize);
    GLOW_GETTER(AspectRatio);
    GLOW_GETTER(ProjectionMatrix);
    GLOW_GETTER(HorizontalFov);
    GLOW_GETTER(VerticalFov);
    GLOW_GETTER(NearPlane);
    GLOW_GETTER(FarPlane);
};
}
}
