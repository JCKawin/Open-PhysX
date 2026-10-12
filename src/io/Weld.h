#pragma once

// B3.5 — Vertex welding (merge duplicate positions within epsilon).
// Layering rule: no raylib, no ImGui.

#include "core/Mesh.h"

namespace openphysx {

// Merge vertices whose positions are within `epsilon` (default 1e-6 m).
// Uses integer-quantized keys: two positions within epsilon of a grid cell boundary
// may not be merged — acceptable for STL because duplicates are bit-identical.
// After merging, degenerate triangles (any repeated index after remapping) are dropped.
void weld_by_position(Mesh& m, float epsilon = 1e-6f);

} // namespace openphysx
