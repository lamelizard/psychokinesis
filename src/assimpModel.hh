#pragma once

#include <memory>
#include <string>
#include <vector>

#include <glow/fwd.hh>

#include <glm/glm.hpp>

// https://www.khronos.org/opengl/wiki/Skeletal_Animation
class AssimpModel
{
public:
    glm::vec3 aabbMin;
    glm::vec3 aabbMax;
    const std::string filename;

private:
    struct VertexData
    {
        std::vector<glm::vec3> positions;
        std::vector<glm::vec3> normals;
        std::vector<glm::vec3> tangents;

        // one vector per channel
        std::vector<std::vector<glm::vec2>> texCoords;
        std::vector<std::vector<glm::vec4>> colors; // currently unused

        std::vector<uint32_t> indices;
    };
    std::unique_ptr<VertexData> vertexData; // save it here before creating va stuff
    glow::SharedVertexArray va;


public:
    static std::shared_ptr<AssimpModel> load(const std::string& filename); // safe to do in a thread
    void draw();                                                           // glow::Program should be active

private:
    AssimpModel(const std::string& filename) : filename(filename) {}
    void createVertexArray(); // once on GL thread
};