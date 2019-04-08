#pragma once

#include <vector>

#include <glow/common/property.hh>
#include <glow/common/shared.hh>

#include <glow/objects/ArrayBufferAttribute.hh>

#include <glow/fwd.hh>

#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

namespace glow
{
namespace debugging
{
/**
 * @brief The DebugRenderer is a easy-to-use (but not so performant) interface for easy debug rendering
 *
 * The DebugRenderer provides a set of functions for drawing primitives:
 *   - drawLine(...)
 *   - drawCube(...)
 *
 * The DebugRenderer accumulates all primitives, unless DebugRenderer::clear() is called
 */
class DebugRenderer
{
private:
    struct PrimitiveVertex
    {
        glm::vec3 pos;

        PrimitiveVertex() = default;
        PrimitiveVertex(float u, float v) : pos(u, v, 0.0) {}
        PrimitiveVertex(glm::vec3 pos) : pos(pos) {}
        PrimitiveVertex(glm::vec3 position, glm::vec3, glm::vec3, glm::vec2) : pos(position) {}
        static std::vector<ArrayBufferAttribute> attributes()
        {
            return {
                {&PrimitiveVertex::pos, "aPosition"}, //
            };
        }
    };

    struct Primitive
    {
        SharedVertexArray vao;
        glm::vec3 color;
        glm::mat4 modelMatrix;
    };

    SharedVertexArray mQuad;
    SharedVertexArray mCube;
    SharedVertexArray mLine;
    SharedProgram mShaderOpaque;

    std::vector<Primitive> mPrimitives;

public:
    DebugRenderer();

    /// Clears all stored primitives
    void clear();

    /// Renders all stored primitives
    void render(glm::mat4 const& vp) const;

    static void GlobalInit();

    // render functions
public:
    void renderLine(glm::vec3 start, glm::vec3 end, glm::vec3 color = glm::vec3(1.0));
    void renderAABB(glm::vec3 start, glm::vec3 end, glm::vec3 color = glm::vec3(1.0), bool wireframe = true);
    void renderAABBCentered(glm::vec3 center, glm::vec3 size, glm::vec3 color = glm::vec3(1.0), bool wireframe = true);
};
GLOW_SHARED(class, DebugRenderer);
}
}
