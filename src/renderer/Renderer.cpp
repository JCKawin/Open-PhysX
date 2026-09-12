#include "renderer/Renderer.h"

#include "logic/ISimulation.h"

#include <algorithm>
#include <cmath>

namespace openphysx {
namespace {

Color to_color(const Rgb& rgb)
{
    return Color{
        static_cast<unsigned char>(std::clamp(rgb.r, 0.0f, 1.0f) * 255.0f),
        static_cast<unsigned char>(std::clamp(rgb.g, 0.0f, 1.0f) * 255.0f),
        static_cast<unsigned char>(std::clamp(rgb.b, 0.0f, 1.0f) * 255.0f),
        255,
    };
}

Vector3 to_vec3(const Vec3& v)
{
    return Vector3{v.x, v.y, v.z};
}

Color wire_color(const Rgb& rgb)
{
    return Color{
        static_cast<unsigned char>(std::clamp(rgb.r * 0.55f + 0.45f, 0.0f, 1.0f) * 255.0f),
        static_cast<unsigned char>(std::clamp(rgb.g * 0.55f + 0.45f, 0.0f, 1.0f) * 255.0f),
        static_cast<unsigned char>(std::clamp(rgb.b * 0.55f + 0.45f, 0.0f, 1.0f) * 255.0f),
        255,
    };
}

constexpr float kPi = 3.14159265358979323846f;

Vector3 vec3_sub(Vector3 a, Vector3 b)
{
    return Vector3{a.x - b.x, a.y - b.y, a.z - b.z};
}

Vector3 vec3_add(Vector3 a, Vector3 b)
{
    return Vector3{a.x + b.x, a.y + b.y, a.z + b.z};
}

Vector3 vec3_scale(Vector3 v, float s)
{
    return Vector3{v.x * s, v.y * s, v.z * s};
}

Vector3 vec3_cross(Vector3 a, Vector3 b)
{
    return Vector3{
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x,
    };
}

Vector3 vec3_normalize(Vector3 v)
{
    const float length = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    if (length <= 1e-8f)
        return Vector3{0.0f, 0.0f, 0.0f};
    return vec3_scale(v, 1.0f / length);
}

} // namespace

void Renderer::init()
{
    target_ = LoadRenderTexture(16, 16);
    reset_camera();
}

void Renderer::shutdown()
{
    if (target_.id != 0)
    {
        UnloadRenderTexture(target_);
        target_ = {};
    }
}

void Renderer::set_viewport_size(int width, int height)
{
    width = std::max(width, 1);
    height = std::max(height, 1);

    if (target_.id != 0 && target_.texture.width == width && target_.texture.height == height)
        return;

    if (target_.id != 0)
        UnloadRenderTexture(target_);

    target_ = LoadRenderTexture(width, height);
}

void Renderer::reset_camera()
{
    camera_.target = {0.0f, 1.0f, 0.0f};
    camera_.up = {0.0f, 1.0f, 0.0f};
    camera_.fovy = 45.0f;
    camera_.projection = CAMERA_PERSPECTIVE;
    distance_ = std::sqrt(41.0f);
    yaw_ = kPi / 4.0f;
    pitch_ = std::asin(3.0f / distance_);
    sync_camera_position();
}

void Renderer::sync_camera_position()
{
    const float cp = std::cos(pitch_);
    camera_.position = {
        camera_.target.x + distance_ * cp * std::sin(yaw_),
        camera_.target.y + distance_ * std::sin(pitch_),
        camera_.target.z + distance_ * cp * std::cos(yaw_),
    };
}

void Renderer::update_camera(bool viewport_hovered)
{
    if (!viewport_hovered)
        return;

    const Vector2 mouse_delta = GetMouseDelta();
    const float wheel = GetMouseWheelMove();
    const bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    const bool rotating =
        !shift && (IsMouseButtonDown(MOUSE_BUTTON_RIGHT) || IsMouseButtonDown(MOUSE_BUTTON_MIDDLE));
    const bool panning =
        shift && (IsMouseButtonDown(MOUSE_BUTTON_RIGHT) || IsMouseButtonDown(MOUSE_BUTTON_MIDDLE));

    if (rotating)
    {
        yaw_ -= mouse_delta.x * 0.005f;
        pitch_ += mouse_delta.y * 0.005f;
        pitch_ = std::clamp(pitch_, -1.45f, 1.45f);
    }

    if (panning)
    {
        const float pan_speed = distance_ * 0.0015f;
        const Vector3 forward = vec3_normalize(vec3_sub(camera_.target, camera_.position));
        const Vector3 right = vec3_normalize(vec3_cross(forward, camera_.up));
        const Vector3 up = vec3_cross(right, forward);
        camera_.target = vec3_add(camera_.target, vec3_scale(right, -mouse_delta.x * pan_speed));
        camera_.target = vec3_add(camera_.target, vec3_scale(up, mouse_delta.y * pan_speed));
    }

    if (wheel != 0.0f)
    {
        distance_ *= (1.0f - wheel * 0.1f);
        distance_ = std::clamp(distance_, 1.0f, 80.0f);
    }

    sync_camera_position();
}

void Renderer::render(const ISimulation& simulation)
{
    if (target_.id == 0)
        return;

    const SimulationState& state = simulation.state();
    const RigidBody body = simulation.visual_body();
    const Vector3 position = to_vec3(body.position);
    const Vector3 size = to_vec3(body.size);

    BeginTextureMode(target_);
    ClearBackground(to_color(state.clear_color));
    BeginMode3D(camera_);

    if (state.show_grid)
        DrawGrid(state.grid_slices, state.grid_spacing);

    DrawCube(position, size.x, size.y, size.z, to_color(body.color));
    DrawCubeWires(position, size.x, size.y, size.z, wire_color(body.color));

    EndMode3D();
    EndTextureMode();
}

} // namespace openphysx
