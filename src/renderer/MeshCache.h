#pragma once

// B1.6 — Mesh cache for GPU drawing (raylib types isolated here).
// Translates const Mesh& into raylib ::Model and caches keyed by (id, revision).

#include "core/Mesh.h"
#include "raylib.h"

#include <cstdint>
#include <unordered_map>

namespace openphysx {

class MeshCache
{
public:
    MeshCache() = default;
    ~MeshCache() { clear(); }

    MeshCache(const MeshCache&) = delete;
    MeshCache& operator=(const MeshCache&) = delete;

    void clear();

    // Retrieves or uploads a raylib Model for the given mesh.
    // If the cached revision differs from mesh_revision, re-uploads.
    // Returns a pointer to the Model, or nullptr if the mesh has no triangles.
    const ::Model* get_or_upload(uint32_t mesh_id, uint64_t mesh_revision, const Mesh& mesh);

private:
    struct Entry
    {
        uint64_t revision = 0;
        ::Model  model{};
    };

    std::unordered_map<uint32_t, Entry> cache_;
};

} // namespace openphysx
