#pragma once

#include "renderer/IRenderer.h"

#include "raylib.h"

namespace openphysx {

class Renderer final : public IRenderer
{
public:
    Renderer() = default;
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    void init() override;
    void shutdown() override;

    void set_viewport_size(int width, int height) override;
    void render(const ISimulation& simulation) override;
    void draw_viewport_image() override;

    View3D& view() override { return view_; }
    const View3D& view() const override { return view_; }
    void reset_view() override;
    void tick_view(float dt) override;
    void cancel_view_motion() override;

    void orbit(float dx_px, float dy_px) override;
    void pan(float dx_px, float dy_px) override;
    void zoom_at(float wheel_ticks, float ndc_x, float ndc_y, float aspect) override;
    void set_axis(ViewAxis axis, bool smooth) override;
    void toggle_projection() override;
    void orbit_step(float yaw_radians, float pitch_radians, bool smooth) override;
    void frame_bounds(Vec3 center, float radius, float aspect, bool smooth) override;

private:
    void begin_smooth(const View3D& goal);
    void sync_camera();

    struct Smooth
    {
        bool active = false;
        float t = 0.0f;
        float duration = 0.18f;
        View3D from{};
        View3D to{};
    };

    RenderTexture2D target_{};
    Camera3D camera_{};
    View3D view_{};
    Smooth smooth_{};
};

} // namespace openphysx
