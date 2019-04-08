#pragma once

#include <glm/glm.hpp>

#include "CameraBase.hh"

namespace glow
{
namespace camera
{
struct Ray
{
    glm::vec3 start;
    glm::vec3 dir;
};

Ray getViewRay(CameraBase& cam, glm::vec2 const& mousePosition);
}
}
