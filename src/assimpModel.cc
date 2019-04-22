#include "assimpModel.hh"

#include <fstream>
#include <functional>

#include <glm/glm.hpp>

#include <glow/common/log.hh>
#include <glow/objects/ArrayBuffer.hh>
#include <glow/objects/ElementArrayBuffer.hh>
#include <glow/objects/Program.hh>
#include <glow/objects/VertexArray.hh>

#include <assimp/postprocess.h>

#define MAX_BONES 64

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

static glm::mat4 aiCast(aiMatrix4x4 const& v)
{
    return glm::mat4(           //
        v.a1, v.a2, v.a3, v.a4, //
        v.b1, v.b2, v.b3, v.b4, //
        v.c1, v.c2, v.c3, v.c4, //
        v.d1, v.d2, v.d3, v.d4  //
    );
}

// mostly from glow-extras:
std::shared_ptr<AssimpModel> AssimpModel::load(const std::string& filename)
{
    std::shared_ptr<AssimpModel> model;
    try
    {
        model = std::shared_ptr<AssimpModel>(new AssimpModel(filename));
    }
    catch (...) // bad
    {
        return nullptr;
    }
    return model;
}

void AssimpModel::draw()
{
    if (!va)
        createVertexArray();
    assert(va);
    assert(Program::getCurrentProgram() != nullptr);
    va->bind().draw();
}

void AssimpModel::draw(const glow::UsedProgram& shader, double time, const std::string& animationStr)
{
    if (!va)
        createVertexArray();
    assert(va);

    auto animation = animations[animationStr]; // might throw
    glm::mat4 boneArray[MAX_BONES];
    // for (int i = 0; i < MAX_BONES; i++)
    //   boneArray[i] = glm::mat4(); // identity
    const auto fillArray = [this, &boneArray](aiNode* thisNode, aiMatrix4x4 parent, auto& fillArray) -> void {
        // https://github.com/vovan4ik123/assimp-Cpp-OpenGL-skeletal-animation/blob/master/Load_3D_model_2/Model.cpp

        auto transform = parent * thisNode->mTransformation; // does this make sense?????????????????????????????????
        if (boneIDOfNode.count(thisNode) == 1) // is node a bone?
            boneArray[boneIDOfNode[thisNode]] = aiCast(transform * offsetOfNode[thisNode]); // write this better
        for (int i = 0; i < thisNode->mNumChildren; i++)
            fillArray(thisNode->mChildren[i], transform, fillArray);
    };
    fillArray(scene->mRootNode, aiMatrix4x4(), fillArray);

    shader.setUniform("uBones[0]", MAX_BONES, boneArray);// really, uBones[0] instead of uBones...

    va->bind().draw();
}

AssimpModel::AssimpModel(const std::string& filename) : filename(filename)
{
    if (!std::ifstream(filename).good())
    {
        error() << "Error loading `" << filename << "' with Assimp.";
        error() << "  File not found/not readable";
        throw std::exception();
    }

    // https://gamedev.stackexchange.com/questions/63498/assimp-in-my-program-is-much-slower-to-import-than-assimp-view-program
    uint32_t flags = aiProcess_SortByPType                //
                     | aiProcess_Triangulate              //
                     | aiProcess_JoinIdenticalVertices    //
                     | aiProcess_ValidateDataStructure    //
                     | aiProcess_ImproveCacheLocality     //
                     | aiProcess_RemoveRedundantMaterials //
                     | aiProcess_FindDegenerates          // err, what's this?
                     | aiProcess_FindInvalidData          //
                     | aiProcess_TransformUVCoords        // yes,no?
                     //| aiProcess_FindInstances  //???
                     | aiProcess_LimitBoneWeights // 4 is max
                     | aiProcess_OptimizeMeshes   //
                     | aiProcess_CalcTangentSpace //
                     | aiProcess_GenSmoothNormals // if not there
                     | aiProcess_GenUVCoords      //
                     //| aiProcess_PreTransformVertices // removes animations...
                     | aiProcess_FlipUVs // test for mech
        ;


    scene = importer.ReadFile(filename, flags);

    if (!scene)
    {
        error() << "Error loading `" << filename << "' with Assimp.";
        error() << "  " << importer.GetErrorString();
        throw std::exception();
    }

    if (!scene->HasMeshes())
    {
        error() << "File `" << filename << "' has no meshes.";
        throw std::exception();
    }

    auto baseIdx = 0u;
    assert(scene->mNumMeshes == 1);
    // for (auto i = 0u; i < scene->mNumMeshes; ++i)
    {
        auto i = 0u;

        vertexData = std::make_unique<VertexData>();
        auto& positions = vertexData->positions;
        auto& normals = vertexData->normals;
        auto& tangents = vertexData->tangents;
        auto& bones = vertexData->bones;
        auto& boneWeights = vertexData->boneWeights;

        // one vector per channel
        std::vector<std::vector<glm::vec2>>& texCoords = vertexData->texCoords;
        std::vector<std::vector<glm::vec4>>& colors = vertexData->colors;

        auto& indices = vertexData->indices;

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
            throw std::exception();
        }

        if (colors.empty())
            colors.resize(colorsCnt);
        else if (colors.size() != colorsCnt)
        {
            error() << "File `" << filename << "':";
            error() << "  contains inconsistent vertex color counts";
            throw std::exception();
        }

        // add faces
        auto fCnt = mesh->mNumFaces;
        for (auto f = 0u; f < fCnt; ++f)
        {
            auto const& face = mesh->mFaces[f];
            if (face.mNumIndices < 3)
                continue; // ignore points and lines, problem?
            if (face.mNumIndices > 3)
            {
                error() << "File `" << filename << "':.";
                error() << "  non-3 faces not implemented/supported";
                throw std::exception();
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
        positions.reserve(vCnt);
        normals.reserve(vCnt);
        tangents.reserve(vCnt);
        bones.reserve(vCnt);
        boneWeights.reserve(vCnt);
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

            bones.push_back(glm::u16vec4(0, 0, 0, 0));
            boneWeights.push_back(glm::vec4(0, 0, 0, 0));

            for (auto t = 0u; t < texCoordsCnt; ++t)
                texCoords[t].push_back((glm::vec2)aiCast(mesh->mTextureCoords[t][v]));
            for (auto t = 0u; t < colorsCnt; ++t)
                colors[t].push_back(aiCast(mesh->mColors[t][v]));
        }

        baseIdx = static_cast<unsigned>(positions.size());

        // bones
        if (mesh->HasBones())
            for (int j = 0; j < mesh->mNumBones; j++)
            {
                auto bone = mesh->mBones[j];
                auto node = scene->mRootNode->FindNode(bone->mName);
                assert(node);
                boneIDOfNode[node] = j;
                offsetOfNode[node] = bone->mOffsetMatrix;

                // weights
                for (int k = 0; k < bone->mNumWeights; k++)
                {
                    auto vid = bone->mWeights[k].mVertexId;
                    auto weight = bone->mWeights[k].mWeight;
                    if (boneWeights[vid].x == 0)
                    {
                        bones[vid].x = j;
                        boneWeights[vid].x = weight;
                    }
                    else if (boneWeights[vid].y == 0)
                    {
                        bones[vid].y = j;
                        boneWeights[vid].y = weight;
                    }
                    else if (boneWeights[vid].z == 0)
                    {
                        bones[vid].z = j;
                        boneWeights[vid].z = weight;
                    }
                    else if (boneWeights[vid].w == 0)
                    {
                        bones[vid].w = j;
                        boneWeights[vid].w = weight;
                    }
                }
            }
    }

    // check bone number
    assert(boneIDOfNode.size() <= MAX_BONES);

    // animations
    if (scene->HasAnimations())
        for (int i = 0; i < scene->mNumAnimations; i++)
            animations[scene->mAnimations[i]->mName.C_Str()] = scene->mAnimations[i];
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
    {
        // normals
        assert(!vertexData->normals.empty());
        auto ab = ArrayBuffer::create();
        ab->defineAttribute<glm::vec3>("aNormal");
        ab->bind().setData(vertexData->normals);
        abs.push_back(ab);
    }
    {
        // tangents
        assert(!vertexData->tangents.empty());
        auto ab = ArrayBuffer::create();
        ab->defineAttribute<glm::vec3>("aTangent");
        ab->bind().setData(vertexData->tangents);
        abs.push_back(ab);
    }
    {
        // bones
        auto ab = ArrayBuffer::create();
        ab->defineAttribute<glm::vec4>("aBoneIDs");
        ab->bind().setData(vertexData->bones);
        abs.push_back(ab);
    }
    {
        // boneWeights
        auto ab = ArrayBuffer::create();
        ab->defineAttribute<glm::vec4>("aBoneWeights");
        ab->bind().setData(vertexData->boneWeights);
        abs.push_back(ab);
    }

    // texture coords
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
    va = VertexArray::create(abs, eab, GL_TRIANGLES);
    va->setObjectLabel(filename);

    // remove data since it's now on GPU
    vertexData.release();
}
