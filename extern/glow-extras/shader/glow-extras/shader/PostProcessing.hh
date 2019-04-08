#pragma once

#include <glm/mat3x3.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <glow/fwd.hh>

namespace glow
{
namespace shader
{
class PostProcessing
{
private:
    bool mInitialized = false;

    // shared members
private:
    glow::SharedVertexArray mQuad;

    // gradient
private:
    glow::SharedProgram mProgramGradient;
    glow::SharedProgram mProgramTextureRect;
    glow::SharedProgram mProgramOutput;
    glow::SharedProgram mProgramCopy;

public:
    void gradientRadial(glm::vec3 colorInner, glm::vec3 colorOuter, glm::vec2 center = {0.5f, 0.5f}, float radius = 0.5f);

    void drawTexture(SharedTextureRectangle const& tex, glm::vec2 start, glm::vec2 end, glm::mat4 const& colorMatrix = glm::mat4());

    // performs gamma correction, dithering, and fxaa
    void output(SharedTextureRectangle const& hdr, bool dithering = true, bool fxaa = true, bool gamma_correction = true);

    void copyFrom(SharedTextureRectangle const& src);

public:
    /// initializes the PP shaders if not already done
    /// (is optional)
    void init();
};
}
}
