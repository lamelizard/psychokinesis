#pragma once

#include <string>

#include <glow/fwd.hh>

/// load a mesh from a .obj file
/// vertex layout:
///     in vec3 aPosition;
///     in vec3 aNormal;
///     in vec3 aTangent;
///     in vec2 aTexCoord;
///
/// by default, computed vertex tangents are interpolated from face tangents,
/// for flat shaded objects (e.g. cube) this flag should be set to false
glow::SharedVertexArray load_mesh_from_obj(std::string const& filename, bool interpolate_tangents = true);
