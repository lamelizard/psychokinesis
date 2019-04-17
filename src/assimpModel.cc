#include "assimpModel.hh"

#include <fstream>

#include <glm/glm.hpp>

#include <glow/objects/Program.hh>
#include <glow/common/log.hh>
#include <glow/objects/ArrayBuffer.hh>
#include <glow/objects/ElementArrayBuffer.hh>
#include <glow/objects/VertexArray.hh>

#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/Importer.hpp>

using namespace glow;

// from glow-extras:
static glm::vec3 aiCast(aiVector3D const& v)
{
    return {v.x, v.y, v.z};
}
static glm::vec4 aiCast(aiColor4D const& v)
{
    return {v.r, v.g, v.b, v.a};
}

// mostly from glow-extras:
std::shared_ptr<AssimpModel> AssimpModel::load(const std::string& filename)
{
    auto model = std::make_shared<AssimpModel>(filename);
    model->vertexData = std::make_unique<VertexData>();

    if (!std::ifstream(filename).good())
    {
        error() << "Error loading `" << filename << "' with Assimp.";
        error() << "  File not found/not readable";
        return nullptr;
    }

    uint32_t flags = aiProcess_SortByPType | aiProcess_Triangulate //
                     | aiProcess_CalcTangentSpace                  //
                     | aiProcess_GenSmoothNormals                  // if not there
                     | aiProcess_GenUVCoords                       //
                     | aiProcess_PreTransformVertices              //
        ;

    Assimp::Importer importer;
    auto scene = importer.ReadFile(filename, flags);

    if (!scene)
    {
        error() << "Error loading `" << filename << "' with Assimp.";
        error() << "  " << importer.GetErrorString();
        return nullptr;
    }

    if (!scene->HasMeshes())
    {
        error() << "File `" << filename << "' has no meshes.";
        return nullptr;
    }

    glm::vec3& aabbMin = model->aabbMin;
    glm::vec3& aabbMax = model->aabbMax;

    std::vector<glm::vec3>& positions = model->vertexData->positions;
    std::vector<glm::vec3>& normals = model->vertexData->normals;
    std::vector<glm::vec3>& tangents = model->vertexData->tangents;

    // one vector per channel
    std::vector<std::vector<glm::vec2>>& texCoords = model->vertexData->texCoords;
    std::vector<std::vector<glm::vec4>>& colors = model->vertexData->colors;

    std::vector<uint32_t>& indices = model->vertexData->indices;


    auto baseIdx = 0u;
    for (auto i = 0u; i < scene->mNumMeshes; ++i)
    {
        auto const& mesh = scene->mMeshes[i];
        auto colorsCnt = mesh->GetNumColorChannels();
        auto texCoordsCnt = mesh->GetNumUVChannels();

        if (texCoordsCnt > 1)
        {
            warning() << "File `" << filename << "':";
            warning() << "  contains " << texCoordsCnt << " texture coordinates";
        }

        if (texCoords.empty())
            texCoords.resize(texCoordsCnt);
        else if (texCoords.size() != texCoordsCnt)
        {
            error() << "File `" << filename << "':";
            error() << "  contains inconsistent texture coordinate counts";
            return nullptr;
        }

        if (colors.empty())
            colors.resize(colorsCnt);
        else if (colors.size() != colorsCnt)
        {
            error() << "File `" << filename << "':";
            error() << "  contains inconsistent vertex color counts";
            return nullptr;
        }

        // add faces
        auto fCnt = mesh->mNumFaces;
        for (auto f = 0u; f < fCnt; ++f)
        {
            auto const& face = mesh->mFaces[f];
            if (face.mNumIndices != 3)
            {
                error() << "File `" << filename << "':.";
                error() << "  non-3 faces not implemented/supported";
                return nullptr;
            }

            for (auto fi = 0u; fi < face.mNumIndices; ++fi)
            {
                indices.push_back(baseIdx + face.mIndices[fi]);
            }
        }

        aabbMin = glm::vec3(+std::numeric_limits<float>().infinity());
        aabbMax = glm::vec3(-std::numeric_limits<float>().infinity());

        // add vertices
        auto vCnt = mesh->mNumVertices;
        for (auto v = 0u; v < vCnt; ++v)
        {
            auto const vertexPosition = aiCast(mesh->mVertices[v]);

            aabbMin = glm::min(aabbMin, vertexPosition);
            aabbMax = glm::max(aabbMax, vertexPosition);

            positions.push_back(vertexPosition);

            assert(mesh->HasNormals());
            normals.push_back(aiCast(mesh->mNormals[v]));
            assert(mesh->HasTangentsAndBitangents());
            tangents.push_back(aiCast(mesh->mTangents[v]));

            for (auto t = 0u; t < texCoordsCnt; ++t)
                texCoords[t].push_back((glm::vec2)aiCast(mesh->mTextureCoords[t][v]));
            for (auto t = 0u; t < colorsCnt; ++t)
                colors[t].push_back(aiCast(mesh->mColors[t][v]));
        }

        baseIdx = static_cast<unsigned>(positions.size());
    }

    return model;
}

void AssimpModel::draw() {
    assert(Program::getCurrentProgram() != nullptr);
    va->bind().draw();
}

// mostly from glow-extras:
void AssimpModel::createVertexArray()
{
    if (va)
        return;
    assert(vertexData);

    std::vector<SharedArrayBuffer> abs;


    {
        // positions
        assert(!vertexData->positions.empty());
        auto ab = ArrayBuffer::create();
        ab->defineAttribute<glm::vec3>("aPosition");
        ab->bind().setData(vertexData->positions);
        abs.push_back(ab);
    }
    assert(!vertexData->normals.empty())
    {
        auto ab = ArrayBuffer::create();
        ab->defineAttribute<glm::vec3>("aNormal");
        ab->bind().setData(vertexData->normals);
        abs.push_back(ab);
    }
    if (!vertexData->tangents.empty())
    {
        auto ab = ArrayBuffer::create();
        ab->defineAttribute<glm::vec3>("aTangent");
        ab->bind().setData(vertexData->tangents);
        abs.push_back(ab);
    }
    for (auto i = 0u; i < vertexData->texCoords.size(); ++i)
    {
        auto ab = ArrayBuffer::create();
        if (i == 0)
            ab->defineAttribute<glm::vec2>("aTexCoord");
        else
            ab->defineAttribute<glm::vec2>("aTexCoord" + std::to_string(i + 1));
        ab->bind().setData(vertexData->texCoords[i]);
        abs.push_back(ab);
    }

    for (auto const& ab : abs)
        ab->setObjectLabel(ab->getAttributes()[0].name + " of " + filename);

    auto eab = ElementArrayBuffer::create(vertexData->indices);
    eab->setObjectLabel(filename);
    auto va = VertexArray::create(abs, eab, GL_TRIANGLES);
    va->setObjectLabel(filename);

    // remove data since it's now on GPU
    vertexData.release();
}
