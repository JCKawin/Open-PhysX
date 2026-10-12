#pragma once
#include "core/Mesh.h"
#include <filesystem>
#include <string>

namespace openphysx {

struct ObjExportOptions
{
    bool apply_transform = false; // UI label: "Apply transform"
};

// Export the mesh (from mesh space, Y-up, no axis conversion) as OBJ.
// `name` is written as the 'o' object name.
// `world` is the world-space 4x4 matrix (column-major, 16 floats); used only
//  when apply_transform is true. Pass nullptr to write mesh-space positions.
// Returns an empty string on success, or an error message on failure.
// Source must be the original mesh from Scene::find_mesh — never an evaluated copy.
std::string export_obj(const Mesh& mesh,
                       const std::string& name,
                       const float* world /*col-major Mat4, may be nullptr*/,
                       const ObjExportOptions& opts,
                       const std::filesystem::path& path);

} // namespace openphysx
