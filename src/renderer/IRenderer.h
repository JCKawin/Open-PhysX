#pragma once

#include "raylib.h"

namespace openphysx {

class ISimulation;

// Rendering contract. Knows how to draw a simulation, not how to step it.
class IRenderer
{
public:
    virtual ~IRenderer() = default;

    virtual void init() = 0;
    virtual void shutdown() = 0;

    virtual void set_viewport_size(int width, int height) = 0;
    virtual void render(const ISimulation& simulation) = 0;
    virtual const RenderTexture2D& viewport_target() const = 0;

    virtual void update_camera(bool viewport_hovered) = 0;
    virtual void reset_camera() = 0;
    virtual Camera3D& camera() = 0;
    virtual const Camera3D& camera() const = 0;
};

} // namespace openphysx
