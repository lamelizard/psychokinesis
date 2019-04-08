#include "CameraUtils.hh"

glow::camera::Ray glow::camera::getViewRay(glow::camera::CameraBase &cam, const glm::vec2 &mousePosition)
{
    glm::vec3 ps[2];
    auto i = 0;
    for (auto d : {0.5f, -0.5f})
    {
        glm::vec4 v{mousePosition.x * 2 - 1, 1 - mousePosition.y * 2, d * 2 - 1, 1.0};

        v = glm::inverse(cam.getProjectionMatrix()) * v;
        v /= v.w;
        v = glm::inverse(cam.getViewMatrix()) * v;
        ps[i++] = glm::vec3(v);
    }

    return Ray{cam.getPosition(), normalize(ps[0] - ps[1])};
}
