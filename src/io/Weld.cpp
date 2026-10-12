// B3.5 — Vertex welding implementation.

#include "io/Weld.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace openphysx {
namespace {

struct Key
{
    int64_t x, y, z;
    bool operator==(const Key& o) const { return x == o.x && y == o.y && z == o.z; }
};

struct KeyHash
{
    std::size_t operator()(const Key& k) const
    {
        // Classic spatial hash coefficients
        const std::size_t hx = static_cast<std::size_t>(k.x) * 73856093u;
        const std::size_t hy = static_cast<std::size_t>(k.y) * 19349663u;
        const std::size_t hz = static_cast<std::size_t>(k.z) * 83492791u;
        return hx ^ hy ^ hz;
    }
};

// Quantise a float coordinate to a grid cell index.
// Note: two positions that straddle a cell boundary may not merge even if they
// are within epsilon. Acceptable for STL because its duplicates are bit-identical.
int64_t quant(float v, float inv_eps)
{
    return static_cast<int64_t>(std::llround(static_cast<double>(v) * static_cast<double>(inv_eps)));
}

} // namespace

void weld_by_position(Mesh& m, float epsilon)
{
    if (m.indices.empty()) return;

    const float inv_eps = 1.0f / epsilon;

    // Map quantized key → new vertex index
    std::unordered_map<Key, uint32_t, KeyHash> key_to_new;
    key_to_new.reserve(m.positions.size());

    std::vector<Vec3>     new_pos;
    std::vector<uint32_t> remap(m.positions.size()); // old → new

    for (std::size_t i = 0; i < m.positions.size(); ++i)
    {
        const Vec3& p = m.positions[i];
        Key k{ quant(p.x, inv_eps), quant(p.y, inv_eps), quant(p.z, inv_eps) };
        auto [it, inserted] = key_to_new.emplace(k, static_cast<uint32_t>(new_pos.size()));
        if (inserted)
            new_pos.push_back(p);
        remap[i] = it->second;
    }

    // Rebuild index buffer, dropping degenerate triangles
    std::vector<uint32_t> new_idx;
    new_idx.reserve(m.indices.size());
    const std::size_t tri_count = m.indices.size() / 3;
    for (std::size_t t = 0; t < tri_count; ++t)
    {
        const uint32_t a = remap[m.indices[t * 3 + 0]];
        const uint32_t b = remap[m.indices[t * 3 + 1]];
        const uint32_t c = remap[m.indices[t * 3 + 2]];
        if (a == b || b == c || a == c) continue; // degenerate
        new_idx.push_back(a);
        new_idx.push_back(b);
        new_idx.push_back(c);
    }

    m.positions = std::move(new_pos);
    m.indices   = std::move(new_idx);
    m.normals.clear(); // will be recomputed by the importer
}

} // namespace openphysx
