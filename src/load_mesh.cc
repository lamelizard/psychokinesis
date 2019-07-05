#include "load_mesh.hh"

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <fstream>

#include <glow/common/log.hh>
#include <glow/objects/ArrayBuffer.hh>
#include <glow/objects/ElementArrayBuffer.hh>
#include <glow/objects/VertexArray.hh>

#include <polymesh/Mesh.hh>
#include <polymesh/algorithms/properties.hh>
#include <polymesh/fields.hh>
#include <polymesh/formats/obj.hh>

glow::SharedVertexArray load_mesh_from_obj(const std::string &filename, bool interpolate_tangents) {
  if (!std::ifstream(filename).good()) {
    glow::error() << filename << " cannot be opened";
    return nullptr;
  }

  pm::Mesh m;
  pm::obj_reader<float> obj_reader(filename, m);
  if (obj_reader.error_faces() > 0)
    glow::warning() << filename << " contains " << obj_reader.error_faces() << " faces that could not be added";

  auto normals = obj_reader.get_normals().map([](std::array<float, 3> const &t) { return glm::vec3(t[0], t[1], t[2]); });
  auto texcoord = obj_reader.get_tex_coords().map([](std::array<float, 3> const &t) { return glm::vec2(t[0], t[1]); });
  auto pos = obj_reader.get_positions().map([](std::array<float, 4> const &t) { return glm::vec3(t[0], t[1], t[2]); });
  auto tangents = m.halfedges().make_attribute_with_default(glm::vec3(0, 0, 0));
  auto interpolated_tangents = m.vertices().make_attribute_with_default(glm::vec3(0, 0, 0));

  auto mesh_has_texcoords = obj_reader.has_valid_texcoords();
  auto mesh_has_normals = obj_reader.has_valid_normals();

  if (!mesh_has_normals) {
    // Compute normals
    auto vnormals = pm::vertex_normals_by_area(m, pos);
    for (auto h : m.halfedges())
      normals[h] = vnormals[h.vertex_to()];
  }

  if (!mesh_has_texcoords) {
    glow::warning() << "Mesh " << filename << " does not have texture coordinates. Cannot compute tangents.";
    for (auto &t : tangents) {
      auto one_over_sqrt3 = 0.57735f;
      t = {one_over_sqrt3, one_over_sqrt3, one_over_sqrt3};
    }
  } else {
    // creation of tangents for triangle meshes
    auto faceTangents = m.faces().make_attribute<glm::vec3>();
    for (auto f : m.faces()) {
      glm::vec3 p[3];
      glm::vec2 t[3];
      int cnt = 0;
      for (auto h : f.halfedges()) {
        auto v = h.vertex_to();
        p[cnt] = pos[v];
        t[cnt] = texcoord[h];
        ++cnt;

        if (cnt > 2)
          break;
      }

      auto p10 = p[1] - p[0];
      auto p20 = p[2] - p[0];

      auto t10 = t[1] - t[0];
      auto t20 = t[2] - t[0];

      auto u10 = t10.x;
      auto u20 = t20.x;
      auto v10 = t10.y;
      auto v20 = t20.y;

      // necessary?
      float dir = (u20 * v10 - u10 * v20) < 0 ? -1 : 1;

      // sanity check
      if (u20 * v10 == u10 * v20) {
        // glow::warning() << "Warning: Creating bad tangent";
        u20 = 1;
        u10 = 0;
        v10 = 1;
        v20 = 0;
      }
      auto tangent = dir * (p20 * v10 - p10 * v20);
      faceTangents[f] = tangent;
    }


    // compute halfedge tangents / interpolated vertex tangents
    for (auto f : m.faces()) {
      for (auto h : f.halfedges()) {
        auto halfedge_tangent = faceTangents[f] - normals[h] * pm::field3<glm::vec3>::dot(faceTangents[f], normals[h]);
        tangents[h] += halfedge_tangent;

        if (interpolate_tangents) {
          auto v = h.vertex_to();
          interpolated_tangents[v] += halfedge_tangent;
        }
      }
    }
  }


  std::vector<glm::vec3> aPos;
  std::vector<glm::vec3> aNormal;
  std::vector<glm::vec3> aTangent;
  std::vector<glm::vec2> aTexCoord;

  for (auto f : m.faces())
    for (auto h : f.halfedges()) {
      auto v = h.vertex_to();
      aPos.push_back(pos[v]);
      aNormal.push_back(normals[h]);

      auto t = interpolate_tangents ? interpolated_tangents[v] : tangents[h];
      auto l = pm::field3<glm::vec3>::length(t);
      if (l > 0)
        t /= l;

      aTangent.push_back(t);
      aTexCoord.push_back(texcoord[h]);
    }

  auto abPos = glow::ArrayBuffer::create("aPosition", aPos);
  auto abNormal = glow::ArrayBuffer::create("aNormal", aNormal);
  auto abTangent = glow::ArrayBuffer::create("aTangent", aTangent);
  auto abTexCoord = glow::ArrayBuffer::create("aTexCoord", aTexCoord);

  // add index buffer if not a triangle mesh
  glow::SharedElementArrayBuffer eab;
  if (!is_triangle_mesh(m)) {
    std::vector<int> indices;
    indices.reserve(m.faces().size() * 3);

    auto v_base = 0;
    for (auto f : m.faces()) {
      auto idx = 0;
      for (auto h : f.halfedges()) {
        (void)h; // unused

        if (idx >= 2) {
          indices.push_back(v_base + 0);
          indices.push_back(v_base + idx - 1);
          indices.push_back(v_base + idx - 0);
        }

        ++idx;
      }

      v_base += idx;
    }
    eab = glow::ElementArrayBuffer::create(indices);
  }

  return glow::VertexArray::create({abPos, abNormal, abTangent, abTexCoord}, eab, GL_TRIANGLES);
}
