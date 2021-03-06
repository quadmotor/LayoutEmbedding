#pragma once

#include <LayoutEmbedding/Util/Assert.hh>

#include <polymesh/pm.hh>
#include <typed-geometry/tg.hh>

#include <fstream>
#include <sstream>
#include <filesystem>

namespace LayoutEmbedding {

namespace fs = std::filesystem;

enum class LandmarkFormat {
    id_x_y_z,
    id,
};

struct EmbeddingInput
{
    pm::Mesh l_m;
    pm::Mesh t_m;

    pm::vertex_attribute<tg::pos3> l_pos;
    pm::vertex_attribute<pm::vertex_handle> l_matching_vertex;

    pm::vertex_attribute<tg::pos3> t_pos;

    EmbeddingInput();
    EmbeddingInput(const EmbeddingInput& _ei);
    EmbeddingInput& operator=(const EmbeddingInput& _ei);

    // Methods
    /** \brief Saves embedding input in .inp file and optionally .obj files
     *         of layout and target mesh.
     *
     * \arg   - filename: Name of input file (including path but excluding suffix)
     *        - write_layout_mesh: If true, also writes obj file of layout_mesh under <filename>_layout.obj
     *        - write_target_input_mesh: If true, also writes obj file of target_mesh under <filename>_target_input.obj
     *
     * \details: Structure of .inp (EmbeddingInput) file
     *           # model name (as a comment)
     *
     *           # relative link to layout mesh
     *           lf  <relative/path/to/layout>
     *           # relative path to target input mesh
     *           tif <relative/path/to/target/input>
     *
     *           # matching vertices
     *           mv <layout_vertex_id_1> <target_vertex_id_1>
     *           ...
     */
    bool save(std::string filename,
              bool write_layout_mesh=true,
              bool write_target_input_mesh=true) const;

    bool load(const std::string& _path_prefix);
    bool load(
            const fs::path& _layout_path,
            const fs::path& _target_path);
    bool load(
            const fs::path& _layout_path,
            const fs::path& _target_path,
            const fs::path& _landmarks_path,
            const LandmarkFormat& _format);

    /**
     * Normalize surface area of target mesh
     */
    void normalize_surface_area();
    void center_translation();

    /// Reverse face orientation of the layout.
    /// Warning:
    /// - Loses all attributes stored on edges, halfedges, and faces.
    /// - May change the indices of edges and halfedges.
    void invert_layout();
};

std::vector<int> load_landmarks(const fs::path& _file_path, const LandmarkFormat& _format);

}
