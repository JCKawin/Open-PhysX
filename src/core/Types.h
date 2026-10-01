#pragma once

namespace openphysx {

struct Vec3
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct Quat
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 1.0f;
};

struct Rgb
{
    float r = 0.12f;
    float g = 0.12f;
    float b = 0.14f;

    float* data() { return &r; }
    const float* data() const { return &r; }
};

struct RigidBody
{
    Vec3 position{0.0f, 1.0f, 0.0f};
    Vec3 size{2.0f, 2.0f, 2.0f};
    Quat rotation{};
    Rgb color{0.15f, 0.35f, 0.85f};
};

struct SimulationState
{
    float time = 0.0f;
    float duration = 10.0f;
    float playback_speed = 1.0f;
    bool playing = false;
    bool loop = true;
    bool show_grid = true;
    bool demo_motion = true;
    int grid_slices = 20;
    float grid_spacing = 1.0f;
    Rgb clear_color{0.12f, 0.12f, 0.14f};
    RigidBody cube{};
    bool cube_visible = true;
    bool cube_selected = true;
};

inline bool operator==(const Vec3& a, const Vec3& b)
{
    return a.x == b.x && a.y == b.y && a.z == b.z;
}

inline bool operator==(const Quat& a, const Quat& b)
{
    return a.x == b.x && a.y == b.y && a.z == b.z && a.w == b.w;
}

inline bool operator==(const Rgb& a, const Rgb& b)
{
    return a.r == b.r && a.g == b.g && a.b == b.b;
}

inline bool operator==(const RigidBody& a, const RigidBody& b)
{
    return a.position == b.position && a.size == b.size && a.rotation == b.rotation && a.color == b.color;
}

inline bool operator==(const SimulationState& a, const SimulationState& b)
{
    return a.time == b.time && a.duration == b.duration && a.playback_speed == b.playback_speed &&
           a.playing == b.playing && a.loop == b.loop && a.show_grid == b.show_grid &&
           a.demo_motion == b.demo_motion && a.grid_slices == b.grid_slices &&
           a.grid_spacing == b.grid_spacing && a.clear_color == b.clear_color && a.cube == b.cube &&
           a.cube_visible == b.cube_visible && a.cube_selected == b.cube_selected;
}

inline bool operator!=(const SimulationState& a, const SimulationState& b)
{
    return !(a == b);
}

} // namespace openphysx
