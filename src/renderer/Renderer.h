#pragma once

#include "renderer/IRenderer.h"

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
    const RenderTexture2D& viewport_target() const override { return target_; }

    void update_camera(bool viewport_hovered) override;
    void reset_camera() override;
    Camera3D& camera() override { return camera_; }
    const Camera3D& camera() const override { return camera_; }

private:
    void sync_camera_position();

    RenderTexture2D target_{};
    Camera3D camera_{};
    float yaw_ = 0.0f;
    float pitch_ = 0.0f;
    float distance_ = 6.0f;
};

} // namespace openphysx
