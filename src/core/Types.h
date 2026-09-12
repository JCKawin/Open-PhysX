#pragma once

namespace openphysx {

struct Vec3
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
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
};

} // namespace openphysx
