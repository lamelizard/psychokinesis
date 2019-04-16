#pragma once

#include <memory>
#include <string>

#include <glow/fwd.hh>

#include <glm/glm.hpp>

//https://www.khronos.org/opengl/wiki/Skeletal_Animation
class AssimpModel
{
public:
    struct Vertex
    {
        glm::vec3 pos;
        glm::vec3 normal;
        glm::vec3 tangent;
        glm::vec2 texcoord;
        glm::vec4 bones;//could optimize to short?
        glm::vec4 boneweights;
    };

public:
    static std::shared_ptr<AssimpModel> load(const std::string& filename);


private:
    AssimpModel();
};