// B1.6 — MeshCache implementation (raylib model upload and cleanup).

#include "renderer/MeshCache.h"

#include <cstdlib>
#include <cstring>

namespace openphysx {

void MeshCache::clear()
{
    for (auto& [id, entry] : cache_)
    {
        if (entry.model.meshCount > 0)
            UnloadModel(entry.model);
    }
    cache_.clear();
}

const ::Model* MeshCache::get_or_upload(uint32_t mesh_id, uint64_t mesh_revision, const Mesh& mesh)
{
    if (mesh_id == 0 || mesh.indices.empty())
        return nullptr;

    auto it = cache_.find(mesh_id);
    if (it != cache_.end() && it->second.revision == mesh_revision)
        return &it->second.model;

    // Need rebuild
    if (it != cache_.end() && it->second.model.meshCount > 0)
    {
        UnloadModel(it->second.model);
        cache_.erase(it);
    }

    // Convert Mesh into raylib ::Mesh
    // Handle meshes by de-indexing into flat triangles if large or for consistency:
    // 3 vertices per triangle:
    const std::size_t tri_count = mesh.indices.size() / 3;
    const std::size_t total_verts = tri_count * 3;

    ::Mesh rmesh{};
    rmesh.vertexCount = static_cast<int>(total_verts);
    rmesh.triangleCount = static_cast<int>(tri_count);

    rmesh.vertices = static_cast<float*>(std::malloc(total_verts * 3 * sizeof(float)));
    rmesh.normals  = static_cast<float*>(std::malloc(total_verts * 3 * sizeof(float)));

    const bool has_normals = (mesh.normals.size() == mesh.positions.size());

    for (std::size_t t = 0; t < tri_count; ++t)
    {
        for (int c = 0; c < 3; ++c)
        {
            const uint32_t idx = mesh.indices[t * 3 + c];
            const std::size_t out_idx = t * 3 + c;

            const Vec3& p = mesh.positions[idx];
            rmesh.vertices[out_idx * 3 + 0] = p.x;
            rmesh.vertices[out_idx * 3 + 1] = p.y;
            rmesh.vertices[out_idx * 3 + 2] = p.z;

            if (has_normals)
            {
                const Vec3& n = mesh.normals[idx];
                rmesh.normals[out_idx * 3 + 0] = n.x;
                rmesh.normals[out_idx * 3 + 1] = n.y;
                rmesh.normals[out_idx * 3 + 2] = n.z;
            }
            else
            {
                rmesh.normals[out_idx * 3 + 0] = 0.0f;
                rmesh.normals[out_idx * 3 + 1] = 1.0f;
                rmesh.normals[out_idx * 3 + 2] = 0.0f;
            }
        }
    }

    UploadMesh(&rmesh, false);
    ::Model model = LoadModelFromMesh(rmesh);

    Entry entry;
    entry.revision = mesh_revision;
    entry.model = model;

    auto [inserted_it, _] = cache_.insert_or_assign(mesh_id, entry);
    return &inserted_it->second.model;
}

} // namespace openphysx
