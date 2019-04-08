#pragma once

#include <iostream>
#include <string>

#include "../Mesh.hh"

namespace polymesh
{
template <class ScalarT>
void write_obj(std::string const& filename, Mesh const& mesh, vertex_attribute<std::array<ScalarT, 3>> const& position);
template <class ScalarT>
bool read_obj(std::string const& filename, Mesh& mesh, vertex_attribute<std::array<ScalarT, 3>>& position);

template <class ScalarT>
struct obj_writer
{
    obj_writer(std::string const& filename);
    obj_writer(std::ostream& out);
    ~obj_writer();

    void write_object_name(std::string object_name);
    void write_mesh(Mesh const& mesh,
                    vertex_attribute<std::array<ScalarT, 4>> const& position,
                    halfedge_attribute<std::array<ScalarT, 3>> const* tex_coord = nullptr,
                    halfedge_attribute<std::array<ScalarT, 3>> const* normal = nullptr);
    void write_mesh(Mesh const& mesh,
                    vertex_attribute<std::array<ScalarT, 3>> const& position,
                    vertex_attribute<std::array<ScalarT, 2>> const* tex_coord = nullptr,
                    vertex_attribute<std::array<ScalarT, 3>> const* normal = nullptr);

    // TODO: tex coords and normals as half-edge attributes

private:
    std::ostream* tmp_out = nullptr;
    std::ostream* out = nullptr;

    int vertex_idx = 1;
    int texture_idx = 1;
    int normal_idx = 1;
};

// clears the given mesh before adding data
// obj must be manifold
// no negative indices
template <class ScalarT>
struct obj_reader
{
    obj_reader(std::string const& filename, Mesh& mesh);
    obj_reader(std::istream& in, Mesh& mesh);

    // get properties of the obj
public:
    vertex_attribute<std::array<ScalarT, 4>> const& get_positions() const { return positions; }
    halfedge_attribute<std::array<ScalarT, 3>> const& get_tex_coords() const { return tex_coords; }
    halfedge_attribute<std::array<ScalarT, 3>> const& get_normals() const { return normals; }

    /// Number of faces that could not be added
    int error_faces() const { return n_error_faces; }

    /// Whether or not this mesh has texture coordinates
    bool has_valid_texcoords() const { return has_texcoords; }

    /// Whether or not this mesh has vertex normals
    bool has_valid_normals() const { return has_normals; }

private:
    void parse(std::istream& in, Mesh& mesh);

    vertex_attribute<std::array<ScalarT, 4>> positions;
    halfedge_attribute<std::array<ScalarT, 3>> tex_coords;
    halfedge_attribute<std::array<ScalarT, 3>> normals;

    int n_error_faces = 0;
    bool has_texcoords = false;
    bool has_normals = false;
};
} // namespace polymesh
