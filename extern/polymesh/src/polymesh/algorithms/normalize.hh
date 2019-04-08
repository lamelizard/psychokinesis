#pragma once

#include "../Mesh.hh"
#include "../fields.hh"

namespace polymesh
{
// Applies a translation and a uniform rescaling such that the mesh is centerd at (0,0,0) and withing the [-1 .. 1] cube
template <class Pos3>
void normalize(vertex_attribute<Pos3>& pos)
{
    auto mm = pos.minmax();
    auto mi = mm.min;
    auto ma = mm.max;

    auto cx = (mi[0] + ma[0]) * 0.5f;
    auto cy = (mi[1] + ma[1]) * 0.5f;
    auto cz = (mi[2] + ma[2]) * 0.5f;

    auto sx = ma[0] - mi[0];
    auto sy = ma[1] - mi[1];
    auto sz = ma[2] - mi[2];
    auto s = std::max(sx, std::max(sy, sz)) * 0.5f;
    s = std::max(s, std::numeric_limits<float>::min());

    for (auto& p : pos)
    {
        p[0] = (p[0] - cx) / s;
        p[1] = (p[1] - cy) / s;
        p[2] = (p[2] - cz) / s;
    }
}
} // namespace polymesh
