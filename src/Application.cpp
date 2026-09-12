#include "Application.h"

#include "imgui.h"
#include "rlImGui.h"

#include "raylib.h"

namespace openphysx {
namespace {

constexpr int kWindowWidth = 1600;
constexpr int kWindowHeight = 900;
constexpr int kTargetFps = 60;
constexpr float kFixedDt = 1.0f / 60.0f;

} // namespace

int Application::run()
{
    init();

    while (!WindowShouldClose() && !workspace_.quit_requested())
        frame();

    shutdown();
    return 0;
}

void Application::init()
{
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT);
    InitWindow(kWindowWidth, kWindowHeight, "Open PhysX");
    SetTargetFPS(kTargetFps);
    SetExitKey(KEY_NULL);

    rlImGuiSetup(true);

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigWindowsMoveFromTitleBarOnly = true;
    io.ConfigDockingWithShift = false;

    renderer_.init();
    workspace_.init();
    initialized_ = true;
}

void Application::shutdown()
{
    if (!initialized_)
        return;

    renderer_.shutdown();
    rlImGuiShutdown();
    CloseWindow();
    initialized_ = false;
}

void Application::frame()
{
    const float dt = GetFrameTime();
    accumulator_ += dt;
    while (accumulator_ >= kFixedDt)
    {
        simulation_.step(kFixedDt);
        accumulator_ -= kFixedDt;
    }

    BeginDrawing();
    ClearBackground(Color{20, 20, 22, 255});

    rlImGuiBegin();
    workspace_.draw(simulation_, renderer_);
    rlImGuiEnd();

    EndDrawing();
}

} // namespace openphysx
