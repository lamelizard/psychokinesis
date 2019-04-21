#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

#include <glow/fwd.hh>

#include <assimp/scene.h>
#include <assimp/Importer.hpp>

#include <glm/glm.hpp>

// https://www.khronos.org/opengl/wiki/Skeletal_Animation
GLOW_SHARED(class, AssimpModel);
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
        std::vector<glm::vec4> bones;
        std::vector<glm::vec4> boneWeights;

        // one vector per channel
        std::vector<std::vector<glm::vec2>> texCoords;
        std::vector<std::vector<glm::vec4>> colors; // currently unused

        std::vector<uint32_t> indices;
    };
    std::unique_ptr<VertexData> vertexData; // save it here before creating va stuff
    glow::SharedVertexArray va;

    Assimp::Importer importer; // will delete scene on detruction?
    const aiScene* scene = nullptr;
    std::map<std::string, aiAnimation*> animations;
    std::map<aiNode*, int> boneIDOfNode;
    std::map<aiNode*, aiMatrix4x4> offsetOfNode;


public:
    static SharedAssimpModel load(const std::string& filename); // safe to do in a thread
    void draw();                                                // glow::Program should be active
    void draw(const glow::UsedProgram&, double time, const std::string& animation);

private:
    AssimpModel(const std::string& filename);
    void createVertexArray(); // once on GL thread (automatic)
};