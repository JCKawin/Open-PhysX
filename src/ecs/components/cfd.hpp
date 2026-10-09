#pragma once

#include "core/Types.h"

#include <vector>

namespace openphysx {

enum class TurbulenceModel
{
    None,
    Laminar,
    KEpsilon,
};

enum class BoundaryKind
{
    Wall,
    Inlet,
    Outlet,
    Symmetry,
};

struct BoundaryCondition
{
    BoundaryKind kind = BoundaryKind::Wall;
    Vec3 velocity{};
    float pressure = 0.0f;
};

// Saved domain description. Result fields are CfdResultField and are never saved.
struct CfdDomainComponent
{
    Vec3 boundsMin{-1.0f, -1.0f, -1.0f};
    Vec3 boundsMax{1.0f, 1.0f, 1.0f};
    int resolutionX = 32;
    int resolutionY = 32;
    int resolutionZ = 32;
    float density = 1.2f;
    float viscosity = 1.8e-5f;
    TurbulenceModel turbulence = TurbulenceModel::Laminar;
    std::vector<BoundaryCondition> boundaries;
};

inline bool operator==(const BoundaryCondition& a, const BoundaryCondition& b)
{
    return a.kind == b.kind && a.velocity == b.velocity && a.pressure == b.pressure;
}

inline bool operator==(const CfdDomainComponent& a, const CfdDomainComponent& b)
{
    return a.boundsMin == b.boundsMin && a.boundsMax == b.boundsMax && a.resolutionX == b.resolutionX &&
           a.resolutionY == b.resolutionY && a.resolutionZ == b.resolutionZ && a.density == b.density &&
           a.viscosity == b.viscosity && a.turbulence == b.turbulence && a.boundaries == b.boundaries;
}

} // namespace openphysx
