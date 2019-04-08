#pragma once

#include <cassert>
#include <map>
#include <vector>

#include <glm/glm.hpp>

#include <glow/fwd.hh>
#include <glow/gl.hh>

namespace glow
{
/**
 * A Texture Pool stores textures of a given type for re-use
 * (a pool is related to allocation and thus uses its terminology)
 *
 * Important operations:
 *  .alloc(internalFormat, size) -> SharedTex
 *  .allocAtLeast(internalFormat, size) -> SharedTex  (might return bigger texture)
 *  .free(tex)
 *  .cleanUp(olderThan)   (should be called once per frame)
 *
 * Usage:
 *  TexturePool<Texture2D> pool;
 *  auto t1 = pool.alloc(GL_RGB16F, {1024, 1024});
 *  ... use t1
 *  pool.free(&t1)
 *  ... once per frame:
 *  pool.cleanUp()
 */
template <class TextureT>
class TexturePool
{
public:
    using SharedTextureT = std::shared_ptr<TextureT>;
    using SizeT = typename TextureT::SizeT;
    using ShapeT = typename TextureT::Shape;

private:
    struct entry
    {
        size_t gen;         ///< generation where this texture was allocated
        ShapeT shape;       ///< texture shape
        SharedTextureT tex; ///< stored texture
    };

    // members
private:
    std::map<GLenum, std::vector<entry>> mEntriesByFormat;
    size_t mCurrGen = 0;

    // public interface
public:
    /// returns an unused or new texture of a given texture shape
    SharedTextureT alloc(ShapeT shape);
    /// returns the smallest unused or new texture of a given shape and AT LEAST the given size
    SharedTextureT allocAtLeast(ShapeT shape);

    /// returns a texture to the texture pool, resets the pointer
    void free(SharedTextureT* tex);

    /// increases generation counter by 1
    /// deletes all textures older than the given number of generations
    /// should be called once per frame
    void cleanUp(int olderGenThan = 10);

    // helper
private:
    bool isSmallerOrEq(int req, int s) const { return req <= s; }
    bool isSmallerOrEq(glm::ivec2 req, glm::ivec2 s) const { return req.x <= s.x && req.y <= s.y; }
    bool isSmallerOrEq(glm::ivec3 req, glm::ivec3 s) const { return req.x <= s.x && req.y <= s.y && req.z <= s.z; }
    bool isSmallerOrEq(glm::ivec4 req, glm::ivec4 s) const
    {
        return req.x <= s.x && req.y <= s.y && req.z <= s.z && req.w <= s.w;
    }
};

/// =========== IMPLEMENTATION ===========

template <class TextureT>
typename TexturePool<TextureT>::SharedTextureT TexturePool<TextureT>::alloc(ShapeT shape)
{
    auto& entries = mEntriesByFormat[shape.format];

    for (entry& e : entries)
        if (e.shape == shape)
        {
            auto t = e.tex;
            std::swap(e, entries.back());
            entries.pop_back();

            return t;
        }

    // nothing found: alloc new
    return TextureT::createStorageImmutable(shape);
}

template <class TextureT>
typename TexturePool<TextureT>::SharedTextureT TexturePool<TextureT>::allocAtLeast(ShapeT shape)
{
    auto& entries = mEntriesByFormat[shape.format];

    auto bestI = -1;
    SizeT bestS;

    // find smaller valid tex
    // TODO: Does not respect parameters other than size
    for (auto i = 0; i < (int)entries.size(); ++i)
        if (this->isSmallerOrEq(shape.size, entries[i].shape.size) && (bestI < 0 || this->isSmallerOrEq(shape.size, bestS)))
        {
            bestI = i;
            bestS = entries[i].shape.size;
        }

    // if found: "alloc" it
    if (bestI >= 0)
    {
        auto& e = entries[bestI];
        auto t = e.tex;

        std::swap(e, entries.back());
        entries.pop_back();

        return t;
    }

    // nothing found: alloc new
    return TextureT::createStorageImmutable(shape);
}

template <class TextureT>
void TexturePool<TextureT>::free(SharedTextureT* tex)
{
    assert(tex && *tex && "cannot free nullptr");
    auto fmt = (*tex)->getInternalFormat();
    mEntriesByFormat[fmt].push_back({mCurrGen, (*tex)->getShape(), *tex});
    (*tex).reset();
}

template <class TextureT>
void TexturePool<TextureT>::cleanUp(int olderGenThan)
{
    ++mCurrGen;

    for (auto& kvp : mEntriesByFormat)
    {
        auto& entries = kvp.second;

        // delete old entries
        for (auto i = (int)entries.size() - 1; i >= 0; --i)
            if (entries[i].gen + olderGenThan < mCurrGen)
            {
                std::swap(entries[i], entries.back());
                entries.pop_back();
            }
    }
}
}
