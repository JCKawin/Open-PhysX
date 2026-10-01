#pragma once

#include "core/View.h"

namespace openphysx {

class ISimulation;

// Drawing contract. The viewport image is consumed inside draw_viewport_image()
// so this header does not name a graphics API.
class IRenderer
{
public:
    virtual ~IRenderer() = default;

    virtual void init() = 0;
    virtual void shutdown() = 0;

    virtual void set_viewport_size(int width, int height) = 0;
    virtual void render(const ISimulation& simulation) = 0;
    virtual void draw_viewport_image() = 0;

    virtual View3D& view() = 0;
    virtual const View3D& view() const = 0;
    virtual void reset_view() = 0;
    virtual void tick_view(float dt) = 0;
    virtual void cancel_view_motion() = 0;

    virtual void orbit(float dx_px, float dy_px) = 0;
    virtual void pan(float dx_px, float dy_px) = 0;
    virtual void zoom_at(float wheel_ticks, float ndc_x, float ndc_y, float aspect) = 0;
    virtual void set_axis(ViewAxis axis, bool smooth) = 0;
    virtual void toggle_projection() = 0;
    virtual void orbit_step(float yaw_radians, float pitch_radians, bool smooth) = 0;
    virtual void frame_bounds(Vec3 center, float radius, float aspect, bool smooth) = 0;
};

} // namespace openphysx
