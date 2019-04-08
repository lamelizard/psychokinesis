#pragma once

#include <vector>

// include glm types
#include <glm/matrix.hpp>

#include <glow/common/macro_join.hh>
#include <glow/common/non_copyable.hh>
#include <glow/fwd.hh>
#include <glow/gl.hh>

namespace glow
{
GLOW_SHARED(class, UniformState);

/// RAII-object that defines a "use"-scope for a Program
/// All functions that operate on the currently bound program are accessed here
struct UsedProgram
{
    GLOW_RAII_CLASS(UsedProgram);

    /// Backreference to the program
    Program* const program;

public: // gl functions with use
    /// Binds a texture to a uniform
    /// Automatically chooses a free texture unit starting from 0
    /// Setting nullptr is ok
    /// Optionally specifies a sampler for this texture
    void setTexture(std::string const& name, SharedTexture const& tex, SharedSampler const& sampler = nullptr);
    /// Binds a texture to an image sampler
    /// Requires an explicit binding location in shader (e.g. binding=N layout)
    /// Requires tex->isStorageImmutable()
    void setImage(int bindingLocation, SharedTexture const& tex, GLenum usage = GL_READ_WRITE, int mipmapLevel = 0, int layer = 0);

    /// ========================================= UNIFORMS - START =========================================
    /// This section defines the various ways of setting uniforms
    /// Setting by uniform _location_ is explicitly NOT supported because drawing may relink and thus reshuffle
    /// locations. However, locations are internally cached, so the performance hit is not high.
    ///
    /// Supported variants:
    ///   setUniform(string name, T value)
    ///   setUniform(string name, int count, T* values)
    ///   setUniform(string name, vector<T> values)
    ///   setUniform(string name, T values[N])
    ///   setUniform(string name, std::array<T, N> values)
    ///   setUniform(string name, {v0, v1, v2, ...})
    ///
    /// for T in
    ///   bool
    ///   int
    ///   float
    ///   unsigned
    ///   [ iub]vec[234]
    ///   mat[234]
    ///   mat[234]x[234]
    ///
    /// NOTE: there are no double uniforms!
    ///
    /// Using the wrong type results in a runtime error.
    /// TODO: make this configurable

    /// Generic interface
    void setUniform(std::string const& name, int count, bool const* values) const;
    void setUniform(std::string const& name, int count, int32_t const* values) const;
    void setUniform(std::string const& name, int count, uint32_t const* values) const;
    void setUniform(std::string const& name, int count, float const* values) const;

    void setUniform(std::string const& name, int count, glm::vec2 const* values) const;
    void setUniform(std::string const& name, int count, glm::vec3 const* values) const;
    void setUniform(std::string const& name, int count, glm::vec4 const* values) const;
    void setUniform(std::string const& name, int count, glm::ivec2 const* values) const;
    void setUniform(std::string const& name, int count, glm::ivec3 const* values) const;
    void setUniform(std::string const& name, int count, glm::ivec4 const* values) const;
    void setUniform(std::string const& name, int count, glm::uvec2 const* values) const;
    void setUniform(std::string const& name, int count, glm::uvec3 const* values) const;
    void setUniform(std::string const& name, int count, glm::uvec4 const* values) const;
    void setUniform(std::string const& name, int count, glm::bvec2 const* values) const;
    void setUniform(std::string const& name, int count, glm::bvec3 const* values) const;
    void setUniform(std::string const& name, int count, glm::bvec4 const* values) const;

    void setUniform(std::string const& name, int count, glm::mat2x2 const* values) const;
    void setUniform(std::string const& name, int count, glm::mat2x3 const* values) const;
    void setUniform(std::string const& name, int count, glm::mat2x4 const* values) const;
    void setUniform(std::string const& name, int count, glm::mat3x2 const* values) const;
    void setUniform(std::string const& name, int count, glm::mat3x3 const* values) const;
    void setUniform(std::string const& name, int count, glm::mat3x4 const* values) const;
    void setUniform(std::string const& name, int count, glm::mat4x2 const* values) const;
    void setUniform(std::string const& name, int count, glm::mat4x3 const* values) const;
    void setUniform(std::string const& name, int count, glm::mat4x4 const* values) const;

/// Convenience interfaces
#define GLOW_PROGRAM_UNIFORM_API_NO_VEC(TYPE)                                                   \
    void setUniform(std::string const& name, TYPE value) const { setUniform(name, 1, &value); } \
    template <std::size_t N>                                                                    \
    void setUniform(std::string const& name, const TYPE(&data)[N])                              \
    {                                                                                           \
        setUniform(name, (int)N, data);                                                         \
    }                                                                                           \
    template <std::size_t N>                                                                    \
    void setUniform(std::string const& name, std::array<TYPE, N> const& data)                   \
    {                                                                                           \
        setUniform(name, (int)N, data.data());                                                  \
    }                                                                                           \
    friend class GLOW_MACRO_JOIN(___prog_uniform_api_no_vec_, __COUNTER__) // enfore ;
#define GLOW_PROGRAM_UNIFORM_API(TYPE)                                                                                                       \
    GLOW_PROGRAM_UNIFORM_API_NO_VEC(TYPE);                                                                                                   \
    void setUniform(std::string const& name, std::vector<TYPE> const& values) const { setUniform(name, (int)values.size(), values.data()); } \
    friend class GLOW_MACRO_JOIN(___prog_uniform_api_, __COUNTER__) // enfore ;

    // basis types
    GLOW_PROGRAM_UNIFORM_API_NO_VEC(bool);
    GLOW_PROGRAM_UNIFORM_API(int32_t);
    GLOW_PROGRAM_UNIFORM_API(uint32_t);
    GLOW_PROGRAM_UNIFORM_API(float);

    // vector types
    GLOW_PROGRAM_UNIFORM_API(glm::vec2);
    GLOW_PROGRAM_UNIFORM_API(glm::vec3);
    GLOW_PROGRAM_UNIFORM_API(glm::vec4);
    GLOW_PROGRAM_UNIFORM_API(glm::ivec2);
    GLOW_PROGRAM_UNIFORM_API(glm::ivec3);
    GLOW_PROGRAM_UNIFORM_API(glm::ivec4);
    GLOW_PROGRAM_UNIFORM_API(glm::uvec2);
    GLOW_PROGRAM_UNIFORM_API(glm::uvec3);
    GLOW_PROGRAM_UNIFORM_API(glm::uvec4);
    GLOW_PROGRAM_UNIFORM_API(glm::bvec2);
    GLOW_PROGRAM_UNIFORM_API(glm::bvec3);
    GLOW_PROGRAM_UNIFORM_API(glm::bvec4);

    // matrix types
    GLOW_PROGRAM_UNIFORM_API(glm::mat2x2);
    GLOW_PROGRAM_UNIFORM_API(glm::mat2x3);
    GLOW_PROGRAM_UNIFORM_API(glm::mat2x4);
    GLOW_PROGRAM_UNIFORM_API(glm::mat3x2);
    GLOW_PROGRAM_UNIFORM_API(glm::mat3x3);
    GLOW_PROGRAM_UNIFORM_API(glm::mat3x4);
    GLOW_PROGRAM_UNIFORM_API(glm::mat4x2);
    GLOW_PROGRAM_UNIFORM_API(glm::mat4x3);
    GLOW_PROGRAM_UNIFORM_API(glm::mat4x4);

#undef GLOW_PROGRAM_UNIFORM_API
#undef GLOW_PROGRAM_UNIFORM_API_NO_VEC

    /// Special case: vector<bool>
    void setUniform(std::string const& name, std::vector<bool> const& values) const
    {
        auto cnt = values.size();
        std::vector<int32_t> tmp(cnt);
        for (auto i = 0u; i < cnt; ++i)
            tmp[i] = (int)values[i];
        setUniformBool(name, (int)tmp.size(), tmp.data());
    }

    /// Applies all saved uniforms from that state
    void setUniforms(SharedUniformState const& state);
    /// Sets generic uniform data
    /// CAUTION: bool is assumed as glUniformi (i.e. integer)
    void setUniform(std::string const& name, GLenum uniformType, GLint size, void const* data);
    /// ========================================== UNIFORMS - END ==========================================


    /// Invokes the compute shader with the given number of groups
    void compute(GLuint groupsX = 1, GLuint groupsY = 1, GLuint groupsZ = 1);
    void compute(glm::uvec2 const& groups) { compute(groups.x, groups.y); }
    void compute(glm::uvec3 const& groups) { compute(groups.x, groups.y, groups.z); }

    /// Starts transform feedback
    /// feedbackBuffer is an optional buffer that is automatically bound to GL_TRANSFORM_FEEDBACK_BUFFER
    /// NOTE: it is recommended to use a FeedbackObject
    void beginTransformFeedback(GLenum primitiveMode, SharedBuffer const& feedbackBuffer = nullptr);
    /// Ends transform feedback
    void endTransformFeedback();

    /// Check for shader reloading
    void checkShaderReload();

private:
    /// Special case: bool uniforms
    void setUniformBool(std::string const& name, int count, int32_t const* values) const;

    /// Special case: overwrite uniform type
    void setUniformIntInternal(std::string const& name, int count, int32_t const* values, GLenum uniformType) const;

private:
    GLint previousProgram;           ///< previously used program
    UsedProgram* previousProgramPtr; ///< previously used program
    UsedProgram(Program* program);
    friend class Program;

    /// returns true iff it's safe to use this bound class
    /// otherwise, runtime error
    bool isCurrent() const;

public:
    UsedProgram(UsedProgram&&); // allow move
    ~UsedProgram();
};
}
